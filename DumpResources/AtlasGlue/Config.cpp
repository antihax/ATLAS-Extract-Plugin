#include "Config.h"
#include <API/Atlas/Atlas.h>

void Config::Load(const std::string& configPath) {
    try {
        std::filesystem::create_directories("config");
        
        std::ifstream file(configPath);
        if (file.is_open()) {
            nlohmann::json j;
            file >> j;
            config = j.get<PluginConfig>();
            Log::GetLog()->info("[Config] Configuration loaded from {}", configPath);
        } else {
            Log::GetLog()->warn("[Config] No config file found at {}, creating default", configPath);
            Save(configPath);
        }
    } catch (const std::exception& e) {
        Log::GetLog()->error("[Config] Error loading config: {}", e.what());
        Save(configPath);
    }
}

void Config::Save(const std::string& configPath) {
    try {
        std::filesystem::create_directories("config");
        std::ofstream file(configPath);
        nlohmann::json j = config;
        
        // Add documentation comments to the JSON
        j["_comments"] = {
            {"heartbeatInterval", "Interval in seconds between Redis heartbeat messages"},
            {"maxConnectionRetries", "Maximum number of times to retry Redis connection"},
            {"retryDelaySeconds", "Delay in seconds between Redis connection retry attempts"},
            {"treasureRetries", "Number of attempts to generate treasure locations on random islands"},
            {"connectionTimeout", "Redis connection timeout in milliseconds"},
            {"enableDetailedLogging", "Enable detailed debug logging for Redis operations"},
            {"enableGlobalChat", "Enable cross-server global chat functionality"},
            {"channelCluster", "Redis channel name for cluster-wide messages"},
            {"channelServer", "Redis channel prefix for server-specific messages"},
            {"qualityBoostMultiplier", "Multiplier for treasure quality boost calculation (0.0 = disabled, 1.0 = normal, >1.0 = increased)"}
        };
        
        file << std::setw(4) << j;
        Log::GetLog()->info("[Config] Configuration saved to {}", configPath);
    } catch (const std::exception& e) {
        Log::GetLog()->error("[Config] Error saving config: {}", e.what());
    }
}


