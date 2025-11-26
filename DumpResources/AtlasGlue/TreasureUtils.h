#pragma once // This program is licensed under the GNU GPL v3.0, except that commercial use is prohibited.
#include <API/Atlas/Atlas.h>
#include <string>
#include <random>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <unordered_set>

// Treasure response aggregation state
struct TreasureCandidate {
	FVector location;
	float quality;
	bool cave;
	unsigned int sender;
};

// Global treasure state (extern declarations)
extern std::mutex g_TreasureMutex;
extern std::unordered_map<int, std::vector<TreasureCandidate>> g_TreasureCandidates;
extern std::unordered_set<int> g_TreasureProcessed;
extern std::mutex g_PickMutex;
extern std::vector<std::pair<int, TreasureCandidate>> g_PendingPicks;

// Treasure-related utility functions
int GetRandomIslandId();
float CalculateQualityBoost(float currentQuality);
void HandleTreasureRequest(unsigned int sender, int requestId, float quality, bool isSpecial, std::string region);
void ScheduleTreasurePick(int requestId);
