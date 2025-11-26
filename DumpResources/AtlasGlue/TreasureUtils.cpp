#include "TreasureUtils.h"
#include "Config.h"
#include "RedisManager.h"
#include <cmath>
#include "json.hpp"

// Global state definitions
std::mutex g_TreasureMutex;
std::unordered_map<int, std::vector<TreasureCandidate>> g_TreasureCandidates;
std::unordered_set<int> g_TreasureProcessed;
std::mutex g_PickMutex;
std::vector<std::pair<int, TreasureCandidate>> g_PendingPicks;

// External references
extern RedisManager* g_Redis;
extern unsigned int GetServerId();
extern std::string GetChannelForServerId(unsigned int id);

const std::unordered_map<std::string, ULevel*> getIslandLevels() {
	std::unordered_map<std::string, ULevel*> levels;
	for (auto level : ArkApi::GetApiUtils().GetWorld()->LevelsField()) {
		if (level->SubLevelGroupUniqueIDField() < 0)
			continue;

		int id = level->SubLevelGroupUniqueIDField();
		FString name;
		level->NameField().ToString(&name);

		std::string idStr = std::to_string(id);
		levels[idStr] = level;
	}
	return levels;
}

int GetRandomIslandId() {
	const auto levels = getIslandLevels();

	if (levels.empty()) {
		return 0;
	}

	// Convert level keys to integers and select randomly
	std::vector<int> validIds;
	for (const auto& level : levels) {
		try {
			int levelId = std::stoi(level.first);
			validIds.push_back(levelId);
		}
		catch (const std::exception& e) {
			Log::GetLog()->warn("[Island] Invalid level ID format: '{}'", level.first);
		}
	}

	if (validIds.empty()) {
		return 0;
	}

	static thread_local std::mt19937 gen(std::random_device{}());
	std::uniform_int_distribution<> dis(0, validIds.size() - 1);

	int selectedId = validIds[dis(gen)];
	return selectedId;
}

float CalculateQualityBoost(float /*currentQuality*/) {
	// Draw an additive boost in [1, treasureQualityBoostMax], skewed toward small values via ramp.
	float maxExtra = Config::GetConfig().treasureQualityBoostMax;
	if (maxExtra < 1.0f) maxExtra = 1.0f;
	const float minExtra = 1.0f;
	float ramp = Config::GetConfig().treasureQualityBoostRamp;
	if (ramp < 0.1f) ramp = 0.1f;

	static thread_local std::mt19937 gen(std::random_device{}());
	static thread_local std::uniform_real_distribution<float> dis(0.0f, 1.0f);
	float u = dis(gen);

	// Power-law toward smaller boosts; near-max is exponentially rarer as ramp grows
	float extra = minExtra + (maxExtra - minExtra) * std::pow(1.0f - u, ramp);
	return extra;
}

void HandleTreasureRequest(unsigned int sender, int requestId, float quality, bool isSpecial, std::string region) {
	if (!ArkApi::GetApiUtils().GetWorld()) return;

	const auto grid = static_cast<UShooterGameInstance*>(ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->GridInfoField();
	if (!grid) return;

	const auto sgm = ArkApi::GetApiUtils().GetShooterGameMode();
	if (!sgm) return;
	const auto tmm = sgm->TreasureMapManagerField();
	if (!tmm) return;

	if (Config::GetConfig().tresureRegionLock) {
		const auto serverInfo = grid->GetCurrentServerInfo();
		FString region_cmp = serverInfo->HiddenAtlasIdField(); // Region Name

		if (region_cmp.ToString() != region) {
			if (Config::GetConfig().enableDetailedLogging)
				Log::GetLog()->warn("[Treasure] Region mismatch sender {}: expected '{}', got '{}'",
					sender, region_cmp.ToString(), region);
			return;
		}
	}

	// Only boost if sender is not this server
	if (Config::GetConfig().treasureQualityBoostMax > 0.01f && sender != GetServerId())
		quality += CalculateQualityBoost(quality);

	for (int tries = Config::GetConfig().treasureRetries; tries > 0; --tries) {
		const int islandId = GetRandomIslandId();
		if (islandId == 0) {
			continue;
		}

		FVector outLocation, mapGlobal;
		float outQuality;
		bool outCave;

		if (tmm->GenerateTreasureLocationOnIsland(islandId, quality, &outLocation, &outCave, &outQuality, nullptr)) {
			grid->ServerLocationToGlobalLocation(&mapGlobal, GetServerId(), outLocation);
			if (Config::GetConfig().enableDetailedLogging) {
				Log::GetLog()->info("[Treasure] Generated treasure for sender {} on island {} at ({}, {}, {}), quality {:.2f}, cave {}",
					sender, islandId, mapGlobal.X, mapGlobal.Y, mapGlobal.Z, outQuality, outCave);
			}
			nlohmann::json response = {
				 {"type", PSM_FetchedTreasureLocation},
				 {"sender", GetServerId()},
				 {"peer", sender},
				 {"cave", outCave},
				 {"quality", outQuality},
				 {"X", mapGlobal.X},
				 {"Y", mapGlobal.Y},
				 {"Z", mapGlobal.Z},
				 {"requestId", requestId},
				 {"raw", {{"bytes", {0}}, {"subtype", nullptr}}}
			};

			g_Redis->PublishMessage(GetChannelForServerId(sender), response);
			return;
		}
	}
	Log::GetLog()->warn("[Treasure] Failed to generate treasure for sender {}", sender);
}

void ScheduleTreasurePick(int requestId) {
	int delay = Config::GetConfig().treasurePickDelayMs;
	if (delay <= 0) delay = 1;
	std::thread([requestId, delay]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(delay));
		try {
			std::vector<TreasureCandidate> candidates;
			{
				std::lock_guard<std::mutex> lg(g_TreasureMutex);
				if (g_TreasureProcessed.count(requestId)) {
					return; // already handled
				}
				auto it = g_TreasureCandidates.find(requestId);
				if (it == g_TreasureCandidates.end() || it->second.empty()) {
					g_TreasureProcessed.insert(requestId);
					return;
				}
				candidates = it->second; // copy
			}

			static thread_local std::mt19937 gen(std::random_device{}());
			std::uniform_int_distribution<size_t> dis(0, candidates.size() - 1);
			const TreasureCandidate pick = candidates[dis(gen)];

			if (Config::GetConfig().enableDetailedLogging)
				Log::GetLog()->info("[Treasure] Picking one of {} responses for request {} from server {}",
					static_cast<int>(candidates.size()), requestId, pick.sender);

			// Enqueue to be processed on game thread or item data is corrupted
			{
				std::lock_guard<std::mutex> ql(g_PickMutex);
				g_PendingPicks.emplace_back(requestId, pick);
			}
		}
		catch (const std::exception& e) {
			Log::GetLog()->error("[Treasure] Error during delayed pick for request {}: {}", requestId, e.what());
		}
		}).detach();
}
