#pragma once // This program is licensed under the GNU GPL v3.0, except that commercial use is prohibited.

#include <API/Atlas/Atlas.h>
#define _WS2DEF_
#define _WINSOCK2API_
#include <cpp_redis/cpp_redis>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <vector>
#include <chrono>
#include <functional>
#include "json.hpp"
#include "Config.h"
#include "RedisHelpers.h"

#pragma comment(lib, "cpp_redis.lib")
#pragma comment(lib, "tacopie.lib")

enum PeerServerMessageType {
	PSM_Ping = 0x0,
	PSM_Pong = 0x1,
	PSM_FetchTreasureLocation = 0x2,
	PSM_FetchedTreasureLocation = 0x3,
	PSM_TravelLog = 0x4,
	PSM_MAX = 0x5,
	PSM_FetchTreasureLocationSpecial = 0x6,
	PSM_HeartBeat = 0x7,
	PSM_GlobalChat = 0x8,
};

struct PlayerNotificationConfig {
	std::string steamId;  // Steam ID from playernotif key and PlayerId field
	std::string webhook;
	int webhookCategoryMask = 0;
	std::string email;
	int emailCategoryMask = 0;
	bool banned = false;
};

struct PlayerInfo {
	std::string playerId;
	std::string tribeId;
	std::string playerName;
	std::string lastOnlineAt;
	std::string rankGroupId;
};

struct TribeMessage {
	int64_t tribeId;      // tid
	int timestamp;        // ts
	std::string gameTime; // gt
	std::string category; // cat
	std::string text;     // text
};

class RedisManager {
public:
	using MessageHandler = std::function<void(const std::string&, const std::string&)>;
	using SubscribeCallback = std::function<void(const std::string&, const std::string&)>;

	static RedisManager& Instance() {
		static RedisManager instance;
		return instance;
	}

	// Initialize Redis connection
	// Returns true if initialization was successful
	bool Initialize() {
		FRedisDatabaseConnectionInfo config;
		if (!TryGetRedisConfig(config)) {
			return false;
		}
		const std::string& host = config.URL;
		const std::string& password = config.Password;
		__int64 port = config.Port;
		auto connection_callback = [this, host, port](const std::string&, std::size_t, cpp_redis::connect_state status) {
			if (status == cpp_redis::connect_state::ok) {
				Log::GetLog()->info("[Redis] Connected to {}:{}", host, port);
				connected = true;
			}
			else if (status == cpp_redis::connect_state::dropped ||
				status == cpp_redis::connect_state::failed ||
				status == cpp_redis::connect_state::lookup_failed) {
				Log::GetLog()->error("[Redis] Connection lost to {}:{}", host, port);
				connected = false;
			}
			};
		// max_reconnects = 0U means reconnect forever (unlimited attempts)
		subscriber.connect(host, port, connection_callback, 0U, Config::GetConfig().connectionTimeout, 0U);
		client.connect(host, port, connection_callback, 0U, Config::GetConfig().connectionTimeout, 0U);
		if (!password.empty()) {
			subscriber.auth(password, &RedisManager::AuthCallback);
			client.auth(password, &RedisManager::AuthCallback);
			subscriber.commit();
			client.commit();
		}
		subscriber.subscribe(Config::GetConfig().channelCluster, [this](const std::string& chan, const std::string& msg) {
			if (messageHandler) messageHandler(chan, msg);
			});
		subscriber.subscribe(GetChannelForServerId(GetServerId()), [this](const std::string& chan, const std::string& msg) {
			if (messageHandler) messageHandler(chan, msg);
			});
		subscriber.commit();
		connected = true;
		return true;
	}

	// Shutdown Redis connection
	void Shutdown() {
		if (subscriber.is_connected()) subscriber.disconnect(true);
		if (client.is_connected()) client.disconnect(true);
		connected = false;
	}

	inline std::vector<std::string> ScanRedisKeys(const std::string& pattern, size_t count = 100) {
		std::vector<std::string> keys;
		std::size_t cursor = 0;

		do {
			auto scan_future = client.scan(cursor, pattern, count);
			client.sync_commit();
			auto scan_result = scan_future.get();

			if (scan_result.is_array() && scan_result.as_array().size() >= 2) {
				// First element is the new cursor
				auto cursor_reply = scan_result.as_array()[0];
				if (cursor_reply.is_string()) {
					cursor = std::stoull(cursor_reply.as_string());
				}

				// Second element is the array of keys
				auto keys_reply = scan_result.as_array()[1];
				if (keys_reply.is_array()) {
					for (const auto& key_reply : keys_reply.as_array()) {
						if (key_reply.is_string()) {
							keys.push_back(key_reply.as_string());
						}
					}
				}
			}
		} while (cursor != 0);

		return keys;
	}

	// Check if Redis is connected
	bool IsConnected() const {
		return connected.load();
	}

	// Set the global message handler for all subscriptions
	void SetMessageHandler(MessageHandler handler) {
		messageHandler = std::move(handler);
	}

	// Publish a JSON message to a Redis channel
	// Returns true if the message was published successfully
	bool PublishMessage(const std::string& channel, const nlohmann::json& msg) {
		if (!connected) {
			Log::GetLog()->error("[Redis] Not connected, cannot publish to '{}'", channel);
			return false;
		}

		try {
			client.publish(channel, msg.dump());
			client.commit();
			return true;
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Redis] Error publishing to '{}': {}", channel, e.what());
			return false;
		}
	}

	// Subscribe to a Redis channel
	// The provided callback will be called when messages are received on this channel
	// If no callback is provided, the global messageHandler will be used instead
	// Returns true if subscription was successful
	bool Subscribe(const std::string& channel, SubscribeCallback callback = nullptr) {
		if (!connected) {
			Log::GetLog()->error("[Redis] Not connected, cannot subscribe to '{}'", channel);
			return false;
		}

		try {
			if (callback) {
				subscriber.subscribe(channel, std::move(callback));
			}
			else {
				subscriber.subscribe(channel, [this](const std::string& chan, const std::string& msg) {
					if (messageHandler) messageHandler(chan, msg);
					});
			}
			subscriber.commit();
			if (Config::GetConfig().enableDetailedLogging)
				Log::GetLog()->info("[Redis] Subscribed to channel '{}'", channel);
			return true;
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Redis] Error subscribing to '{}': {}", channel, e.what());
			return false;
		}
	}

	// Unsubscribe from a Redis channel
	// Returns true if unsubscription was successful
	bool Unsubscribe(const std::string& channel) {
		if (!connected) {
			Log::GetLog()->error("[Redis] Not connected, cannot unsubscribe from '{}'", channel);
			return false;
		}

		try {
			subscriber.unsubscribe(channel);
			subscriber.commit();
			if (Config::GetConfig().enableDetailedLogging)
				Log::GetLog()->info("[Redis] Unsubscribed from channel '{}'", channel);
			return true;
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Redis] Error unsubscribing from '{}': {}", channel, e.what());
			return false;
		}
	}

	// Subscribe to a Redis pattern (wildcard subscription)
	// The provided callback will be called when messages are received on channels matching the pattern
	// If no callback is provided, the global messageHandler will be used instead
	// Returns true if subscription was successful
	bool PSubscribe(const std::string& pattern, SubscribeCallback callback = nullptr) {
		if (!connected) {
			Log::GetLog()->error("[Redis] Not connected, cannot psubscribe to '{}'", pattern);
			return false;
		}

		try {
			if (callback) {
				subscriber.psubscribe(pattern, std::move(callback));
			}
			else {
				subscriber.psubscribe(pattern, [this](const std::string& chan, const std::string& msg) {
					if (messageHandler) messageHandler(chan, msg);
					});
			}
			subscriber.commit();
			return true;
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Redis] Error psubscribing to '{}': {}", pattern, e.what());
			return false;
		}
	}

	// Unsubscribe from a Redis pattern
	// Returns true if unsubscription was successful
	bool PUnsubscribe(const std::string& pattern) {
		if (!connected) {
			Log::GetLog()->error("[Redis] Not connected, cannot punsubscribe from '{}'", pattern);
			return false;
		}

		try {
			subscriber.punsubscribe(pattern);
			subscriber.commit();
			return true;
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Redis] Error punsubscribing from '{}': {}", pattern, e.what());
			return false;
		}
	}

	// Set a key with NX (only if not exists) and expiry
	// Returns true if the key was set (i.e., the lock was acquired)
	bool SetKeyNX(const std::string& key, const std::string& value, int expireMs) {
		if (!connected) {
			Log::GetLog()->error("[Redis] Not connected, cannot set key '{}'", key);
			return false;
		}

		try {
			// Use the SET command with NX and PX options
			auto future = client.send({ "SET", key, value, "NX", "PX", std::to_string(expireMs) });
			client.sync_commit();
			auto result = future.get();
			return result.is_simple_string() && result.as_string() == "OK";
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Redis] Error setting key '{}': {}", key, e.what());
			return false;
		}
	}

	// Try to renew a key only if we own it
	// Returns true if we successfully renewed the key we own
	bool RenewKeyIfOwned(const std::string& key, const std::string& expectedValue, int expireMs) {
		if (!connected) {
			Log::GetLog()->error("[Redis] Not connected, cannot renew key '{}'", key);
			return false;
		}

		try {
			// Use Lua script for atomic check-and-renew
			// Script: if current value == expected value, then SET with new TTL and return 1, else return 0
			const char* lua_script = R"(
				local current = redis.call('GET', KEYS[1])
				if current == ARGV[1] then
					redis.call('PSETEX', KEYS[1], ARGV[2], ARGV[1])
					return 1
				else
					return 0
				end
			)";

			// EVAL script numkeys key [key ...] arg [arg ...]
			// KEYS[1] = key, ARGV[1] = expectedValue, ARGV[2] = expireMs
			auto future = client.send({ "EVAL", lua_script, "1", key, expectedValue, std::to_string(expireMs) });
			client.sync_commit();
			auto result = future.get();

			// Result is 1 if we owned and renewed, 0 otherwise
			return result.is_integer() && result.as_integer() == 1;
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Redis] Error renewing key '{}': {}", key, e.what());
			return false;
		}
	}

	// Set expiry time for a key
	// Returns true if the key's expiry was set
	bool SetKeyExpiry(const std::string& key, int expireSeconds) {
		if (!connected) {
			Log::GetLog()->error("[Redis] Not connected, cannot set expiry for key '{}'", key);
			return false;
		}

		try {
			auto future = client.expire(key, expireSeconds);
			client.sync_commit();
			auto result = future.get();
			return result.is_integer() && result.as_integer() == 1;
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Redis] Error setting expiry for key '{}': {}", key, e.what());
			return false;
		}
	}

	// Get all player notification configurations from Redis
	// Returns a vector of PlayerNotificationConfig, or empty vector on error
	std::vector<PlayerNotificationConfig> GetPlayerNotifications() {
		std::vector<PlayerNotificationConfig> configs;

		if (!Config::GetConfig().enablePlayerNotifications) {
			return configs;
		}

		if (!connected) {
			Log::GetLog()->error("[Redis] Not connected, cannot get player notifications");
			return configs;
		}

		try {
			// Use consolidated SCAN helper
			auto keys = this->ScanRedisKeys("playernotif:*");

			// Now fetch HGETALL for each key
			for (const auto& key : keys) {
				// Extract Steam ID from key "playernotif:{steamId}"
				std::string steamId = key.substr(13); // Skip "playernotif:"

				auto hgetall_future = client.hgetall(key);
				client.sync_commit();
				auto hash_result = hgetall_future.get();

				if (hash_result.is_array()) {
					PlayerNotificationConfig config;
					auto& arr = hash_result.as_array();

					// Parse hash fields (format: field1, value1, field2, value2, ...)
					for (size_t i = 0; i + 1 < arr.size(); i += 2) {
						if (arr[i].is_string() && arr[i + 1].is_string()) {
							std::string field = arr[i].as_string();
							std::string value = arr[i + 1].as_string();

							if (field == "PlayerId") {
								config.steamId = value;
							}
							else if (field == "Webhook") {
								config.webhook = value;
							}
							else if (field == "WebhookCategoryMask") {
								try {
									config.webhookCategoryMask = std::stoi(value);
								}
								catch (...) {
									config.webhookCategoryMask = 0;
								}
							}
							else if (field == "Email") {
								config.email = value;
							}
							else if (field == "EmailCategoryMask") {
								try {
									config.emailCategoryMask = std::stoi(value);
								}
								catch (...) {
									config.emailCategoryMask = 0;
								}
							}
							else if (field == "Banned") {
								config.banned = (value == "True" || value == "true" || value == "1");
							}
						}
					}

					// If PlayerId wasn't set in the hash, use the Steam ID from the key to look it up
					if (config.steamId.empty() && !steamId.empty()) {
						// Try to get playerId from Steam ID mapping
						std::lock_guard<std::mutex> lock(playerIdMutex);
						auto it = steamIdToPlayerId.find(steamId);
						if (it != steamIdToPlayerId.end()) {
							config.steamId = it->second;
						}
						else {
							// If we don't have the mapping, store the Steam ID as playerId for now
							// The caller can try to resolve it later
							config.steamId = steamId;
						}
					}

					// Only add if webhook is not empty and not banned
					if (!config.webhook.empty() && !config.banned) {
						configs.push_back(config);
					}
				}
			}
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Redis] Error fetching player notifications: {}", e.what());
		}

		return configs;
	}

	// Load bidirectional Steam ID <-> Player ID mappings from Redis
	// Populates internal lookup maps for quick bidirectional lookup
	// Should be called before GetPlayerNotifications() if you need player ID lookups
	// Returns true if successful
	bool LoadPlayerIdMappings() {
		if (!connected) {
			Log::GetLog()->error("[Redis] Not connected, cannot load player ID mappings");
			return false;
		}

		try {
			std::lock_guard<std::mutex> lock(playerIdMutex);
			playerIdToSteamId.clear();
			steamIdToPlayerId.clear();

			// Use consolidated SCAN helper
			auto keys = this->ScanRedisKeys("PlayerDataId:*");

			// Now fetch the value for each key
			for (const auto& key : keys) {
				// Extract player ID from key "PlayerDataId:{playerId}"
				std::string playerId = key.substr(13); // Skip "PlayerDataId:"

				auto get_future = client.get(key);
				client.sync_commit();
				auto result = get_future.get();

				if (result.is_string()) {
					std::string steamId = result.as_string();

					if (!playerId.empty() && !steamId.empty()) {
						playerIdToSteamId[playerId] = steamId;
						steamIdToPlayerId[steamId] = playerId;
					}
				}
			}

			return true;
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Redis] Error loading player ID mappings: {}", e.what());
			return false;
		}
	}

	// Load player information from Redis (playerinfo:* hashes)
	// Populates internal map of player ID -> PlayerInfo
	// Should be called after LoadPlayerIdMappings() to enable lookups by Steam ID
	// Returns true if successful
	bool LoadPlayerInfo() {
		if (!connected) {
			Log::GetLog()->error("[Redis] Not connected, cannot load player info");
			return false;
		}

		try {
			std::lock_guard<std::mutex> lock(playerInfoMutex);
			playerInfoByPlayerId.clear();

			// Use consolidated SCAN helper
			auto keys = this->ScanRedisKeys("playerinfo:*");

			// Now fetch HGETALL for each key
			for (const auto& key : keys) {
				auto hgetall_future = client.hgetall(key);
				client.sync_commit();
				auto hash_result = hgetall_future.get();

				if (hash_result.is_array()) {
					PlayerInfo info;
					auto& arr = hash_result.as_array();

					// Parse hash fields (format: field1, value1, field2, value2, ...)
					for (size_t i = 0; i + 1 < arr.size(); i += 2) {
						if (arr[i].is_string() && arr[i + 1].is_string()) {
							std::string field = arr[i].as_string();
							std::string value = arr[i + 1].as_string();

							if (field == "PlayerId") {
								info.playerId = value;
							}
							else if (field == "TribeID") {
								info.tribeId = value;
							}
							else if (field == "PlayerName") {
								info.playerName = value;
							}
							else if (field == "LastOnlineAt") {
								info.lastOnlineAt = value;
							}
							else if (field == "RankGroupId") {
								info.rankGroupId = value;
							}
						}
					}

					// Add if playerId is valid
					if (!info.playerId.empty()) {
						playerInfoByPlayerId[info.playerId] = info;
					}
				}
			}

			return true;
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Redis] Error loading player info: {}", e.what());
			return false;
		}
	}

	// Get player info by Player ID
	// Returns nullptr if not found
	const PlayerInfo* GetPlayerInfoByPlayerId(const std::string& playerId) const {
		std::lock_guard<std::mutex> lock(playerInfoMutex);
		auto it = playerInfoByPlayerId.find(playerId);
		return (it != playerInfoByPlayerId.end()) ? &it->second : nullptr;
	}

	// Get player info by Steam ID (requires LoadPlayerIdMappings to be called first)
	// Returns nullptr if not found
	const PlayerInfo* GetPlayerInfoBySteamId(const std::string& steamId) const {
		std::string playerId = GetPlayerIdFromSteamId(steamId);
		if (playerId.empty()) {
			return nullptr;
		}
		return GetPlayerInfoByPlayerId(playerId);
	}

	// Get player name by Player ID
	// Returns empty string if not found
	std::string GetPlayerName(const std::string& playerId) const {
		auto info = GetPlayerInfoByPlayerId(playerId);
		return info ? info->playerName : "";
	}

	// Get tribe ID by Player ID
	// Returns empty string if not found
	std::string GetTribeId(const std::string& playerId) const {
		auto info = GetPlayerInfoByPlayerId(playerId);
		return info ? info->tribeId : "";
	}

	// Check if a player exists in the info cache
	bool HasPlayerInfo(const std::string& playerId) const {
		std::lock_guard<std::mutex> lock(playerInfoMutex);
		return playerInfoByPlayerId.find(playerId) != playerInfoByPlayerId.end();
	}

	// Get Steam ID from Player ID
	// Returns empty string if not found
	std::string GetSteamIdFromPlayerId(const std::string& playerId) const {
		std::lock_guard<std::mutex> lock(playerIdMutex);
		auto it = playerIdToSteamId.find(playerId);
		return (it != playerIdToSteamId.end()) ? it->second : "";
	}

	// Get Player ID from Steam ID
	// Returns empty string if not found
	std::string GetPlayerIdFromSteamId(const std::string& steamId) const {
		std::lock_guard<std::mutex> lock(playerIdMutex);
		auto it = steamIdToPlayerId.find(steamId);
		return (it != steamIdToPlayerId.end()) ? it->second : "";
	}

	// Check if a Player ID exists in the mapping
	bool HasPlayerId(const std::string& playerId) const {
		std::lock_guard<std::mutex> lock(playerIdMutex);
		return playerIdToSteamId.find(playerId) != playerIdToSteamId.end();
	}

	// Check if a Steam ID exists in the mapping
	bool HasSteamId(const std::string& steamId) const {
		std::lock_guard<std::mutex> lock(playerIdMutex);
		return steamIdToPlayerId.find(steamId) != steamIdToPlayerId.end();
	}

	// Get the current server ID
	static unsigned int GetServerId() {
		return static_cast<UShooterGameInstance*>(ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())
			->GridInfoField()->GetCurrentServerId();
	}

	// Get the Redis channel name for a specific server ID
	static std::string GetChannelForServerId(unsigned int id) {
		return Config::GetConfig().channelServer + std::to_string(id);
	}



private:
	RedisManager() {};
	RedisManager(const RedisManager&) = delete;
	RedisManager& operator=(const RedisManager&) = delete;

	static void AuthCallback(cpp_redis::reply& reply) {
		if (reply.is_error()) {
			Log::GetLog()->error("[Redis] Authentication failed: {}", reply.error());
			Instance().connected = false;
		}
	}

	bool TryGetRedisConfig(FRedisDatabaseConnectionInfo& outConfig) {
		try {
			const auto connectionMap = FDatabaseRedisShared::ConnectionLookups();
			for (const auto& conn : connectionMap) {
				if (conn.Key.Equals("Default")) {
					outConfig = conn.Value;
					return true;
				}
			}
		}
		catch (const std::exception& e) {
			Log::GetLog()->debug("[Redis] Redis config not yet available: {}", e.what());
		}
		return false;
	}

	// Redis
	cpp_redis::subscriber subscriber;
	cpp_redis::client client;
	std::atomic<bool> connected{ false };

	// Server tracking
	std::map<int, std::chrono::system_clock::time_point> lastSeen;
	std::mutex lastSeenMutex;
	MessageHandler messageHandler;

	// Player ID mappings
	std::map<std::string, std::string> playerIdToSteamId;
	std::map<std::string, std::string> steamIdToPlayerId;
	mutable std::mutex playerIdMutex;

	// Player information cache
	std::map<std::string, PlayerInfo> playerInfoByPlayerId;
	mutable std::mutex playerInfoMutex;
};