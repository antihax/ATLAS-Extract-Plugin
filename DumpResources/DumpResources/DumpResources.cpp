#include <iostream>
#include <fstream>
#include <API/Atlas/Atlas.h>
#include <vector>
#include <unordered_map>
#include <regex>
#include <filesystem>
#include "json.hpp"

#pragma comment(lib, "AtlasApi.lib")

DECLARE_HOOK(AShooterGameMode_BeginPlay, void, AShooterGameMode*);


static const std::string ServerGrid() {
	// Get server grid 
	const auto grid = static_cast<UShooterGameInstance*> (ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->GridInfoField();
	const char x[15] = { 'A', 'B', 'C', 'D' , 'E' , 'F' , 'G' , 'H' , 'I' , 'J' , 'K' , 'L' , 'M' , 'N', 'O' };
	const char* y[15] = { "1", "2", "3", "4" , "5" , "6" , "7" , "8" , "9" , "10" , "11" , "12" , "13" , "14", "15" };

	return std::string(1, x[grid->GetCurrentServerInfo()->gridXField()]) + std::string(y[grid->GetCurrentServerInfo()->gridYField()]);
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
		if (name.Contains(FString("incorrect primalitem")) || name.StartsWith("Base"))
			continue;
		for (auto res : n->BaseCraftingResourceRequirementsField()) {
			if (res.ResourceItemType.uClass) {
				if (res.ResourceItemType.uClass->ClassDefaultObjectField()) {
					auto pi = static_cast<UPrimalItem*> (res.ResourceItemType.uClass->ClassDefaultObjectField());
					FString type;
					pi->GetItemName(&type, false, true, NULL);
					type = fixName(type);
					json["Craftables"][name.ToString()][type.ToString()] = res.BaseResourceRequirement;
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

void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* a_shooter_game_mode) {
	AShooterGameMode_BeginPlay_original(a_shooter_game_mode);

	dumpCraftables();

	UWorld* World = ArkApi::GetApiUtils().GetWorld();

	nlohmann::json json;
	json["Discoveries"] = nullptr;
	json["Stones"] = nullptr;

	// Get all Discoveries
	TArray<AActor*> found_actors;
	UGameplayStatics::GetAllActorsOfClass(reinterpret_cast<UObject*> (World), ADiscoveryZone::GetPrivateStaticClass(NULL), &found_actors);
	for (auto actor : found_actors) {
		auto dz = static_cast<ADiscoveryZone*> (actor);
		auto gps = ActorGPS(dz);
		json["Discoveries"][dz->VolumeName().ToString()] = { gps.X, gps.Y };
	}
	found_actors.Empty();

	// Build a map of all override resources and their resulting class
	std::unordered_map<std::string, UClass*> OverrideClasses;
	UGameplayStatics::GetAllActorsOfClass(reinterpret_cast<UObject*> (World), AActor::GetPrivateStaticClass(NULL), &found_actors);
	for (auto actor : found_actors) {
		FString name;
		actor->GetFullName(&name, NULL);
		Log::GetLog()->info("{}", name.ToString());

		/*if (name.Contains("Biome_C")) {
			auto bz = reinterpret_cast<ABiomeZoneVolume*> (actor);
			auto gps = ActorGPS(bz);
			Log::GetLog()->info("{}\t{}\t{}", name.ToString(), gps.X, gps.Y);
		}*/
			
		// Find the power stones
		if (name.Contains("PowerStoneStation_BP_C")) {
			auto gps = ActorGPS(actor);
			FString s = GetStoneIndexFromActor(actor);
			if (name.Contains("Secondary")) {
				if (name.EndsWith(s)) {
					json["Stones"]["Essence " + s.ToString()] = { gps.X, gps.Y };
				}
			} else {
				json["Stones"]["Stone " + s.ToString()] = { gps.X, gps.Y };
			}
		}
		
		if (name.Contains("FoliageOverride")) {
			//Log::GetLog()->info("Overrides {}", name.ToString());
			std::string island = GetIslandName(name.ToString());
			auto dz = reinterpret_cast<AFoliageAttachmentOverrideVolume*> (actor);
			for (auto oxr : dz->FoliageAttachmentOverrides()) {
				FString name;
				oxr.ForFoliageTypeName.ToString(&name);
				//Log::GetLog()->info("\t{}", name.ToString());
				OverrideClasses[island + "_" + name.ToString()] = oxr.OverrideActorComponent.uClass;
			}
		}
	}
	found_actors.Empty();

	// Build map of harvestable classes
	TArray<UObject*> objects;
	std::unordered_map<UClass*, std::vector<std::string>> harvestableClasses;

	Globals::GetObjectsOfClass(UPrimalHarvestingComponent::GetPrivateStaticClass(NULL), &objects, true, EObjectFlags::RF_NoFlags);
	for (auto object : objects) {
		auto n = static_cast<UPrimalHarvestingComponent*> (object);
		FString name;
		n->GetFullName(&name, NULL);
		//Log::GetLog()->info("Harvest {}", name.ToString());

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
					//Log::GetLog()->info("\t{}\t{}", name.ToString(), type.ToString());
					harvestableClasses[n->ClassField()].push_back(type.ToString());
				}
				else {
					// Add resource to the list
					FString type;
					hcSub.uClass->GetDescription(&type);
					if (name.StartsWith("StoneHarvestComponent"))
						type.Append(" (Rock)");
					//Log::GetLog()->info("\t{}\t{}", name.ToString(), type.ToString());
					harvestableClasses[n->ClassField()].push_back(type.ToString());
				}
			}
		}
	}
	objects.Empty();

	// Get all resource node spawners
	std::unordered_map<std::string, std::unordered_map<std::string, int>> resources;
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

			// If we actually found a subclass, process the resource
			if (nSub) {
				// Get GPS Coords
				FVector vec;
				n->GetWorldLocation(&vec);
				const FVector2D loc = VectorGPS(vec);

				// Add all the harvestable classes
				auto rs = harvestableClasses[nSub];
				for (auto r : rs) {
					char buff[200];
					snprintf(buff, sizeof(buff), "%.2f:%.2f", loc.X, loc.Y);
					std::string key = buff;
					resources[key][r] += 1;
				}
			}
		}
	}
	objects.Empty();

	UGameplayStatics::GetAllActorsOfClass(reinterpret_cast<UObject*> (World),
		AActor::GetPrivateStaticClass(NULL), &found_actors);
	for (auto actor : found_actors) {
		FString name;
		actor->GetFullName(&name, NULL);

		if (name.Contains("BP_Lake")) {
			auto loc = ActorGPS(actor);
			char buff[200];
			snprintf(buff, sizeof(buff), "%.2f:%.2f", loc.X, loc.Y);
			std::string key = buff;
			resources[key]["FreshWater"]=1;
		}

		if (name.Contains("TreasureBottle")) {
			auto sc = reinterpret_cast<ASupplyCrateSpawningVolume*> (actor);
			auto loc = ActorGPS(sc);
			char buff[200];
			snprintf(buff, sizeof(buff), "%.2f:%.2f", loc.X, loc.Y);
			std::string key = buff;
			resources[key]["Maps"] = sc->MaxNumCratesField();
		}
	}
	found_actors.Empty();

	// Add to the json object
	json["Resources"] = nullptr;
	for (auto location : resources) {
		json["Resources"][location.first] = nullptr;
		for (auto resource : location.second) {
			json["Resources"][location.first][resource.first] = resource.second;
		}
	}

	std::filesystem::create_directory("resources");
	std::ofstream file("resources/" + ServerGrid() + ".json");
	file << json;
	file.flush();
	file.close();

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