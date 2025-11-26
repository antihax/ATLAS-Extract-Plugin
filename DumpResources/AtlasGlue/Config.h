#pragma once // This program is licensed under the GNU GPL v3.0, except that commercial use is prohibited.
#include "json.hpp"
#include <string>
#include <fstream>
#include <filesystem>
#include <API/Atlas/Atlas.h>

struct PluginConfig {
	int heartbeatInterval = 5;
	int retryDelaySeconds = 30;
	int connectionTimeout = 15000;
	bool enableDetailedLogging = false;
	bool enableGlobalChat = true;
	bool enableExtendedTax = true;
	bool enableTreasureBottlesFix = true;
	bool enablePlayerNotifications = true;
	bool enableOldStructureChecks = true;
	bool enableUnlimitedRespecs = true;
	std::string globalChatWebhookUrl = "";
	std::string channelCluster = "seamless:cluster";
	std::string channelServer = "seamless:";
	float treasureQualityBoostMax = 78.0f; // max extra boost
	float treasureQualityBoostRamp = 8.0f;  // steep bias towards small boosts
	int treasureRetries = 3;
	bool tresureRegionLock = true;
	int treasurePickDelayMs = 250;
};

class Config {
public:
	static Config& Instance() {
		static Config instance;
		return instance;
	}

	// Main Accessor for the config
	static PluginConfig& GetConfig() {
		return Instance().config;
	}

	void Load(const std::string& configPath = "AtlasGlue.json") {
		bool needSave = false;
		try {
			std::filesystem::create_directories("config");
			nlohmann::json j;
			std::ifstream file(configPath);
			if (file.is_open()) {
				file >> j;
				file.close();
			}
			else {
				// File missing, use defaults and save
				j = nlohmann::json::object();
				needSave = true;
				Log::GetLog()->warn("[Config] No config file found at {}, creating default", configPath);
			}

			// For each field, check if present, else use default and mark for save
			auto setOrDefault = [&](auto& field, const char* name, auto&& def) {
				if (j.contains(name) && !j[name].is_null()) {
					field = j[name].get<std::decay_t<decltype(field)>>();
				}
				else {
					j[name] = def;
					field = def;
					needSave = true;
				}
				};


			setOrDefault(config.enableDetailedLogging, "enableDetailedLogging", false);
			setOrDefault(config.enableGlobalChat, "enableGlobalChat", true);
			setOrDefault(config.enableExtendedTax, "enableExtendedTax", true);
			setOrDefault(config.enableTreasureBottlesFix, "enableTreasureBottlesFix", true);
			setOrDefault(config.enablePlayerNotifications, "enablePlayerNotifications", true);
			setOrDefault(config.enableOldStructureChecks, "enableOldStructureChecks", true);
			setOrDefault(config.enableUnlimitedRespecs, "enableUnlimitedRespecs", true);
			setOrDefault(config.globalChatWebhookUrl, "globalChatWebhookUrl", std::string(""));
			setOrDefault(config.treasureQualityBoostMax, "treasureQualityBoostMax", 78.0f);
			setOrDefault(config.treasureQualityBoostRamp, "treasureQualityBoostRamp", 8.0f);
			setOrDefault(config.treasureRetries, "treasureRetries", 3);
			setOrDefault(config.tresureRegionLock, "tresureRegionLock", true);
			setOrDefault(config.treasurePickDelayMs, "treasurePickDelayMs", 250);

			if (needSave) {
				Save(configPath);
			}
			Log::GetLog()->info("[Config] Configuration loaded from {}", configPath);
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Config] Error loading config: {}", e.what());
			std::exit(EXIT_FAILURE);
		}
	}

	void Save(const std::string& configPath = "AtlasGlue.json") {
		try {
			std::filesystem::create_directories("config");
			std::ofstream file(configPath);
			nlohmann::json j;
			j["enableDetailedLogging"] = config.enableDetailedLogging;
			j["enableGlobalChat"] = config.enableGlobalChat;
			j["enableExtendedTax"] = config.enableExtendedTax;
			j["enableTreasureBottlesFix"] = config.enableTreasureBottlesFix;
			j["enablePlayerNotifications"] = config.enablePlayerNotifications;
			j["enableOldStructureChecks"] = config.enableOldStructureChecks;
			j["enableUnlimitedRespecs"] = config.enableUnlimitedRespecs;
			j["globalChatWebhookUrl"] = config.globalChatWebhookUrl;
			j["treasureQualityBoostMax"] = config.treasureQualityBoostMax;
			j["treasureQualityBoostRamp"] = config.treasureQualityBoostRamp;
			j["treasureRetries"] = config.treasureRetries;
			j["treasurePickDelayMs"] = config.treasurePickDelayMs;
			j["tresureRegionLock"] = config.tresureRegionLock;

			file << std::setw(4) << j;
			file.close();
			Log::GetLog()->info("[Config] Configuration saved to {}", configPath);
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Config] Error saving config: {}", e.what());
		}
	}

	// Direct access to config struct
	PluginConfig config;

private:
	Config() = default;
	~Config() = default;
	Config(const Config&) = delete;
	Config& operator=(const Config&) = delete;
};