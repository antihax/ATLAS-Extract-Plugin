#include <iostream>
#include <fstream>
#include <sstream>
#include <API/Atlas/Atlas.h>
#include <unordered_map>
#include <regex>
#include <filesystem>
#include "json.hpp"
#include "convexHull.h"

#pragma comment(lib, "AtlasApi.lib")

DECLARE_HOOK(AShooterGameMode_BeginPlay, void, AShooterGameMode*, float);
DECLARE_HOOK(AShooterGameMode_InitOptions, void, AShooterGameMode*, FString);

DECLARE_HOOK(AShooterGameMode_InitOptionString, void, AShooterGameMode*, FString, FString, FString);
DECLARE_HOOK(AShooterGameMode_InitOptionInteger, void, AShooterGameMode*, FString, FString, FString, int);
DECLARE_HOOK(AShooterGameMode_InitOptionFloat, void, AShooterGameMode*, FString, FString, FString, float);
DECLARE_HOOK(AShooterGameMode_InitOptionBool, void, AShooterGameMode*, FString, FString, FString, bool);

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

// Remove things we do not care about.
FString fixNPCName(FString npcname) {
	npcname = npcname.Replace(L"Default__", L"");
	npcname = npcname.Replace(L"_Character_BP_Trench", L"");
	npcname = npcname.Replace(L"_Character_BP_Cave", L"");
	npcname = npcname.Replace(L"_Character_BP_C", L"");
	npcname = npcname.Replace(L"FreshwaterFish_", L"");
	npcname = npcname.Replace(L"SaltwaterFish_", L"");
	npcname = npcname.Replace(L"Wild", L"");
	npcname = npcname.Replace(L"Atlas", L"");
	npcname = npcname.Replace(L"GiantSnake", L"Cobra");
	npcname = npcname.Replace(L"Giant", L"");
	npcname = npcname.Replace(L"hild_C", L"");
	npcname = npcname.Replace(L"_C", L"");
	npcname = npcname.TrimStartAndEnd();
	return npcname;
}

void dumpCraftables() {
	nlohmann::json json;
	json["Craftables"] = nullptr;
	Log::GetLog()->info("Dump Craftables");

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

void dumpStructures() {
	nlohmann::json json;
	json["Structures"] = nullptr;
	Log::GetLog()->info("Dump Structures");
	// Get all blueprint crafting requirements
	TArray<UObject*> types;
	Globals::GetObjectsOfClass(APrimalStructure::GetPrivateStaticClass(NULL), &types, true, EObjectFlags::RF_NoFlags);
	for (auto object : types) {
		auto n = static_cast<APrimalStructure*> (object);
		FString name;
		n->NameField().ToString(&name);
		if (n->DecayDestructionPeriodMultiplierField() > 1.0f) {
			json["Structures"][name.ToString()]["DecayMultiplier"] = n->DecayDestructionPeriodMultiplierField();
		}
	}
	std::filesystem::create_directory("resources");
	std::ofstream file("resources/structures.json");
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
		json[dz->VolumeName().ToString()] = { gps.X, gps.Y };
	}
	return json;
}

void dumpLootTables() {
	Log::GetLog()->info("Dump Loot Tables");
	TArray<UObject*> objects;
	Globals::GetObjectsOfClass(UPrimalSupplyCrateItemSets::GetPrivateStaticClass(NULL), &objects, true, EObjectFlags::RF_NoFlags);
	for (auto object : objects) {
		Log::GetLog()->info("loot set found ");
		auto n = static_cast<UPrimalSupplyCrateItemSets*> (object);
		for (auto table : n->ItemSetsField()) {
			Log::GetLog()->info("loot set {} ", table.SetName.ToString());
		}
	}
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
		for (auto r : n->HarvestResourceEntriesField()) {
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

//static_cast<const void*>(this);
std::string stringrepresentation(const void* address) {
	std::stringstream ss;
	ss << address;
	return ss.str();
}

void dumpAnimals() {
	nlohmann::json json;
	json["Animals"] = nullptr;
	Log::GetLog()->info("Dump Animals");

	TArray<UObject*> types;
	Globals::GetObjectsOfClass(APrimalDinoCharacter::GetPrivateStaticClass(NULL), &types, true, EObjectFlags::RF_NoFlags);
	for (auto object : types) {
		auto n = static_cast<APrimalDinoCharacter*> (object);
		if (!n)
			continue;
		FString name;
		n->NameField().ToString(&name);

		if (name.Contains("Default__Vulture") || name.Contains("Default__Sheep") || name.Contains("Default__Crab") || name.Contains("Ship") || name.Contains("Dino") || name.Contains("Primal") || name.Contains("PathFollow") || name.Contains("HumanNPC") || name.Contains("Creature") || name.Contains("Galley") || name.Contains("_Raft"))
			continue;

		name = fixNPCName(name);

		for (auto b : n->MatingRequiresBiomeTagsField()) {
			FString bname;
			b.ToString(&bname);
			json["Animals"][name.ToString()]["breedingBiomes"].push_back(bname.ToString());
		}
		json["Animals"][name.ToString()]["minTemperatureToBreed"] = n->MinTemperatureToBreedField();
		json["Animals"][name.ToString()]["maxTemperatureToBreed"] = n->MaxTemperatureToBreedField();
		json["Animals"][name.ToString()]["canBeTamed"] = n->bCanBeTamed().Get();
		json["Animals"][name.ToString()]["tameConsumeInterval"] = n->WakingTameFeedIntervalField();
		json["Animals"][name.ToString()]["deathInventoryChance"] = n->DeathInventoryChanceToUseField();
		json["Animals"][name.ToString()]["babyGestationSpeed"] = n->BabyGestationSpeedField();
		json["Animals"][name.ToString()]["babyGestationMultiplier"] = n->ExtraBabyGestationSpeedMultiplierField();
		json["Animals"][name.ToString()]["babyTripletsChance"] = n->BabyChanceOfTripletsField();
		json["Animals"][name.ToString()]["babyTwinsChance"] = n->BabyChanceOfTwinsField();
		json["Animals"][name.ToString()]["requiredAffinity"] = n->RequiredTameAffinityField();
		json["Animals"][name.ToString()]["requiredAffinityPerBaseLevel"] = n->RequiredTameAffinityPerBaseLevelField();
		/*if (n->DinoSettingsClassField().uClass) {

				Log::GetLog()->info("Server Grid {} ", stringrepresentation(n->DinoSettingsClassField().uClass));
			auto dsc = static_cast<UPrimalDinoSettings*>(n->DinoSettingsClassField().uClass->GetDefaultObject(true));
			if (dsc) {
				for (auto food : dsc->FoodEffectivenessMultipliersField()) {

				}
			}
		}*/

		if (n->GetKillXP())
			json["Animals"][name.ToString()]["XP"] = n->XPEarnMultiplierField() * n->KillXPBaseField();
		json["Animals"][name.ToString()]["XPperHit"] = n->bGiveXPPerHit().Get();


		if (n->DeathHarvestingComponentField().uClass) {
			auto harv = static_cast<UPrimalHarvestingComponent*>(n->DeathHarvestingComponentField().uClass->GetDefaultObject(true));
			if (!harv)
				continue;

			for (auto r : harv->HarvestResourceEntriesField()) {
				TSubclassOf<UPrimalItem> hcSub = r.ResourceItem;
				if (hcSub.uClass) {
					if (hcSub.uClass->ClassDefaultObjectField()) {
						auto pi = static_cast<UPrimalItem*> (hcSub.uClass->ClassDefaultObjectField());
						FString type;
						pi->GetItemName(&type, false, false, NULL);
						json["Animals"][name.ToString()]["resources"].push_back(type.ToString());
					}
				}
			}
		}
	}

	std::ofstream file("resources/animals.json");
	file << std::setw(4) << json;
	file.flush();
	file.close();
}

void extract(float a2) {
	UWorld* World = ArkApi::GetApiUtils().GetWorld();
	auto gameInstance = static_cast<UShooterGameInstance*> (World->OwningGameInstanceField());
	nlohmann::json json;

	Log::GetLog()->info("Server Grid {} ", ServerGrid());
	// Save craftables.
	dumpCraftables();
	dumpStructures();
	dumpLootTables();
	dumpAnimals();

	// Get discovery Zones
	json["Discoveries"] = getDiscoveries(World);
	const auto grid = static_cast<UShooterGameInstance*> (World->OwningGameInstanceField())->GridInfoField();
	const auto server = grid->GetCurrentServerInfo();

	//Log::GetLog()->info("Server Name {} {}", grid->WorldFriendlyNameField().ToString(), server->NameField().ToString());

	TArray<AActor*> found_actors;

	// Loop all actors first time to find overrides and things we are interested in
	std::unordered_map<std::string, UClass*> OverrideClasses;
	UGameplayStatics::GetAllActorsOfClass(reinterpret_cast<UObject*> (World), AActor::GetPrivateStaticClass(NULL), &found_actors);
	for (auto actor : found_actors) {
		FString name;
		actor->GetFullName(&name, NULL);
		if (name.Contains("Biome")) {
			auto gps = ActorGPS(actor);
			auto biome = static_cast<ABiomeZoneVolume*> (actor);
			biome->BeginPlay();
			FString biomeName;
			biome->GetBiomeZoneName(&biomeName, NULL);

			char buff[200];
			snprintf(buff, sizeof(buff), "%.2f:%.2f", gps.X, gps.Y);
			std::string key = buff;
			json["Biomes"][key] = {
				{ "name", biomeName.ToString() },
				{ "gps", {gps.X, gps.Y}},
				{ "temp", {biome->AbsoluteMinTemperature(), biome->AbsoluteMaxTemperature()}},
			};

			for (auto x : biome->BiomeZoneTags()) {
				FString biomeTag;
				x.ToString(&biomeTag);
				//Log::GetLog()->info("	Biome {} ", biomeTag.ToString());
				json["Biomes"][key]["tags"].push_back(biomeTag.ToString());
			}
		}

		if (name.Contains("CreatureSpawnEntries")) {
			auto zm = static_cast<ANPCZoneManager*> (actor);
			auto type = static_cast<UNPCSpawnEntriesContainer*> (zm->NPCSpawnEntriesContainerObjectField().uClass->ClassDefaultObjectField());

			char buff[200];
			nlohmann::json locations;
			bool found;
			for (auto spawnVolume : zm->LinkedZoneSpawnVolumeEntriesField()) {
				if (spawnVolume.LinkedZoneSpawnVolume) {
					found = true;
					auto gps = ActorGPS(spawnVolume.LinkedZoneSpawnVolume);
					snprintf(buff, sizeof(buff), "%.2f:%.2f", gps.X, gps.Y);
					locations.push_back({ gps.X, gps.Y });
				}
			}
			if (found) {
				for (auto npc : type->NPCSpawnEntriesField()) {
					int num = 0;
					nlohmann::json animals;
					for (auto entryClass : npc.NPCsToSpawn) {
						auto e = static_cast<APrimalDinoCharacter*> (entryClass.uClass->ClassDefaultObjectField());
						FString npcname;
						e->NameField().ToString(&npcname);
						if (npcname.Contains("HumanNPC"))
							continue;

						npcname = fixNPCName(npcname);
						//Log::GetLog()->info("animal {}", npcname.ToString());
						nlohmann::json animal;
						animal["name"] = npcname.ToString();

						if (npc.NPCMinLevelOffset.Num() > num)
							animal["minLevelOffset"] = npc.NPCMinLevelOffset[num];
						if (npc.NPCMinLevelOffset.Num() > num)
							animal["maxLevelOffset"] = npc.NPCMaxLevelOffset[num];
						if (npc.NPCMinLevelMultiplier.Num() > num)
							animal["minLevelMultiplier"] = npc.NPCMinLevelMultiplier[num];
						if (npc.NPCMaxLevelMultiplier.Num() > num)
							animal["maxLevelMultiplier"] = npc.NPCMaxLevelMultiplier[num];
						if (npc.NPCsToSpawnPercentageChance.Num() > num && npc.NPCsToSpawnPercentageChance[num] != 1.0f)
							animal["spawnChance"] = npc.NPCsToSpawnPercentageChance[num];
						if (npc.NPCOverrideLevel.Num() > num)
							animal["overrideLevel"] = (int)npc.NPCOverrideLevel[num];

						animals.push_back(animal);
						num++;
					}

					json["Animals"][buff].push_back({
						{ "gps", locations },
						{ "spawnLimits", {zm->MinDesiredNumberOfNPCField(), zm->AbsoluteMaxNumberOfNPCField(), zm->DesiredNumberOfNPCMultiplierField() } },
						{ "levelOffset", zm->ExtraNPCLevelOffsetField() },
						{ "levelLerp", zm->NPCLerpToMaxRandomBaseLevelField() },
						{ "levelMultiplier", zm->NPCLevelMultiplierField() },
						{ "levelMinOveride", zm->ForceOverrideNPCBaseLevelField() },
						{ "islandLevelMultiplier", zm->IslandFinalNPCLevelMultiplierField() },
						{ "islandLevelOffset", zm->IslandFinalNPCLevelOffsetField() },
						{ "animals", animals },
						});
				}
			}
		}

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
			if (json["Boss"][boss.ToString()].is_null())
				json["Boss"][boss.ToString()] = nlohmann::json::array();
			for (auto spawnVolume : zoneManager->LinkedZoneSpawnVolumeEntriesField()) {
				auto gps = ActorGPS(spawnVolume.LinkedZoneSpawnVolume);
				json["Boss"][boss.ToString()].push_back({ gps.X, gps.Y });
			}

		}
		if (name.Contains("OceanEpicNPCZoneManager")) {
			Log::GetLog()->info("Found ocean epic {}", name.ToString());
			auto zoneManager = static_cast<ANPCZoneManager*> (actor);
			for (const auto dino : zoneManager->NPCSpawnEntriesField()) {
				Log::GetLog()->info("Found dino epic {}", name.ToString());
				int count = 0;
				for (const auto npc : dino.NPCsToSpawn) {
					auto type = static_cast<APrimalDinoCharacter*> (npc.uClass->ClassDefaultObjectField());
					type->GetFullName(&name, NULL);
					Log::GetLog()->info("animal {}", name.ToString());
					auto gps = VectorGPS(dino.NPCsSpawnOffsets[count]);

					if (name.Contains("MeanWhale_SeaMonster")) {
						if (json["Boss"]["MeanWhale"].is_null())
							json["Boss"]["MeanWhale"] = nlohmann::json::array();
						json["Boss"]["MeanWhale"].push_back({ gps.X, gps.Y });
					}
					else if (name.Contains("GentleWhale_SeaMonster")) {
						if (json["Boss"]["GentleWhale"].is_null())
						json["Boss"]["GentleWhale"] = nlohmann::json::array();
						json["Boss"]["GentleWhale"].push_back({ gps.X, gps.Y });
					}
					else if (name.Contains("Squid_Character")) {
						if (json["Boss"]["GiantSquid"].is_null())
						json["Boss"]["GiantSquid"] = nlohmann::json::array();
						json["Boss"]["GiantSquid"].push_back({ gps.X, gps.Y });
					}

					count++;
				}
			}
			continue;
		}
		else if (name.Contains("SnowCaveBossManager")) {
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
			dz->BeginPlay(a2);
			for (auto o : dz->FoliageAttachmentOverrides()) {
				FString name;
				o.ForFoliageTypeName.ToString(&name);
				//Log::GetLog()->info("override {}", name.ToString());
				OverrideClasses[island + "_" + name.ToString()] = o.OverrideActorComponent.uClass;
			}
			for (auto o : dz->FoliageOverrideMap()) {
				FString key, value;

				o.Key.ToString(&key);
				//Log::GetLog()->info("override map {} {}", key.ToString(), stringrepresentation(o.Value.uClass));
				OverrideClasses[island + "_" + key.ToString()] = o.Value.uClass;
			}
		}
	}
	found_actors.Empty();

	// Find actual resources, maps, and meshes
	std::unordered_map<UClass*, std::vector<std::string>> harvestableClasses = getHarvestableClasses();
	std::unordered_map<std::string, std::unordered_map<std::string, int>> resources;
	std::unordered_map<std::string, std::unordered_map<std::string, int>> assets;
	std::unordered_map<std::string, std::vector<std::vector<float>>> maps;
	std::unordered_map<std::string, std::vector<std::string>> meshes;

	nlohmann::json resourceNodes;
	nlohmann::json assetNodes;

	TArray<UObject*> objects;
	Globals::GetObjectsOfClass(UInstancedStaticMeshComponent::GetPrivateStaticClass(NULL), &objects, true, EObjectFlags::RF_NoFlags);
	for (auto object : objects) {
		auto n = reinterpret_cast<UInstancedStaticMeshComponent*> (object);

		FString name;
		object->GetPathName(&name, NULL);
		if (n) {
			// Get the override key
			auto level = n->GetComponentLevel();
			FString lvlname;
			level->GetFullName(&lvlname, NULL);
			std::string island = GetIslandName(lvlname.ToString());
			std::string overrideSettings = island + "_" + name.ToString();

			// Get the base and override with any applicable resources
			auto nSub = n->AttachedComponentClassField().uClass;
			if (OverrideClasses.find(overrideSettings) != OverrideClasses.end()) {
				nSub = OverrideClasses[overrideSettings];
			}

			auto u = n->FoliageTypeReferenceField();
			if (u) {
				u->NameField().ToString(&name);
				TSubclassOf<UActorComponent> result;
				gameInstance->GetOverridenFoliageAttachment(&result, n->GetComponentLevel(), u);
				nSub = result.uClass;
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

					// map key
					char buff[200];
					snprintf(buff, sizeof(buff), "%.2f:%.2f", loc.X, loc.Y);
					std::string key = buff;
					FString assetName;
					// find asset names
					if (u) {
						u->NameField().ToString(&name);
						if (n->StaticMeshField()) {
							n->StaticMeshField()->NameField().ToString(&assetName);
							assets[key][assetName.ToString()] += count;
						}
					}

					// count resources
					for (auto r : rs) {
						resources[key][r] += count;
					}

					// save location of nodes
					for (int i = 0; i < count; i++) {
						FVector x;
						n->GetPositionOfInstance(&x, i);
						auto g = VectorGPS(x);
						resourceNodes[island][nodes].push_back(std::make_pair(g.X, g.Y));
						if (!assetName.IsEmpty())
							assetNodes[island][assetName.ToString()].push_back(std::make_pair(g.X, g.Y));
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
			for (auto point : points) {
				json["DetailedResources"][island.key()][node.key()].push_back({ std::get<0>(point), std::get<1>(point) });
			}
		}
	}

	for (auto island : assetNodes.items()) {
		for (auto node : assetNodes[island.key()].items()) {
			std::vector<point> points = assetNodes[island.key()][node.key()];
			// Find areas of resources
			auto hull = convexHull(points);
			for (auto point : hull) {
				json["DetailedAssets"][island.key()][node.key()].push_back({ std::get<0>(point), std::get<1>(point) });
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

			sc->BeginPlay(a2);
			resources[key]["Maps"] += sc->MaxNumCratesField();

			for (auto se : sc->LinkedSupplyCrateEntriesField()) {
				maps[key].push_back({ se.EntryWeight, se.OverrideCrateValues.RandomQualityMultiplierMin, se.OverrideCrateValues.RandomQualityMultiplierMax, se.OverrideCrateValues.RandomQualityMultiplierPower });
			}
		}
	}
	found_actors.Empty();

	// Add to the json object
	json["Resources"] = nullptr;
	json["Assets"] = nullptr;
	json["Maps"] = nullptr;
	json["Meshes"] = nullptr;

	for (auto location : resources) {
		json["Resources"][location.first] = nullptr;
		for (auto resource : location.second) {
			json["Resources"][location.first][resource.first] = resource.second;
		}
	}

	for (auto location : assets) {
		json["Assets"][location.first] = nullptr;
		for (auto resource : location.second) {
			json["Assets"][location.first][resource.first] = resource.second;
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
	file << json.dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore);
	file.flush();
	file.close();
}

void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* a_shooter_game_mode, float a2) {
	std::filesystem::create_directory("resources");
	AShooterGameMode_BeginPlay_original(a_shooter_game_mode, a2);

	extract(a2);
	exit(0);
}

void Hook_AShooterGameMode_InitOptions(AShooterGameMode* This, FString Options) {
	AShooterGameMode_InitOptions_original(This, Options);
}

void Hook_AShooterGameMode_InitOptionString(AShooterGameMode* This, FString CommandLine, FString Section, FString Option) {
	AShooterGameMode_InitOptionString_original(This, CommandLine, Section, Option);
}

void Hook_AShooterGameMode_InitOptionInteger(AShooterGameMode* This, FString CommandLine, FString Section, FString Option, int CurrentValue) {
	AShooterGameMode_InitOptionInteger_original(This, CommandLine, Section, Option, CurrentValue);
}

void Hook_AShooterGameMode_InitOptionFloat(AShooterGameMode* This, FString CommandLine, FString Section, FString Option, float CurrentValue) {
	AShooterGameMode_InitOptionFloat_original(This, CommandLine, Section, Option, CurrentValue);
}

void Hook_AShooterGameMode_InitOptionBool(AShooterGameMode* This, FString CommandLine, FString Section, FString Option, bool CurrentValue) {
	AShooterGameMode_InitOptionBool_original(This, CommandLine, Section, Option, CurrentValue);
}

void Load() {
	Log::Get().Init("DumpResources");
	ArkApi::GetHooks().SetHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay, &AShooterGameMode_BeginPlay_original);
	ArkApi::GetHooks().SetHook("AShooterGameMode.InitOptions", &Hook_AShooterGameMode_InitOptions, &AShooterGameMode_InitOptions_original);
	ArkApi::GetHooks().SetHook("AShooterGameMode.InitOptionString", &Hook_AShooterGameMode_InitOptionString, &AShooterGameMode_InitOptionString_original);
	ArkApi::GetHooks().SetHook("AShooterGameMode.InitOptionInteger", &Hook_AShooterGameMode_InitOptionInteger, &AShooterGameMode_InitOptionInteger_original);
	ArkApi::GetHooks().SetHook("AShooterGameMode.InitOptionFloat", &Hook_AShooterGameMode_InitOptionFloat, &AShooterGameMode_InitOptionFloat_original);
	ArkApi::GetHooks().SetHook("AShooterGameMode.InitOptionBool", &Hook_AShooterGameMode_InitOptionBool, &AShooterGameMode_InitOptionBool_original);
}

void Unload() {
	ArkApi::GetHooks().DisableHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay);
	ArkApi::GetHooks().DisableHook("AShooterGameMode.InitOptions", &Hook_AShooterGameMode_InitOptions);
	ArkApi::GetHooks().DisableHook("AShooterGameMode.InitOptionString", &Hook_AShooterGameMode_InitOptionString);
	ArkApi::GetHooks().DisableHook("AShooterGameMode.InitOptionInteger", &Hook_AShooterGameMode_InitOptionInteger);
	ArkApi::GetHooks().DisableHook("AShooterGameMode.InitOptionFloat", &Hook_AShooterGameMode_InitOptionFloat);
	ArkApi::GetHooks().DisableHook("AShooterGameMode.InitOptionBool", &Hook_AShooterGameMode_InitOptionBool);
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