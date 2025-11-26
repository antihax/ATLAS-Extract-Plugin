#pragma once // This program is licensed under the GNU GPL v3.0, except that commercial use is prohibited.
#include <string>
#include <deque>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <map>
#include "json.hpp"

#define CURL_STATICLIB 
#include "curl/curl.h"
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libcurl.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "iphlpapi.lib")

#include "Logger/Logger.h"
#include "Config.h"

class DiscordWebhookManager {
public:
	static constexpr size_t CONTENT_LIMIT = 2000;

	struct WebhookMessage {
		std::string url;
		nlohmann::json payload;
		int attempts = 0;
	};

	struct RateLimitInfo {
		int limit = 0;                    // X-RateLimit-Limit
		int remaining = 0;                // X-RateLimit-Remaining
		double reset = 0.0;               // X-RateLimit-Reset (Unix timestamp)
		double resetAfter = 0.0;          // X-RateLimit-Reset-After (seconds)
		std::string bucket;               // X-RateLimit-Bucket
		std::string scope;                // X-RateLimit-Scope
		bool isGlobal = false;            // global field from response body
		std::chrono::steady_clock::time_point blockedUntil;
	};

	DiscordWebhookManager(int maxRetries = 5, int retryDelaySeconds = 10)
		: maxRetries(maxRetries), retryDelaySeconds(retryDelaySeconds) {
		running = true;
		worker = std::thread([this]() { WorkerLoop(); });
	}

	~DiscordWebhookManager() {
		running = false;
		cv.notify_all();
		if (worker.joinable()) worker.join();
	}

	// Sanitize message to prevent Discord @ mentions
	static std::string SanitizeForDiscord(const std::string& text) {
		std::string result = text;
		size_t pos = 0;
		while ((pos = result.find('@', pos)) != std::string::npos) {
			result.replace(pos, 1, "@ ");
			pos += 2;
		}
		return result;
	}

	// Send a single webhook message (will retry on failure)
	void SendWebhook(const std::string& url, const nlohmann::json& payload) {
		// Discord standard: content field max 2000 chars
		if (payload.contains("content") && payload["content"].is_string() && payload["content"].get<std::string>().size() > CONTENT_LIMIT) {
			Log::GetLog()->error("[DiscordWebhook] Message too long (max {} chars)", CONTENT_LIMIT);
			return;
		}
		{
			std::lock_guard<std::mutex> lock(queueMutex);
			queue.push(WebhookMessage{ url, payload, 0 });
		}
		cv.notify_one();
	}

	// Send batched messages with intelligent splitting
	// Messages are formatted using the provided formatter lambda
	template<typename Message, typename Formatter>
	void SendBatch(
		const std::string& webhookUrl,
		const std::string& username,
		std::deque<Message>& messages,
		Formatter lineFormatter)
	{
		if (messages.empty()) {
			return;
		}

		std::string batchedContent;
		for (const auto& msg : messages) {
			std::string line = lineFormatter(msg);

			// Handle individual messages that exceed the limit
			if (line.length() > CONTENT_LIMIT) {
				// Send current batch first if it has content
				if (!batchedContent.empty()) {
					SendWebhook(webhookUrl, {
						{"content", batchedContent},
						{"username", username}
						});
					batchedContent.clear();
				}

				// Truncate the oversized message and send it
				std::string truncated = line.substr(0, CONTENT_LIMIT - 20) + " [truncated...]";
				SendWebhook(webhookUrl, {
					{"content", truncated},
					{"username", username}
					});
				continue;
			}

			size_t lineLength = line.length();
			size_t newlineLength = batchedContent.empty() ? 0 : 1;

			if (!batchedContent.empty() && (batchedContent.length() + newlineLength + lineLength) > CONTENT_LIMIT) {
				// Send current batch before adding this message
				SendWebhook(webhookUrl, {
					{"content", batchedContent},
					{"username", username}
					});
				batchedContent = line;
			}
			else {
				if (!batchedContent.empty()) {
					batchedContent += "\n";
				}
				batchedContent += line;
			}
		}

		// Send any remaining content
		if (!batchedContent.empty()) {
			SendWebhook(webhookUrl, {
				{"content", batchedContent},
				{"username", username}
				});
		}

		messages.clear();
	}

private:
	// CURL header callback to capture response headers
	static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
		size_t totalSize = size * nitems;
		std::string header(buffer, totalSize);
		auto* rateLimitInfo = static_cast<RateLimitInfo*>(userdata);

		// Parse rate limit headers
		if (header.find("X-RateLimit-Limit:") == 0) {
			rateLimitInfo->limit = std::stoi(header.substr(19));
		}
		else if (header.find("X-RateLimit-Remaining:") == 0) {
			rateLimitInfo->remaining = std::stoi(header.substr(23));
		}
		else if (header.find("X-RateLimit-Reset:") == 0) {
			rateLimitInfo->reset = std::stod(header.substr(19));
		}
		else if (header.find("X-RateLimit-Reset-After:") == 0) {
			rateLimitInfo->resetAfter = std::stod(header.substr(25));
		}
		else if (header.find("X-RateLimit-Bucket:") == 0) {
			rateLimitInfo->bucket = header.substr(20);
			// Trim whitespace and newlines
			rateLimitInfo->bucket.erase(rateLimitInfo->bucket.find_last_not_of(" \r\n\t") + 1);
		}
		else if (header.find("X-RateLimit-Scope:") == 0) {
			rateLimitInfo->scope = header.substr(19);
			rateLimitInfo->scope.erase(rateLimitInfo->scope.find_last_not_of(" \r\n\t") + 1);
		}

		return totalSize;
	}

	// CURL write callback to capture response body
	static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userdata) {
		size_t totalSize = size * nmemb;
		auto* responseBody = static_cast<std::string*>(userdata);
		responseBody->append(static_cast<char*>(contents), totalSize);
		return totalSize;
	}

	void WorkerLoop() {
		while (running) {
			std::unique_lock<std::mutex> lock(queueMutex);
			cv.wait(lock, [this] { return !queue.empty() || !running; });
			if (!running) break;

			auto msg = queue.front();
			queue.pop();
			lock.unlock();

			// Check for GLOBAL rate limit first (affects all webhooks)
			{
				std::lock_guard<std::mutex> rateLock(rateLimitMutex);
				auto now = std::chrono::steady_clock::now();
				if (now < globalRateLimitUntil) {
					auto waitTime = std::chrono::duration_cast<std::chrono::seconds>(
						globalRateLimitUntil - now).count();
					Log::GetLog()->error("[DiscordWebhook] GLOBAL rate limit active, waiting {} seconds", waitTime);

					// Requeue the message
					std::lock_guard<std::mutex> relock(queueMutex);
					queue.push(msg);
					cv.notify_one();

					// Sleep outside the lock (cap at 60 seconds)
					lock.unlock();
					long long sleepTime = waitTime < 60 ? waitTime : 60;
					std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
					continue;
				}
			}

			// Check if we're rate limited for this specific URL
			{
				std::lock_guard<std::mutex> rateLock(rateLimitMutex);
				auto it = rateLimits.find(msg.url);
				if (it != rateLimits.end()) {
					auto now = std::chrono::steady_clock::now();
					if (now < it->second.blockedUntil) {
						auto waitTime = std::chrono::duration_cast<std::chrono::seconds>(
							it->second.blockedUntil - now).count();
						Log::GetLog()->warn("[DiscordWebhook] Rate limited (bucket: {}), waiting {} seconds",
							it->second.bucket.empty() ? "unknown" : it->second.bucket, waitTime);

						// Requeue the message
						std::lock_guard<std::mutex> relock(queueMutex);
						queue.push(msg);
						cv.notify_one();

						// Sleep outside the lock (cap at 60 seconds)
						lock.unlock();
						long long sleepTime = waitTime < 60 ? waitTime : 60;
						std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
						continue;
					}
				}
			}

			bool success = PostWebhook(msg.url, msg.payload);
			if (!success && msg.attempts < maxRetries) {
				msg.attempts++;
				std::this_thread::sleep_for(std::chrono::seconds(retryDelaySeconds));
				std::lock_guard<std::mutex> relock(queueMutex);
				queue.push(msg);
				cv.notify_one();
			}
			else if (!success) {
				Log::GetLog()->error("[DiscordWebhook] Failed after {} attempts: {}", maxRetries, msg.url);
			}
		}
	}

	bool PostWebhook(const std::string& url, const nlohmann::json& payload) {
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		RateLimitInfo rateLimitInfo;
		std::string responseBody;

		struct curl_slist* headers = nullptr;
		headers = curl_slist_append(headers, "Content-Type: application/json");

		std::string data = payload.dump();

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &rateLimitInfo);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

		CURLcode res = curl_easy_perform(curl);
		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);

		if (res == CURLE_OK && http_code >= 200 && http_code < 300) {
			// Update rate limit info from successful response
			UpdateRateLimitInfo(url, rateLimitInfo);

			// Only warn if we're actually approaching the limit AND there's meaningful time until reset
			// If resetAfter is 0, the window just reset and we're fine
			if (rateLimitInfo.remaining == 0 && rateLimitInfo.resetAfter > 0) {
				Log::GetLog()->warn("[DiscordWebhook] Rate limit exhausted (bucket: {}), resets in {:.2f}s",
					rateLimitInfo.bucket.empty() ? "unknown" : rateLimitInfo.bucket,
					rateLimitInfo.resetAfter);
			}
			else if (rateLimitInfo.limit > 0 && Config::GetConfig().enableDetailedLogging) {
				// Debug logging for rate limit status
				Log::GetLog()->debug("[DiscordWebhook] Sent successfully (remaining: {}/{}, resets in {:.2f}s)",
					rateLimitInfo.remaining, rateLimitInfo.limit, rateLimitInfo.resetAfter);
			}

			return true;
		}
		else if (http_code == 429) {
			// Parse rate limit response body
			try {
				auto json = nlohmann::json::parse(responseBody);
				double retryAfter = json.value("retry_after", rateLimitInfo.resetAfter);
				rateLimitInfo.isGlobal = json.value("global", false);

				Log::GetLog()->error("[DiscordWebhook] Rate limited! Message: {}, Retry after: {:.2f}s, Global: {}, Bucket: {}, Scope: {}",
					json.value("message", "Unknown"),
					retryAfter,
					rateLimitInfo.isGlobal ? "YES" : "NO",
					rateLimitInfo.bucket.empty() ? "unknown" : rateLimitInfo.bucket,
					rateLimitInfo.scope.empty() ? "unknown" : rateLimitInfo.scope);

				// Use retry_after from body if available
				if (retryAfter > 0) {
					rateLimitInfo.resetAfter = retryAfter;
				}
			}
			catch (const std::exception& e) {
				Log::GetLog()->error("[DiscordWebhook] Failed to parse 429 response body: {}", e.what());
			}

			// Block based on rate limit type
			{
				std::lock_guard<std::mutex> lock(rateLimitMutex);
				auto waitDuration = std::chrono::milliseconds(
					static_cast<long long>(rateLimitInfo.resetAfter * 1000) + 500); // Add 500ms buffer
				auto blockedUntil = std::chrono::steady_clock::now() + waitDuration;

				if (rateLimitInfo.isGlobal) {
					// GLOBAL rate limit - block ALL webhooks
					globalRateLimitUntil = blockedUntil;
					Log::GetLog()->error("[DiscordWebhook] GLOBAL RATE LIMIT ACTIVE - All Discord webhooks blocked for {:.2f}s",
						rateLimitInfo.resetAfter);
				}
				else {
					// Per-URL/bucket rate limit - block only this URL
					rateLimitInfo.blockedUntil = blockedUntil;
					rateLimits[url] = rateLimitInfo;
				}
			}

			return false;
		}
		else {
			Log::GetLog()->warn("[DiscordWebhook] Error {} HTTP {}: {}", res, http_code, url);
			if (!responseBody.empty() && responseBody.length() < 500) {
				Log::GetLog()->debug("[DiscordWebhook] Response: {}", responseBody);
			}
			return false;
		}
	}

	void UpdateRateLimitInfo(const std::string& url, const RateLimitInfo& info) {
		std::lock_guard<std::mutex> lock(rateLimitMutex);

		// Only update if we got meaningful data
		if (info.limit > 0 || !info.bucket.empty()) {
			auto& stored = rateLimits[url];
			stored.limit = info.limit;
			stored.remaining = info.remaining;
			stored.reset = info.reset;
			stored.resetAfter = info.resetAfter;
			stored.bucket = info.bucket;
			stored.scope = info.scope;

			// Don't update blockedUntil here - only set it on 429
		}
	}

	std::queue<WebhookMessage> queue;
	std::mutex queueMutex;
	std::condition_variable cv;
	std::thread worker;
	std::atomic<bool> running{ false };
	int maxRetries;
	int retryDelaySeconds;

	// Rate limiting state
	std::map<std::string, RateLimitInfo> rateLimits;  // URL -> rate limit info
	std::mutex rateLimitMutex;

	// Global rate limit state (affects ALL webhooks)
	std::chrono::steady_clock::time_point globalRateLimitUntil;
};
