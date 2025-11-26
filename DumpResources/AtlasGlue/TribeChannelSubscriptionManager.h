#pragma once // This program is licensed under the GNU GPL v3.0, except that commercial use is prohibited.
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <set>
#include <map>
#include <unordered_map>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <deque>
#include "RedisManager.h"
#include "DiscordWebhookLeaderElector.h"
#include "DiscordWebhookManager.h"
#include "Logger/Logger.h"
#include "Config.h"

class TribeChannelSubscriptionManager {
public:
	TribeChannelSubscriptionManager(RedisManager* redis, DiscordWebhookLeaderElector* leaderElector, DiscordWebhookManager* webhookManager)
		: redis(redis), leaderElector(leaderElector), webhookManager(webhookManager), running(false) {
	}

	void Start() {
		running = true;
		managerThread = std::thread([this]() { ManagementLoop(); });
	}

	void Stop() {
		running = false;
		if (managerThread.joinable()) managerThread.join();
	}

	// Process batched messages (called from engine tick)
	void ProcessBatches();

	// Handle incoming tribe messages from subscribed channels
	void HandleTribeMessage(const std::string& chan, const std::string& msg);

private:
	// Structure to hold pending tribe message for batching
	struct PendingTribeMessage {
		std::string gameTime;
		std::string category;
		std::string text;
		std::chrono::system_clock::time_point timestamp;
	};

	// Structure to hold cached notification data per tribe with batching
	struct TribeNotificationCache {
		std::vector<PlayerNotificationConfig> notifications;
		std::map<std::string, std::deque<PendingTribeMessage>> pendingMessagesByWebhook; // webhook URL -> pending messages
		std::map<std::string, std::chrono::system_clock::time_point> lastFlushByWebhook; // webhook URL -> last flush time
	};

	// Batching constants
	static constexpr int BATCH_INTERVAL_SECONDS = 20;
	static constexpr size_t CONTENT_LIMIT = 2000;
	static constexpr size_t BATCH_THRESHOLD = 1800;

	// Helper functions
	std::string StripRichTextFormatting(const std::string& text);
	std::string FormatCategoryForDiscord(const std::string& category);

	// Management and processing functions
	void ManagementLoop();
	void ProcessTribeMessageForWebhooks(const std::string& tribeIdStr, const TribeMessage& tribeMsg);
	bool ShouldFlushWebhookBatch(const TribeNotificationCache& cache, const std::string& webhookUrl);
	void FlushWebhookBatch(const std::string& tribeIdStr, const std::string& webhookUrl, TribeNotificationCache& cache);
	void UpdateTribeChannelSubscriptions();
	void UnsubscribeFromAllTribeChannels();

	RedisManager* redis;
	DiscordWebhookLeaderElector* leaderElector;
	DiscordWebhookManager* webhookManager;
	std::atomic<bool> running;
	std::thread managerThread;

	// Track which tribe channels we're subscribed to
	std::set<std::string> subscribedTribeIds;
	std::mutex subscriptionsMutex;

	// Cache of notifications indexed by tribe ID for fast message processing
	std::map<std::string, TribeNotificationCache> tribeNotificationCache;
	std::mutex cacheMutex;
};
