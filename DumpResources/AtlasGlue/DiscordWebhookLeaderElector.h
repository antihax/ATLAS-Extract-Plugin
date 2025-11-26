#pragma once // This program is licensed under the GNU GPL v3.0, except that commercial use is prohibited.
#include <atomic>
#include <chrono>
#include <string>
#include "RedisManager.h"
#include "Logger/Logger.h"
#include "Config.h"

class DiscordWebhookLeaderElector {
public:
	DiscordWebhookLeaderElector(RedisManager* redis, std::string serverId)
		: redis(redis), serverId(std::move(serverId)), isLeader(false), running(false),
		lastHeartbeat(std::chrono::steady_clock::now()), lastElectionCheck(std::chrono::steady_clock::now()) {
	}

	void Start() {
		running = true;
		lastElectionCheck = std::chrono::steady_clock::now();
		lastHeartbeat = std::chrono::steady_clock::now();
		Log::GetLog()->info("[DiscordWebhook] Leader elector started for server {}", serverId);
	}

	void Stop() {
		running = false;
		// Drop leadership when stopping
		if (isLeader.load()) {
			isLeader = false;
			Log::GetLog()->info("[DiscordWebhook] Server {} dropped leadership on shutdown", serverId);
		}
	}

	bool IsLeader() const {
		return isLeader.load();
	}

	// Called from game tick to prove we're alive
	// Can still be called by batch processors for additional heartbeat updates
	void Heartbeat() {
		lastHeartbeat = std::chrono::steady_clock::now();
	}

	// Called from game tick to check/renew leadership
	// Automatically updates heartbeat since being called proves the game loop is running
	void Tick() {
		if (!running) {
			return;
		}

		// We're alive!
		Heartbeat();

		try {
			auto now = std::chrono::steady_clock::now();
			bool currentlyLeader = isLeader.load();

			if (currentlyLeader) {
				// Check if it's time to renew (every 7 seconds)
				auto timeSinceLastCheck = std::chrono::duration_cast<std::chrono::seconds>(now - lastElectionCheck.load()).count();
				if (timeSinceLastCheck < 7) {
					return; // Not time to renew yet
				}

				// Verify we still own the key AND renew it atomically. Prevent dual-leadership.
				bool renewed = redis->RenewKeyIfOwned("discord_webhook_leader", serverId, 10000);

				if (renewed) {
					// Successfully renewed - we still own the key
					lastElectionCheck = now;
					if (Config::GetConfig().enableDetailedLogging) {
						Log::GetLog()->debug("[DiscordWebhook] Server {} renewed leadership", serverId);
					}
				}
				else {
					// Renewal failed - either key doesn't exist or belongs to someone else
					// Immediately drop leadership
					isLeader = false;
					Log::GetLog()->warn("[DiscordWebhook] Server {} lost leadership (ownership verification failed)", serverId);
					lastElectionCheck = now;
				}
			}
			else {
				// Not currently the leader - check if it's time to try acquiring
				auto timeSinceLastCheck = std::chrono::duration_cast<std::chrono::seconds>(now - lastElectionCheck.load()).count();
				if (timeSinceLastCheck < 2) {
					return; // Wait 2 seconds between acquisition attempts
				}

				// Try to acquire leadership
				bool acquired = redis->SetKeyNX("discord_webhook_leader", serverId, 10000);

				if (acquired) {
					isLeader = true;
					lastHeartbeat = std::chrono::steady_clock::now(); // Reset heartbeat on acquisition
					lastElectionCheck = now;
					Log::GetLog()->info("[DiscordWebhook] Server {} acquired leadership", serverId);
				}
				else {
					// Failed to acquire - another server is leader
					lastElectionCheck = now;
				}
			}
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[DiscordWebhook] Leader election error for server {}: {}", serverId, e.what());
			// On exception, immediately assume we lost leadership to be safe
			bool wasLeader = isLeader.exchange(false);
			if (wasLeader) {
				Log::GetLog()->warn("[DiscordWebhook] Server {} lost leadership due to exception", serverId);
			}
			lastElectionCheck = std::chrono::steady_clock::now();
		}
	}

private:
	RedisManager* redis;
	std::string serverId;
	std::atomic<bool> isLeader;
	std::atomic<bool> running;
	std::atomic<std::chrono::steady_clock::time_point> lastHeartbeat;
	std::atomic<std::chrono::steady_clock::time_point> lastElectionCheck;
};
