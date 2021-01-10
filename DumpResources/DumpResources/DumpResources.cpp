#include <iostream>
#include <fstream>
#include <API/Atlas/Atlas.h>
#include <unordered_map>
#include <regex>
#include <filesystem>
#include "json.hpp"
#include "convexHull.h"

#pragma comment(lib, "AtlasApi.lib")

DECLARE_HOOK(AShooterGameMode_BeginPlay, void, AShooterGameMode*);

static const std::string ServerGrid() {
	// Get server grid 
	const auto grid = static_cast<UShooterGameInstance*> (ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->GridInfoField();
	const char* x[15] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O" };
	const char* y[15] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15" };

	std::string gridStr = x[grid->GetCurrentServerInfo()->gridXField()];
	gridStr += y[grid->GetCurrentServerInfo()->gridYField()];
	return gridStr;
};

FString GetStoneIndexFromActor(AActor* a)
{
	int island;
	TArray<FString> islandData1;
	TArray<FString> islandData2;
	UVictoryCore::GetIslandCustomDatas(a, &island, &islandData1, &islandData2);

	int findVar = 0;
	for (auto s : islandData1) {
		if (s.Equals("PowerStoneIndex")) {
			return islandData2[findVar];
		}
		findVar++;
	}
	return FString("");
}

static FVector2D VectorGPS(FVector loc) {
	// Get server grid 
	const auto grid = static_cast<UShooterGameInstance*> (ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->GridInfoField();

	// Convert to GPS coords
	FVector2D gps;
	grid->ServerLocationToGPSLocation(&gps, grid->GetCurrentServerId(), loc);

	return gps;
};

static FVector2D ActorGPS(AActor* a) {
	// Get actor location
	return VectorGPS(a->RootComponentField()->RelativeLocationField());
};

static std::string GetIslandName(std::string s) {
	transform(s.begin(), s.end(), s.begin(), ::tolower);
	const std::regex rgx("([a-z]{2,9}_[a-z]{1,3})_[a-z0-9_]{1,50}(-[0-9]{1,5})");

	std::smatch matches;
	std::regex_search(s, matches, rgx);

	if (matches.size() < 3) {
		return "";
	}

	return matches[1].str() + matches[2].str();
}

// Remove things we do not care about.
FString fixName(FString name) {
	name = name.Replace(L"Common ", L"");
	name = name.Replace(L" (Broken)", L"");
	name = name.TrimStartAndEnd();
	return name;
}

void dumpCraftables() {
	nlohmann::json json;
	json["Craftables"] = nullptr;

	// Get all blueprint crafting requirements
	TArray<UObject*> types;
	Globals::GetObjectsOfClass(UPrimalItem::GetPrivateStaticClass(NULL), &types, true, EObjectFlags::RF_NoFlags);
	for (auto object : types) {
		auto n = static_cast<UPrimalItem*> (object);
		FString name;
		n->GetItemName(&name, false, true, NULL);
		name = fixName(name);

		// Ignore some of the trash items
		if (name.Contains(FString("incorrect primalitem")) || name.StartsWith("Base"))
			continue;
		for (auto res : n->BaseCraftingResourceRequirementsField()) {
			if (res.ResourceItemType.uClass) {
				if (res.ResourceItemType.uClass->ClassDefaultObjectField()) {
					auto pi = static_cast<UPrimalItem*> (res.ResourceItemType.uClass->ClassDefaultObjectField());
					FString type;
					pi->GetItemName(&type, false, true, NULL);
					type = fixName(type);
					json["Craftables"][name.ToString()]["Ingredients"][type.ToString()] = res.BaseResourceRequirement;
				}
			}
		}
	}
	std::filesystem::create_directory("resources");
	std::ofstream file("resources/craftables.json");
	file << json;
	file.flush();
	file.close();
}


nlohmann::json getDiscoveries(UWorld* World) {
	nlohmann::json json;
	// Get all Discoveries
	TArray<AActor*> found_actors;
	UGameplayStatics::GetAllActorsOfClass(reinterpret_cast<UObject*> (World), ADiscoveryZone::GetPrivateStaticClass(NULL), &found_actors);
	for (auto actor : found_actors) {
		auto dz = static_cast<ADiscoveryZone*> (actor);
		auto gps = ActorGPS(dz);
		json["Discoveries"][dz->VolumeName().ToString()] = { gps.X, gps.Y };
	}
	return json;
}

// Build map of harvestable classes
std::unordered_map<UClass*, std::vector<std::string>> getHarvestableClasses() {
	TArray<UObject*> objects;
	std::unordered_map<UClass*, std::vector<std::string>> harvestableClasses;
	Globals::GetObjectsOfClass(UPrimalHarvestingComponent::GetPrivateStaticClass(NULL), &objects, true, EObjectFlags::RF_NoFlags);
	for (auto object : objects) {
		auto n = static_cast<UPrimalHarvestingComponent*> (object);
		FString name;
		n->GetFullName(&name, NULL);

		// Find all the resources provided by the component
		for (auto r : n->HarvestResourceEntries()) {
			TSubclassOf<UPrimalItem> hcSub = r.ResourceItem;
			if (hcSub.uClass) {
				if (hcSub.uClass->ClassDefaultObjectField()) {
					auto pi = static_cast<UPrimalItem*> (hcSub.uClass->ClassDefaultObjectField());
					FString type;
					pi->GetItemName(&type, false, false, NULL);
					if (name.StartsWith("StoneHarvestComponent"))
						type.Append(" (Rock)");
					harvestableClasses[n->ClassField()].push_back(type.ToString());
				}
				else {
					// Add resource to the list
					FString type;
					hcSub.uClass->GetDescription(&type);
					if (name.StartsWith("StoneHarvestComponent"))
						type.Append(" (Rock)");
					harvestableClasses[n->ClassField()].push_back(type.ToString());
				}
			}
		}
	}
	objects.Empty();
	return harvestableClasses;
}

void extract() {
	UWorld* World = ArkApi::GetApiUtils().GetWorld();
	nlohmann::json json;

	// Save craftables.
	dumpCraftables();
	
	// Get discovery Zones
	json["Discoveries"] = getDiscoveries(World);
	
	TArray<AActor*> found_actors;

	// Loop all actors first time to find overrides and things we are interested in
	std::unordered_map<std::string, UClass*> OverrideClasses;
	UGameplayStatics::GetAllActorsOfClass(reinterpret_cast<UObject*> (World), AActor::GetPrivateStaticClass(NULL), &found_actors);
	for (auto actor : found_actors) {
		FString name;
		actor->GetFullName(&name, NULL);

		// BOSSES
		// Look for boss managers & entries
		if (name.Contains("CreatureSpawnEntries_Boss")) {
			// Some islands have remapped bosses, so get the container name instead.
			auto zoneManager = static_cast<ANPCZoneManager*> (actor);
			auto type = static_cast<UNPCSpawnEntriesContainer*> (zoneManager->NPCSpawnEntriesContainerObjectField().uClass->ClassDefaultObjectField());
			type->GetFullName(&name, NULL);

			FString boss = "Unknown";
			if (name.Contains("Dragon")) {
				boss = "Drake";
			}
			else if (name.Contains("Hydra")) {
				boss = "Hydra";
			}
			json["Boss"][boss.ToString()] = nlohmann::json::array();
			for (auto spawnVolume : zoneManager->LinkedZoneSpawnVolumeEntriesField()) {
				auto gps = ActorGPS(spawnVolume.LinkedZoneSpawnVolume);
				json["Boss"][boss.ToString()].push_back({ gps.X, gps.Y });
			}
			continue;
		} else if (name.Contains("OceanEpicNPCZoneManager")) {
			auto zoneManager = static_cast<ANPCZoneManager*> (actor);
			for (const auto dino : zoneManager->NPCSpawnEntriesField()) {
				int count = 0;
				for (const auto npc : dino.NPCsToSpawn) {
					auto type = static_cast<APrimalDinoCharacter*> (npc.uClass->ClassDefaultObjectField());
					type->GetFullName(&name, NULL);
					auto gps = VectorGPS(dino.NPCsSpawnOffsets[count]);

					if (name.Contains("MeanWhale_SeaMonster")) {
						json["Boss"]["MeanWhale"] = nlohmann::json::array();
						json["Boss"]["MeanWhale"].push_back({ gps.X, gps.Y });
					}
					else if (name.Contains("GentleWhale_SeaMonster")) {
						json["Boss"]["GentleWhale"] = nlohmann::json::array();
						json["Boss"]["GentleWhale"].push_back({ gps.X, gps.Y });
					}
					else if (name.Contains("Squid_Character")) {
						json["Boss"]["GiantSquid"] = nlohmann::json::array();
						json["Boss"]["GiantSquid"].push_back({ gps.X, gps.Y });
					}

					count++;
				}
			}
			continue;
		} else if (name.Contains("SnowCaveBossManager")) {
			auto gps = ActorGPS(actor);
			json["Boss"]["Yeti"] = nlohmann::json::array();
			json["Boss"]["Yeti"].push_back({ gps.X, gps.Y });
			continue;
		}

		// POWERSTONE AND ESSENCE
		// Find the power stones
		if (name.Contains("PowerStoneStation_BP_C")) {
			auto gps = ActorGPS(actor);
			FString s = GetStoneIndexFromActor(actor);
			if (name.Contains("Secondary")) {
				if (name.EndsWith(s)) {
					json["Stones"]["Essence " + s.ToString()] = { gps.X, gps.Y };
				}
			}
			else {
				json["Stones"]["Stone " + s.ToString()] = { gps.X, gps.Y };
			}
			continue;
		}

		// RESOURCE OVERRIDES
		// Build list of class overrides for foliage
		if (name.Contains("FoliageOverride")) {
			std::string island = GetIslandName(name.ToString());
			auto dz = reinterpret_cast<AFoliageAttachmentOverrideVolume*> (actor);
			for (auto oxr : dz->FoliageAttachmentOverrides()) {
				FString name;
				oxr.ForFoliageTypeName.ToString(&name);
				OverrideClasses[island + "_" + name.ToString()] = oxr.OverrideActorComponent.uClass;
			}
		}
	}
	found_actors.Empty();

	// Find actual resources, maps, and meshes
	std::unordered_map<UClass*, std::vector<std::string>> harvestableClasses = getHarvestableClasses();
	std::unordered_map<std::string, std::unordered_map<std::string, int>> resources;
	std::unordered_map<std::string, std::vector<std::vector<float>>> maps;
	std::unordered_map<std::string, std::vector<std::string>> meshes;

	nlohmann::json resourceNodes;

	TArray<UObject*> objects;
	Globals::GetObjectsOfClass(UInstancedStaticMeshComponent::GetPrivateStaticClass(NULL), &objects, true, EObjectFlags::RF_NoFlags);
	for (auto object : objects) {
		auto n = reinterpret_cast<UInstancedStaticMeshComponent*> (object);

		FString name;
		object->GetPathName(&name, NULL);
		if (n) {
			auto u = n->FoliageTypeReference();
			if (u)
				u->NameField().ToString(&name);

			auto level = n->GetComponentLevel();
			// Get the override key
			FString lvlname;
			level->GetFullName(&lvlname, NULL);
			std::string island = GetIslandName(lvlname.ToString());
			std::string overrideSettings = island + "_" + name.ToString();

			// Get the base and override with any applicable resources
			auto nSub = n->AttachedComponentClass().uClass;
			if (OverrideClasses.find(overrideSettings) != OverrideClasses.end()) {
				nSub = OverrideClasses[overrideSettings];
			}

			if (nSub) {
				// Get GPS Coords
				FVector vec;
				n->GetWorldLocation(&vec);
				const FVector2D loc = VectorGPS(vec);

				auto count = n->GetInstanceCount();
				// Don't add 0 counts
				if (count > 0) {
					// Add all the harvestable classes
					auto rs = harvestableClasses[nSub];
					std::string nodes = std::accumulate(std::begin(rs), std::end(rs), std::string(), [](std::string& ss, std::string& s) { return ss.empty() ? s : ss + ", " + s; });

					for (auto r : rs) {
						char buff[200];
						snprintf(buff, sizeof(buff), "%.2f:%.2f", loc.X, loc.Y);
						std::string key = buff;
						resources[key][r] += count;
					}

					for (int i = 0; i < count; i++) {
						FVector x;
						n->GetPositionOfInstance(&x, i);
						auto g = VectorGPS(x);
						resourceNodes[island][nodes].push_back(std::make_pair(g.X, g.Y));
					}
				}
			}
		}
	}
	objects.Empty();

	for (auto island : resourceNodes.items()) {
		for (auto node : resourceNodes[island.key()].items()) {
			std::vector<point> points = resourceNodes[island.key()][node.key()];
			// Find areas of resources
			auto hull = convexHull(points);
			for (auto point : hull) {
				json["DetailedResources"][island.key()][node.key()].push_back({ std::get<0>(point), std::get<1>(point) });
			}
		}
	}

	UGameplayStatics::GetAllActorsOfClass(reinterpret_cast<UObject*> (World),
		AActor::GetPrivateStaticClass(NULL), &found_actors);
	for (auto actor : found_actors) {
		FString name;
		actor->GetFullName(&name, NULL);

		if (
			name.Contains("HierarchicalInstancedStaticMeshActor") &&
			!name.Contains("Rock") &&
			!name.Contains("Boulder") &&
			!name.Contains("Capture_Point")
			) {
			auto loc = ActorGPS(actor);
			char buff[200];
			snprintf(buff, sizeof(buff), "%.2f:%.2f", loc.X, loc.Y);
			std::string key = buff;
			meshes[key].push_back(name.Mid(name.Find(".") + 1).ToString());
		}

		if (name.Contains("BP_Lake")) {
			auto loc = ActorGPS(actor);
			char buff[200];
			snprintf(buff, sizeof(buff), "%.2f:%.2f", loc.X, loc.Y);
			std::string key = buff;
			resources[key]["FreshWater"] = 1;
		}

		if (name.Contains("TreasureBottle")) {
			auto sc = reinterpret_cast<ASupplyCrateSpawningVolume*> (actor);
			auto loc = ActorGPS(sc);
			char buff[200];
			snprintf(buff, sizeof(buff), "%.2f:%.2f", loc.X, loc.Y);
			std::string key = buff;
			resources[key]["Maps"] = sc->MaxNumCratesField();
			Log::GetLog()->info("{} ", key);
			for (auto se : sc->LinkedSupplyCrateEntriesField()) {
				maps[key].push_back({ se.EntryWeight, se.OverrideCrateValues.RandomQualityMultiplierMin, se.OverrideCrateValues.RandomQualityMultiplierMax, se.OverrideCrateValues.RandomQualityMultiplierPower });
			}
		}
	}
	found_actors.Empty();

	// Add to the json object
	json["Resources"] = nullptr;
	json["Maps"] = nullptr;
	json["Meshes"] = nullptr;
	for (auto location : resources) {
		json["Resources"][location.first] = nullptr;
		for (auto resource : location.second) {
			json["Resources"][location.first][resource.first] = resource.second;
		}
	}

	for (auto location : maps) {
		json["Maps"][location.first] = nullptr;
		json["Maps"][location.first] = location.second;
	}

	for (auto location : meshes) {
		json["Meshes"][location.first] = nullptr;
		json["Meshes"][location.first] = location.second;
	}

	std::filesystem::create_directory("resources");
	std::ofstream file("resources/" + ServerGrid() + ".json");
	file << json;
	file.flush();
	file.close();
}

void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* a_shooter_game_mode) {
	AShooterGameMode_BeginPlay_original(a_shooter_game_mode);
	extract();

	exit(0);
}

void Load() {
	Log::Get().Init("DumpResources");
	ArkApi::GetHooks().SetHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay, &AShooterGameMode_BeginPlay_original);
}

void Unload() {
	ArkApi::GetHooks().DisableHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay);
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD ul_reason_for_call, LPVOID /*lpReserved*/) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		Load();
		break;
	case DLL_PROCESS_DETACH:
		Unload();
		break;
	}

	return TRUE;
}