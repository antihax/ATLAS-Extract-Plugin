#include "TribeChannelSubscriptionManager.h"
#include "Config.h"
#include "Logger/Logger.h"

void TribeChannelSubscriptionManager::ProcessBatches() {
	// Early exit if not leader - check before acquiring lock
	if (!leaderElector || !leaderElector->IsLeader()) {
		return;
	}

	std::lock_guard<std::mutex> lock(cacheMutex);

	// Double-check leadership after acquiring lock
	if (!leaderElector->IsLeader()) {
		return;
	}

	auto now = std::chrono::system_clock::now();

	for (auto& tribePair : tribeNotificationCache) {
		const std::string& tribeIdStr = tribePair.first;
		auto& cache = tribePair.second;

		// Check each webhook's batch
		for (auto& webhookPair : cache.pendingMessagesByWebhook) {
			const std::string& webhookUrl = webhookPair.first;

			if (webhookPair.second.empty()) {
				continue;
			}

			// Initialize lastFlushByWebhook if not present
			auto lastFlushIt = cache.lastFlushByWebhook.find(webhookUrl);
			if (lastFlushIt == cache.lastFlushByWebhook.end()) {
				cache.lastFlushByWebhook[webhookUrl] = now;
				lastFlushIt = cache.lastFlushByWebhook.find(webhookUrl);
			}

			// Check if enough time has passed since last flush
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastFlushIt->second).count();
			if (elapsed >= BATCH_INTERVAL_SECONDS) {
				// Final leadership check before flushing
				if (leaderElector->IsLeader()) {
					FlushWebhookBatch(tribeIdStr, webhookUrl, cache);
				}
			}
		}
	}
}

void TribeChannelSubscriptionManager::HandleTribeMessage(const std::string& chan, const std::string& msg) {
	try {
		// Parse binary data
		if (msg.length() < 12) {
			// Silently ignore - likely a different message type or empty message
			return;
		}

		size_t offset = 0;

		// Read TribeID (int64)
		int64_t tribeId;
		std::memcpy(&tribeId, msg.data() + offset, sizeof(int64_t));
		offset += sizeof(int64_t);

		// Validate tribe ID - should be a reasonable positive number
		// Silently ignore messages with invalid tribe IDs (likely different message types)
		if (tribeId <= 0 || tribeId > 9999999999LL) {
			return;
		}

		// Read Timestamp (int32)
		int32_t timestamp;
		std::memcpy(&timestamp, msg.data() + offset, sizeof(int32_t));
		offset += sizeof(int32_t);

		// Validate timestamp - silently ignore if invalid (likely different message type)
		if (timestamp <= 0) {
			return;
		}

		// Helper lambda to read length-prefixed string
		auto readString = [&msg, &offset]() -> std::string {
			if (offset + sizeof(int32_t) > msg.length()) {
				throw std::runtime_error("Not enough data for string length");
			}

			int32_t strLen;
			std::memcpy(&strLen, msg.data() + offset, sizeof(int32_t));
			offset += sizeof(int32_t);

			if (strLen < 0 || offset + strLen > msg.length()) {
				throw std::runtime_error("Invalid string length or not enough data");
			}

			// Sanity check: string lengths should be reasonable (max 10KB per field)
			if (strLen > 10240) {
				throw std::runtime_error("String length exceeds maximum allowed size");
			}

			std::string result(msg.data() + offset, strLen);
			offset += strLen;
			return result;
			};

		// Read strings
		std::string gameTime = readString();
		std::string category = readString();
		std::string text = readString();

		// Validate that we have meaningful data
		// Silently ignore messages where all text fields are empty (likely different message type)
		if (gameTime.empty() && category.empty() && text.empty()) {
			return;
		}

		// Check if gameTime contains mostly non-printable/garbage characters
		// Valid gameTime should be readable text like "Day 123, 12:34:56"
		if (!gameTime.empty()) {
			size_t printableCount = 0;
			for (char c : gameTime) {
				if (std::isprint(static_cast<unsigned char>(c))) {
					printableCount++;
				}
			}
			// If less than 50% of characters are printable, likely not a tribe log message
			if (printableCount < gameTime.length() / 2) {
				return;
			}
		}

		// Build TribeMessage struct
		TribeMessage tribeMsg;
		tribeMsg.tribeId = tribeId;
		tribeMsg.timestamp = static_cast<int>(timestamp);
		tribeMsg.gameTime = gameTime;
		tribeMsg.category = category;
		tribeMsg.text = text;

		// Extract tribe ID from channel name "tribemsg:{tribeId}"
		std::string tribeIdStr = chan.substr(9);

		ProcessTribeMessageForWebhooks(tribeIdStr, tribeMsg);
	}
	catch (const std::exception& e) {
		// Silently ignore parsing errors - these are likely different message types on the channel
		// Only log in detailed mode for debugging
		if (Config::GetConfig().enableDetailedLogging) {
			Log::GetLog()->debug("[TribeChannelManager] Skipped non-tribe-log message from '{}': {}", chan, e.what());

			if (msg.length() > 0 && msg.length() < 1000) {
				std::stringstream hexDump;
				hexDump << "Binary data (hex): ";
				size_t maxBytes = msg.length() < 64 ? msg.length() : 64;
				for (size_t i = 0; i < maxBytes; i++) {
					hexDump << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)msg[i] << " ";
				}
				Log::GetLog()->debug("[TribeChannelManager] {}", hexDump.str());
			}
		}
	}
}

std::string TribeChannelSubscriptionManager::StripRichTextFormatting(const std::string& text) {
	std::string result = text;

	size_t pos = 0;
	while ((pos = result.find("<RichColor", pos)) != std::string::npos) {
		size_t endPos = result.find(">", pos);
		if (endPos != std::string::npos) {
			result.erase(pos, endPos - pos + 1);
		}
		else {
			break;
		}
	}

	while ((pos = result.find("</>")) != std::string::npos) {
		result.erase(pos, 3);
	}

	return result;
}

std::string TribeChannelSubscriptionManager::FormatCategoryForDiscord(const std::string& category) {
	std::string categoryName = category;

	size_t pos = category.find("::TL_");
	if (pos != std::string::npos) {
		categoryName = category.substr(pos + 5);
	}

	// Use static map for fast lookup instead of if-else chain
	static const std::unordered_map<std::string, std::string> categoryMap = {
		{"WARNING", "WARN"},
		{"WARN", "WARN"},
		{"DEMOLISH", "DEMOLISH"},
		{"DESTROY", "DEMOLISH"}
	};

	auto it = categoryMap.find(categoryName);
	if (it != categoryMap.end()) {
		return it->second;
	}

	return categoryName; // Return as-is for INFO, ERROR, CRITICAL, DEBUG, TAME, KILL, DEATH, BUILD
}

void TribeChannelSubscriptionManager::ManagementLoop() {
	std::this_thread::sleep_for(std::chrono::seconds(5));

	while (running) {
		try {
			if (!leaderElector->IsLeader()) {
				UnsubscribeFromAllTribeChannels();
				std::this_thread::sleep_for(std::chrono::seconds(10));
				continue;
			}

			UpdateTribeChannelSubscriptions();
			std::this_thread::sleep_for(std::chrono::seconds(30));
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[TribeChannelManager] Error in management loop: {}", e.what());
			std::this_thread::sleep_for(std::chrono::seconds(10));
		}
	}

	UnsubscribeFromAllTribeChannels();
}

void TribeChannelSubscriptionManager::ProcessTribeMessageForWebhooks(const std::string& tribeIdStr, const TribeMessage& tribeMsg) {
	std::string cleanText = StripRichTextFormatting(tribeMsg.text);
	// Use consolidated sanitization helper
	cleanText = DiscordWebhookManager::SanitizeForDiscord(cleanText);
	std::string formattedCategory = FormatCategoryForDiscord(tribeMsg.category);

	// Skip empty messages - don't buffer or send them
	if (cleanText.empty()) {
		return;
	}

	PendingTribeMessage pendingMsg{
		tribeMsg.gameTime,
		formattedCategory,
		cleanText,
		std::chrono::system_clock::now()
	};

	std::lock_guard<std::mutex> lock(cacheMutex);
	auto it = tribeNotificationCache.find(tribeIdStr);
	if (it == tribeNotificationCache.end() || it->second.notifications.empty()) {
		return;
	}

	auto& cache = it->second;
	auto now = std::chrono::system_clock::now();

	for (const auto& notif : cache.notifications) {
		if (notif.webhook.empty()) continue;

		cache.pendingMessagesByWebhook[notif.webhook].push_back(pendingMsg);

		if (cache.lastFlushByWebhook.find(notif.webhook) == cache.lastFlushByWebhook.end()) {
			cache.lastFlushByWebhook[notif.webhook] = now;
		}

		// Only check for early flush if we're the leader
		if (leaderElector && leaderElector->IsLeader() && ShouldFlushWebhookBatch(cache, notif.webhook)) {
			// Double-check leadership before flushing
			if (leaderElector->IsLeader()) {
				FlushWebhookBatch(tribeIdStr, notif.webhook, cache);
			}
		}
	}
}

bool TribeChannelSubscriptionManager::ShouldFlushWebhookBatch(const TribeNotificationCache& cache, const std::string& webhookUrl) {
	auto it = cache.pendingMessagesByWebhook.find(webhookUrl);
	if (it == cache.pendingMessagesByWebhook.end() || it->second.empty()) {
		return false;
	}

	size_t estimatedSize = 0;
	for (const auto& msg : it->second) {
		estimatedSize += msg.gameTime.length() + msg.category.length() + msg.text.length() + 15;
	}

	return estimatedSize >= BATCH_THRESHOLD;
}

void TribeChannelSubscriptionManager::FlushWebhookBatch(const std::string& tribeIdStr, const std::string& webhookUrl, TribeNotificationCache& cache) {
	auto msgIt = cache.pendingMessagesByWebhook.find(webhookUrl);
	if (msgIt == cache.pendingMessagesByWebhook.end() || msgIt->second.empty()) {
		return;
	}

	if (!webhookManager) {
		msgIt->second.clear();
		return;
	}

	auto& pendingMessages = msgIt->second;

	// Use consolidated batching in DiscordWebhookManager
	webhookManager->SendBatch(
		webhookUrl,
		"Tribe " + tribeIdStr + " Log",
		pendingMessages,
		[](const PendingTribeMessage& msg) {
			return "**[" + msg.gameTime + "]** " + msg.category + " " + msg.text;
		}
	);

	cache.lastFlushByWebhook[webhookUrl] = std::chrono::system_clock::now();
}

void TribeChannelSubscriptionManager::UpdateTribeChannelSubscriptions() {
	if (!redis->LoadPlayerIdMappings()) {
		Log::GetLog()->warn("[TribeChannelManager] Failed to load player ID mappings");
		return;
	}

	if (!redis->LoadPlayerInfo()) {
		Log::GetLog()->warn("[TribeChannelManager] Failed to load player info");
		return;
	}

	auto playerNotifications = redis->GetPlayerNotifications();

	std::map<std::string, TribeNotificationCache> newCache;
	std::set<std::string> requiredTribeIds;

	for (const auto& notif : playerNotifications) {
		if (notif.webhook.empty() || notif.banned) {
			continue;
		}

		const PlayerInfo* playerInfo = redis->GetPlayerInfoBySteamId(notif.steamId);

		if (!playerInfo && Config::GetConfig().enableDetailedLogging) {
			Log::GetLog()->warn("[TribeChannelManager] Could not find player info for Steam ID '{}' from notification",
				notif.steamId);
		}

		if (playerInfo && !playerInfo->tribeId.empty() && playerInfo->tribeId != "0") {
			requiredTribeIds.insert(playerInfo->tribeId);
			newCache[playerInfo->tribeId].notifications.push_back(notif);
		}
		else if (playerInfo && Config::GetConfig().enableDetailedLogging) {
			Log::GetLog()->warn("[TribeChannelManager] Steam ID '{}' (Player '{}') found but has no tribe (tribeId: '{}')",
				notif.steamId, playerInfo->playerId, playerInfo->tribeId);
		}
	}

	{
		std::lock_guard<std::mutex> lock(cacheMutex);

		// Flush pending messages for webhooks that will be removed
		for (auto& oldTribePair : tribeNotificationCache) {
			const std::string& tribeId = oldTribePair.first;
			auto& oldCache = oldTribePair.second;

			// Check if this tribe still exists in the new cache
			auto newTribeIt = newCache.find(tribeId);

			for (auto& webhookPair : oldCache.pendingMessagesByWebhook) {
				const std::string& webhookUrl = webhookPair.first;

				// Check if this webhook still exists for this tribe
				bool webhookStillExists = false;
				if (newTribeIt != newCache.end()) {
					for (const auto& notif : newTribeIt->second.notifications) {
						if (notif.webhook == webhookUrl) {
							webhookStillExists = true;
							break;
						}
					}
				}

				// Flush messages for webhooks that are being removed
				if (!webhookStillExists && !webhookPair.second.empty()) {
					FlushWebhookBatch(tribeId, webhookUrl, oldCache);
				}
			}
		}

		// For each tribe that still exists, preserve its pending messages
		for (auto& newPair : newCache) {
			const std::string& tribeId = newPair.first;
			auto oldIt = tribeNotificationCache.find(tribeId);

			if (oldIt != tribeNotificationCache.end()) {
				// Preserve pending messages and flush times for webhooks that still exist
				for (const auto& notif : newPair.second.notifications) {
					if (notif.webhook.empty()) continue;

					auto oldMsgIt = oldIt->second.pendingMessagesByWebhook.find(notif.webhook);
					if (oldMsgIt != oldIt->second.pendingMessagesByWebhook.end()) {
						newPair.second.pendingMessagesByWebhook[notif.webhook] = std::move(oldMsgIt->second);
					}

					auto oldFlushIt = oldIt->second.lastFlushByWebhook.find(notif.webhook);
					if (oldFlushIt != oldIt->second.lastFlushByWebhook.end()) {
						newPair.second.lastFlushByWebhook[notif.webhook] = oldFlushIt->second;
					}
				}
			}
		}

		tribeNotificationCache = std::move(newCache);
	}

	std::set<std::string> toSubscribe;
	std::set<std::string> toUnsubscribe;

	{
		std::lock_guard<std::mutex> lock(subscriptionsMutex);

		for (const auto& tribeId : requiredTribeIds) {
			if (subscribedTribeIds.find(tribeId) == subscribedTribeIds.end()) {
				toSubscribe.insert(tribeId);
			}
		}

		for (const auto& tribeId : subscribedTribeIds) {
			if (requiredTribeIds.find(tribeId) == requiredTribeIds.end()) {
				toUnsubscribe.insert(tribeId);
			}
		}
	}

	for (const auto& tribeId : toSubscribe) {
		std::string channel = "tribemsg:" + tribeId;
		auto callback = [this](const std::string& chan, const std::string& msg) {
			this->HandleTribeMessage(chan, msg);
			};

		if (redis->Subscribe(channel, callback)) {
			std::lock_guard<std::mutex> lock(subscriptionsMutex);
			subscribedTribeIds.insert(tribeId);
			Log::GetLog()->info("[TribeChannelManager] Subscribed to tribe channel: {}", channel);
		}
	}

	for (const auto& tribeId : toUnsubscribe) {
		std::string channel = "tribemsg:" + tribeId;
		if (redis->Unsubscribe(channel)) {
			std::lock_guard<std::mutex> lock(subscriptionsMutex);
			subscribedTribeIds.erase(tribeId);
			Log::GetLog()->info("[TribeChannelManager] Unsubscribed from tribe channel: {}", channel);
		}
	}
}

void TribeChannelSubscriptionManager::UnsubscribeFromAllTribeChannels() {
	std::lock_guard<std::mutex> lock(subscriptionsMutex);

	if (subscribedTribeIds.empty()) {
		return;
	}
	if (Config::GetConfig().enableDetailedLogging)
		Log::GetLog()->info("[TribeChannelManager] Unsubscribing from all {} tribe channels", subscribedTribeIds.size());

	for (const auto& tribeId : subscribedTribeIds) {
		std::string channel = "tribemsg:" + tribeId;
		redis->Unsubscribe(channel);
	}

	subscribedTribeIds.clear();
}
