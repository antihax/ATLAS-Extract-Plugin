#include "GlobalChatHandler.h"
#include "DiscordWebhookLeaderElector.h"
#include "DiscordWebhookManager.h"
#include "Config.h"
#include "Logger/Logger.h"

GlobalChatHandler::GlobalChatHandler(DiscordWebhookLeaderElector* leader, DiscordWebhookManager* webhook)
	: leaderElector(leader), webhookManager(webhook), lastFlushTime(std::chrono::system_clock::now()) {
}

void GlobalChatHandler::HandleChatMessage(const std::string& serverGrid, const std::string& playerName, const std::string& message) {

	// Only buffer if we have a webhook URL configured and this server is the leader
	if (!leaderElector || !leaderElector->IsLeader() || Config::GetConfig().globalChatWebhookUrl.empty()) {
		return;
	}

	// Sanitize message to prevent Discord mentions using consolidated helper
	std::string sanitizedMessage = DiscordWebhookManager::SanitizeForDiscord(message);

	if (Config::GetConfig().enableDetailedLogging) {
		Log::GetLog()->info("[GlobalChatHandler] Buffered chat message from '{}' on grid '{}': {}", playerName, serverGrid, sanitizedMessage);
	}

	bool shouldFlush = false;
	{
		std::lock_guard<std::mutex> lock(mutex);
		pendingMessages.push_back({
			serverGrid,
			playerName,
			sanitizedMessage,
			std::chrono::system_clock::now()
			});

		// Check if we should flush early due to size
		shouldFlush = ShouldFlushBySize();
	}

	// Flush immediately if approaching content limit
	if (shouldFlush) {
		FlushBatch();
	}
}

void GlobalChatHandler::ProcessBatch() {
	// Early exit if not leader - check before work
	if (!leaderElector || !leaderElector->IsLeader() || Config::GetConfig().globalChatWebhookUrl.empty()) {
		return;
	}

	bool shouldFlush = false;
	{
		std::lock_guard<std::mutex> lock(mutex);

		// Double-check leadership after acquiring lock
		if (!leaderElector->IsLeader()) {
			return;
		}

		if (!pendingMessages.empty()) {
			auto now = std::chrono::system_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastFlushTime).count();
			shouldFlush = (elapsed >= BATCH_INTERVAL_SECONDS);
		}
	}

	// Flush if time has elapsed (outside lock to avoid deadlock) with final leadership check
	if (shouldFlush && leaderElector->IsLeader()) {
		FlushBatch();
	}
}

void GlobalChatHandler::FlushBatch() {
	// Final leadership check before flushing
	if (!leaderElector || !leaderElector->IsLeader()) {
		return;
	}

	if (!webhookManager || Config::GetConfig().globalChatWebhookUrl.empty()) {
		return;
	}

	std::deque<PendingChatMessage> messagesToSend;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (pendingMessages.empty()) {
			return;
		}

		// Triple-check leadership before committing to send
		if (!leaderElector->IsLeader()) {
			return;
		}

		messagesToSend.swap(pendingMessages);
		lastFlushTime = std::chrono::system_clock::now();
		if (Config::GetConfig().enableDetailedLogging) {
			Log::GetLog()->info("[GlobalChatHandler] Flushing {} chat messages to Discord webhook", messagesToSend.size());
		}
	}

	// Use consolidated batching in DiscordWebhookManager
	webhookManager->SendBatch(
		Config::GetConfig().globalChatWebhookUrl,
		"Atlas Global Chat",
		messagesToSend,
		[](const PendingChatMessage& msg) {
			return "[" + msg.serverGrid + "] **" + msg.playerName + "**: " + msg.message;
		}
	);
}

bool GlobalChatHandler::ShouldFlushBySize() const {
	// Must be called with mutex locked
	size_t estimatedSize = 0;
	for (const auto& msg : pendingMessages) {
		// Estimate: [grid] **name**: message + newline
		estimatedSize += msg.serverGrid.length() + msg.playerName.length() + msg.message.length() + 10;
	}
	return estimatedSize >= BATCH_THRESHOLD;
}
