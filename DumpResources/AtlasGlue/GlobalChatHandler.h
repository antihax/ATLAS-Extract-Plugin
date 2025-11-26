#pragma once // This program is licensed under the GNU GPL v3.0, except that commercial use is prohibited.
#include <string>
#include <deque>
#include <mutex>
#include <chrono>
#include "json.hpp"

// Forward declarations
class DiscordWebhookLeaderElector;
class DiscordWebhookManager;

struct PendingChatMessage {
	std::string serverGrid;
	std::string playerName;
	std::string message;
	std::chrono::system_clock::time_point timestamp;
};

class GlobalChatHandler {
public:
	GlobalChatHandler(DiscordWebhookLeaderElector* leader, DiscordWebhookManager* webhook);
	~GlobalChatHandler() = default;

	// Handle incoming chat message from Redis
	void HandleChatMessage(const std::string& serverGrid, const std::string& playerName, const std::string& message);

	// Called periodically to flush batched messages
	void ProcessBatch();

private:
	// Flush batched messages to Discord
	void FlushBatch();

	// Check if we should flush based on size
	bool ShouldFlushBySize() const;

	// Dependencies
	DiscordWebhookLeaderElector* leaderElector;
	DiscordWebhookManager* webhookManager;

	// Batching state
	std::mutex mutex;
	std::deque<PendingChatMessage> pendingMessages;
	std::chrono::system_clock::time_point lastFlushTime;

	// Constants
	static const int BATCH_INTERVAL_SECONDS = 20;
	static const size_t CONTENT_LIMIT = 2000;
	static const size_t BATCH_THRESHOLD = 1800; // Send early if approaching limit
};
