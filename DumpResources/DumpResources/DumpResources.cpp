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

DECLARE_HOOK(UShooterGameInstance_StartGameInstance, void, UShooterGameInstance*);

const char* statNames[18] = { "Health", "Stamina", "Torpidity", "Oxygen", "Food", "Water", "Temperature", "Weight", "MeleeDamageMultiplier", "SpeedMultiplier", "TemperatureFortitude", "CraftingSpeedMultiplier", "VitaminA", "VitaminB", "VitaminC", "VitaminD", "StaminaRegeneration", "MAX" };

// get server packed id
static const unsigned int ServerId() {
	const auto grid = static_cast<UShooterGameInstance*> (ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->GridInfoField();
	return grid->GetCurrentServerId();
}

const std::unordered_map<std::string, ULevel*> getIslandLevels() {
	std::unordered_map<std::string, ULevel*> levels;
	for (auto level : ArkApi::GetApiUtils().GetWorld()->LevelsField()) {
		if (level->SubLevelGroupUniqueIDField() < 0)
			continue;

		int id = level->SubLevelGroupUniqueIDField();
		FString name;
		level->NameField().ToString(&name);

		Log::GetLog()->info("{}", level->SubLevelGroupUniqueIDField());
		std::string idStr = std::to_string(id);
		levels[idStr] = level;
	}
	return levels;
}


static void GPSBounds() {
	const auto grid = static_cast<UShooterGameInstance*> (ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->GridInfoField();
	FVector vec;
	FVector2D res;
	nlohmann::json json;
	vec.X = grid->TotalGridsXField() * (grid->GridSizeField());
	vec.Y = grid->TotalGridsYField() * (grid->GridSizeField());

	grid->GlobalLocationToGPSLocation(&res, FVector(0, 0, 0));
	json["min"] = { res.X, res.Y };

	grid->GlobalLocationToGPSLocation(&res, vec);
	json["max"] = { res.X, res.Y };

	std::ofstream file("resources/gpsbounds.json");
	file << std::setw(4) << json;
	file.flush();
	file.close();
}


static const std::string ServerGrid() {
	// Get server grid 
	const auto grid = static_cast<UShooterGameInstance*> (ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->GridInfoField();
	const char* x[15] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O" };
	const char* y[15] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15" };
	std::string gridStr = x[grid->GetCurrentServerInfo()->gridXField()];
	gridStr += y[grid->GetCurrentServerInfo()->gridYField()];
	return gridStr;
}


FString GetStoneIndexFromActor(AActor* a)
{
	int island;
	TArray<FString> islandData1;
	TArray<FString> islandData2;
	UVictoryCore::GetIslandCustomDatas(a, &island, &islandData1, &islandData2);

	if (islandData2.Num() == 0)
		return FString("");

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
}


static FVector2D ActorGPS(AActor* a) {
	// Get actor location
	return VectorGPS(a->RootComponentField()->RelativeLocationField());
}


static std::string GetIslandID(std::string s) {
	transform(s.begin(), s.end(), s.begin(), ::tolower);
	const std::regex rgx("-([0-9]{1,6})\.");

	std::smatch matches;
	std::regex_search(s, matches, rgx);

	if (matches.size() < 1) {
		return "";
	}

	return matches[1].str();
}


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
	name = name.Replace(L"Default__", L"");
	name = name.Replace(L"PrimalItem", L"");
	name = name.Replace(L"Common ", L"");
	name = name.Replace(L" (Broken)", L"");
	name = name.TrimStartAndEnd();
	name.RemoveFromStart("_");
	return name;
}


// Remove things we do not care about.
FString fixNPCName(FString npcname) {
	npcname = npcname.Replace(L"Default__", L"");
	npcname = npcname.Replace(L"_Character_BP_Trench", L"");
	npcname = npcname.Replace(L"_Character_BP_Cave", L"");
	npcname = npcname.Replace(L"_Character_BP", L"");
	npcname = npcname.Replace(L"FreshwaterFish_", L"");
	npcname = npcname.Replace(L"SaltwaterFish_", L"");
	npcname = npcname.Replace(L"Wild", L"");
	npcname = npcname.Replace(L"Atlas", L"");
	npcname = npcname.Replace(L"GiantSnake", L"Cobra");
	npcname = npcname.Replace(L"CarrierTurtle", L"Grand Tortugar");
	npcname = npcname.Replace(L"CaveBat", L"Bat");
	npcname = npcname.Replace(L"Giant", L"");
	npcname = npcname.Replace(L"Child", L"");
	npcname.RemoveFromEnd(L"_C");
	npcname = npcname.Replace(L"_BP", L"");
	npcname = npcname.Replace(L"_", L" ");
	npcname = npcname.TrimStartAndEnd();
	return npcname;
}


std::string getIconName(UTexture2D* tex) {
	FString name;
	tex->NameField().ToString(&name);
	return name.ToString();
}


void dumpItems() {
	nlohmann::json json;
	json = nullptr;
	Log::GetLog()->info("Dump Items");

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
		FString className;
		n->NameField().ToString(&className);
		className = fixName(className);

		json[name.ToString()]["ClassName"] = className.ToString();
		if (n->ItemIconField())
			json[name.ToString()]["Icon"] = getIconName(n->ItemIconField());

		json[name.ToString()]["Name"] = n->DescriptiveNameBaseField().ToString();

		if (n->UseItemAddCharacterStatusValuesField().Num()) {
			for (auto stat : n->UseItemAddCharacterStatusValuesField()) {
				json[name.ToString()]["stats"][statNames[stat.StatusValueType.GetValue()]] = {
					{"add", stat.BaseAmountToAdd},
					{"addOverTime", stat.bAddOverTime == 1 ? true : false},
					{"addOverTimeSpeed", stat.AddOverTimeSpeed},
				};
				json[name.ToString()]["SpoilTime"] = n->SpoilingTimeField();
				json[name.ToString()]["Weight"] = n->BaseItemWeightField() * n->BaseItemWeightMultiplierField();
				json[name.ToString()]["Type"] = n->ItemTypeCategoryStringField().ToString();
			}
		}

		if (n->BuffToGiveOwnerCharacterField().uClass && n->BuffToGiveOwnerCharacterField().uClass->ClassDefaultObjectField()) {
			auto buff = static_cast<APrimalBuff*> (n->BuffToGiveOwnerCharacterField().uClass->ClassDefaultObjectField());
			//	Log::GetLog()->info("Buff Ref {} {}", buff->BuffDescriptionField().ModifierName.ToString(), buff->BuffDescriptionField().ModifierDescription.ToString());

			for (auto statGroup : buff->BuffStatGroupEntriesField()) {
				json[name.ToString()]["stats"][statGroup.StatGroupName.ToString().ToString()] = {
					{"modify", statGroup.StatModifier},
				};
				//	Log::GetLog()->info("\t{} ", statGroup.StatGroupName.ToString().ToString());
			}
		}

		for (auto res : n->BaseCraftingResourceRequirementsField()) {
			if (res.ResourceItemType.uClass && res.BaseResourceRequirement > 0.0f) {
				if (res.ResourceItemType.uClass->ClassDefaultObjectField()) {
					auto pi = static_cast<UPrimalItem*> (res.ResourceItemType.uClass->ClassDefaultObjectField());
					FString type;
					pi->GetItemName(&type, false, true, NULL);
					type = fixName(type);
					json[name.ToString()]["Ingredients"][type.ToString()] = res.BaseResourceRequirement;
				}
			}
		}
	}

	std::ofstream file("resources/items.json");
	file << std::setw(4) << json;
	file.flush();
	file.close();
}


void dumpIslandInfo() {
	nlohmann::json json;
	const auto sgm = ArkApi::GetApiUtils().GetShooterGameMode();
	const auto islandMap = sgm->AtlasIslandInfoField();

	for (auto i : islandMap) {
		if (i.Key && i.Value->IslandIdField())
			json[std::to_string(i.Value->IslandIdField())] = {
				{"id", i.Value->IslandIdField()},
				{"islandPoints", i.Value->IslandPointsField()}
		};
	}

	std::ofstream file("resources/islandInfo.json");
	file << std::setw(4) << json << std::endl;
	file.flush();
	file.close();
}


void dumpShips() {
	nlohmann::json json;
	json["Structures"] = nullptr;
	Log::GetLog()->info("Dump Ships");

	TArray<UObject*> types;
	Globals::GetObjectsOfClass(APrimalRaft::GetPrivateStaticClass(NULL), &types, true, EObjectFlags::RF_NoFlags);
	for (auto object : types) {
		auto n = static_cast<APrimalRaft*> (object);
		FString name;
		n->NameField().ToString(&name);
		FString className;
		n->NameField().ToString(&className);
		className = fixName(className);

		json["Ships"][name.ToString()]["ClassName"] = className.ToString();
		if (n->IconField())
			json["Ships"][name.ToString()]["Icon"] = getIconName(n->IconField());
	}

	std::ofstream file("resources/ships.json");
	file << std::setw(4) << json << std::endl;
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
		
		json["Structures"][name.ToString()]["DecayTimeDays"] = n->DecayDestructionPeriodMultiplierField() * n->DecayDestructionPeriodField() / 86400;
		json["Structures"][name.ToString()]["DecayTimeNoFlagDays"] = n->NoClaimFlagDecayDestructionPeriodMultiplierField() * n->NoClaimFlagDecayDestructionPeriodField() / 86400;
		json["Structures"][name.ToString()]["PreventPlacingNearEnemyRadius"] = n->PreventPlacingNearEnemyRadiusField();
		json["Structures"][name.ToString()]["AdditionalFoundationSupportDistanceForLinkedStructures"] = n->AdditionalFoundationSupportDistanceForLinkedStructuresField();
		json["Structures"][name.ToString()]["MaximumFoundationSupport2DBuildDistance"] = n->MaximumFoundationSupport2DBuildDistanceField();

		json["Structures"][name.ToString()]["IsFoundation"] = n->bIsFoundation().Get();
		json["Structures"][name.ToString()]["IsFenceFoundation"] = n->bIsFenceFoundation().Get();
		json["Structures"][name.ToString()]["UseFenceFoundation"] = n->bUseFenceFoundation().Get();
		json["Structures"][name.ToString()]["IsFloor"] = n->bIsFloor().Get();
		json["Structures"][name.ToString()]["IsWall"] = n->bIsWall().Get();
		json["Structures"][name.ToString()]["TakeAnythingAsGround"] = n->bTakeAnythingAsGround().Get();
		json["Structures"][name.ToString()]["ForcePlacingOnGround"] = n->bForcePlacingOnGround().Get();
		json["Structures"][name.ToString()]["ForceGroundForFoundation"] = n->bForceGroundForFoundation().Get();
		json["Structures"][name.ToString()]["FoundationRequiresGroundTrace"] = n->bFoundationRequiresGroundTrace().Get();
	}

	std::ofstream file("resources/structures.json");
	file << std::setw(4) << json << std::endl;
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


nlohmann::json getLootSets(FSupplyCrateItemSet* set) {
	nlohmann::json json;
	int count = 0;
	for (auto entry : set->ItemEntries) {
		auto entryName = entry.ItemEntryName.ToString();
		json[count]["ForceBlueprint"] = entry.bForceBlueprint;
		json[count]["Weight"] = entry.EntryWeight;
		json[count]["ChanceForItem"] = entry.ChanceToActuallyGiveItem;
		json[count]["ChanceForBlueprint"] = entry.ChanceToBeBlueprintOverride;
		json[count]["MaxQuality"] = entry.MaxQuality;
		json[count]["MaxQuantity"] = entry.MaxQuantity;
		json[count]["MinQuality"] = entry.MinQuality;
		json[count]["MinQuantity"] = entry.MinQuantity;
		json[count]["QualityPower"] = entry.QualityPower;
		json[count]["QuantityPower"] = entry.QuantityPower;

		for (auto item : entry.Items) {
			if (item.uClass && item.uClass->ClassDefaultObjectField()) {
				auto i = static_cast<UPrimalItem*> (item.uClass->ClassDefaultObjectField());
				FString itemName;
				i->GetItemName(&itemName, false, true, NULL);
				json[count]["Items"].push_back(fixName(itemName).ToString());
			}
		}
		count++;
	}
	return json;
}


void dumpLootTables() {
	Log::GetLog()->info("Dump Loot Tables");
	nlohmann::json json;
	json["Loot"] = nullptr;

	TArray<UObject*> objects;
	Globals::GetObjectsOfClass(UPrimalSupplyCrateItemSet::GetPrivateStaticClass(NULL), &objects, true, EObjectFlags::RF_NoFlags);
	for (auto object : objects) {
		auto n = static_cast<UPrimalSupplyCrateItemSet*> (object);
		FString name;
		n->NameField().ToString(&name);
		name = fixName(name);
		json["Loot"][name.ToString()] = getLootSets(&n->ItemSetField());
	}

	objects.Empty();
	Globals::GetObjectsOfClass(UPrimalInventoryComponent::GetPrivateStaticClass(NULL), &objects, true, EObjectFlags::RF_NoFlags);
	for (auto object : objects) {
		auto n = static_cast<UPrimalInventoryComponent*> (object);
		if (n->ItemSetsField().Num() > 0) {
			FString name;
			n->NameField().ToString(&name);
			name = fixName(name);
			//Log::GetLog()->info("Loot Ref {} ", name.ToString());
			for (auto set : n->ItemSetsField()) {
				json["Loot"][name.ToString()] = getLootSets(&set);
			}
		}
	}

	std::ofstream file("resources/loottable.json");
	file << std::setw(4) << json;
	file.flush();
	file.close();
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
		FString className;
		n->NameField().ToString(&className);
		className = fixName(className);
		if (name.Contains("Default__Vulture") || name.Contains("Default__Sheep") || name.Contains("Default__Crab") || name.Contains("Dino") || name.Contains("Primal") || name.Contains("PathFollow") || name.Contains("Galley") || name.Contains("_Raft"))
			continue;

		name = fixNPCName(name);

		for (auto b : n->MatingRequiresBiomeTagsField()) {
			FString bname;
			b.ToString(&bname);
			json["Animals"][name.ToString()]["breedingBiomes"].push_back(bname.ToString());
		}
		json["Animals"][name.ToString()]["className"] = className.ToString();
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

		//Log::GetLog()->info("animal {} stats {}", name.ToString(), n->GetCharacterStatusComponent()->BaseLevelMaxStatusValuesField().GetSize());

		if (n->GetCharacterStatusComponent() && n->GetCharacterStatusComponent()->BaseLevelMaxStatusValuesField().GetSize() > 0)
			for (int i = 0; i < n->GetCharacterStatusComponent()->BaseLevelMaxStatusValuesField().GetSize(); i++) {
				json["Animals"][name.ToString()]["baseStatValue"][statNames[i]] = n->MyCharacterStatusComponentField()->BaseLevelMaxStatusValuesField()()[i];
			}

		if (n->DinoSettingsClassField().uClass) {
			const auto dsc = static_cast<UPrimalDinoSettings*>(n->DinoSettingsClassField().uClass->GetDefaultObject(true));
			json["Animals"][name.ToString()]["foodPreference"] = dsc->DinoFoodTypeNameField().ToString();
			json["Animals"][name.ToString()]["affinityDecreaseSpeed"] = dsc->TamingAffinityNoFoodDecreasePercentageSpeedField();

			if (dsc) {
				for (const auto food : dsc->FoodEffectivenessMultipliersField()) {
					if (food.FoodItemParent.uClass) {
						auto foodItem = static_cast<UPrimalItem*>(food.FoodItemParent.uClass->GetDefaultObject(true));
						FString type;
						foodItem->GetItemName(&type, false, false, NULL);
						json["Animals"][name.ToString()]["food"].push_back({
							{"name", type.ToString()},
							{"weight", foodItem->BaseItemWeightField() * foodItem->BaseItemWeightMultiplierField()},
							{"affinityOverride", food.AffinityEffectivenessMultiplier * food.AffinityOverride},
							{"foodEffectivenessMultiplier", food.FoodEffectivenessMultiplier},
							});
					}
				}
			}
		}

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


nlohmann::json getAnimal(TSubclassOf<APrimalDinoCharacter> entryClass) {
	auto e = static_cast<APrimalDinoCharacter*> (entryClass.uClass->ClassDefaultObjectField());
	FString npcname;
	e->NameField().ToString(&npcname);
	if (npcname.Contains("HumanNPC"))
		return NULL;

	npcname = fixNPCName(npcname);
	
	nlohmann::json animal;
	animal["name"] = npcname.ToString();
	return animal;
}

// Compress nodes into a convex hull
std::vector<point> getNodeLocations(UInstancedStaticMeshComponent* n) {
	std::vector<point> points;

	for (int i = 0; i < n->GetInstanceCount(); i++) {
		FVector x;
		n->GetPositionOfInstance(&x, i);
		auto g = VectorGPS(x);
		points.push_back(std::make_tuple(g.X, g.Y));
	}

	return convexHull(points);
}

void getResources(nlohmann::json& json) {
    TArray<UObject*> objects;
    auto levels = getIslandLevels();
    Globals::GetObjectsOfClass(UInstancedStaticMeshComponent::GetPrivateStaticClass(NULL), &objects, true, EObjectFlags::RF_NoFlags);

    for (auto object : objects) {
        auto n = reinterpret_cast<UInstancedStaticMeshComponent*>(object);
        if (!n) continue;

        auto componentLevel = n->GetComponentLevel();
        FString lvlname;
        componentLevel->GetFullName(&lvlname, NULL);
        std::string island = GetIslandID(lvlname.ToString());
        auto level = levels[island];
        if (!level) continue;

        UPrimalHarvestingComponent* harvestComponent = nullptr;
        TSubclassOf<UActorComponent> myOverride;
        AFoliageAttachmentOverrideVolume::GetOverridenFoliageAttachment(&myOverride, level, n->FoliageTypeReferenceField());
        if (myOverride.uClass) {
            harvestComponent = static_cast<UPrimalHarvestingComponent*>(myOverride.uClass->GetDefaultObject(true));
        } else if (n->AttachedComponentClassField().uClass) {
            harvestComponent = static_cast<UPrimalHarvestingComponent*>(n->AttachedComponentClassField().uClass->GetDefaultObject(true));
        }
        if (!harvestComponent) continue;

        std::vector<std::string> names;
        for (auto& r : harvestComponent->HarvestResourceEntriesField()) {
            if (r.ResourceItem.uClass) {
                auto pi = static_cast<UPrimalItem*>(r.ResourceItem.uClass->GetDefaultObject(true));
                FString type;
                pi->GetItemName(&type, false, false, NULL);
                names.push_back(type.ToString());
                if (n->GetInstanceCount()) {
						 if (json[island]["resources"][type.ToString()].is_null())
							 json[island]["resources"][type.ToString()] = 0;
                    json[island]["resources"][type.ToString()] = json[island]["resources"][type.ToString()] + n->GetInstanceCount();
                }
            }
        }
		  if (n->GetInstanceCount() && names.size()) {
			  std::string resourceNames = std::accumulate(names.begin(), names.end(), std::string(),
				  [](const std::string& a, const std::string& b) -> std::string {
					  return a + (a.length() > 0 ? "," : "") + b;
				  });

			  if (json[island]["resourceNodes"][resourceNames].is_null()) {
				  json[island]["resourceNodes"][resourceNames] = nlohmann::json::array();
			  }
			  json[island]["resourceNodes"][resourceNames] += getNodeLocations(n);
		  }
    }
}


void extractAll() {
	nlohmann::json json;
	Log::GetLog()->info("Server Grid {} ", ServerGrid());

	GPSBounds();

	dumpItems();
	dumpStructures();
	dumpShips();
	dumpLootTables();
	dumpIslandInfo();
	dumpAnimals();

	getResources(json);

	std::ofstream file("resources/" + ServerGrid() + "_new.json");
	file << json.dump(4, ' ', false, nlohmann::json::error_handler_t::ignore);
	file.flush();
	file.close();
}

void extract(float a2) {
	UWorld* World = ArkApi::GetApiUtils().GetWorld();
	auto gameInstance = static_cast<UShooterGameInstance*> (World->OwningGameInstanceField());
	auto gameData = ArkApi::GetApiUtils().GetGameData();

	nlohmann::json json;

	Log::GetLog()->info("Server Grid {} ", ServerGrid());

	GPSBounds();

	dumpItems();
	dumpStructures();
	dumpShips();
	dumpLootTables();
	dumpIslandInfo();
	dumpAnimals();

	// Get discovery Zones
	json["Discoveries"] = getDiscoveries(World);
	const auto grid = static_cast<UShooterGameInstance*> (World->OwningGameInstanceField())->GridInfoField();
	const auto server = grid->GetCurrentServerInfo();

	Log::GetLog()->info("Server Name {} {}", grid->WorldFriendlyNameField().ToString(), server->NameField().ToString());

	TArray<AActor*> found_actors;
	// Loop all actors first time to find things we are interested in
	UGameplayStatics::GetAllActorsOfClass(reinterpret_cast<UObject*> (World), AActor::GetPrivateStaticClass(NULL), &found_actors);
	for (auto actor : found_actors) {
		FString name;
		actor->GetFullName(&name, NULL);

		if (name.Contains("Biome")) {
			auto gps = ActorGPS(actor);
			auto biome = static_cast<ABiomeZoneVolume*> (actor);
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

		if (actor->IsA(ANPCZoneManager::GetPrivateStaticClass(NULL))) // if (name.Contains("CreatureSpawnEntries")) 
		{
			//Log::GetLog()->info("Spawner ? {}", name.ToString());
			auto zm = static_cast<ANPCZoneManager*> (actor);
			if (zm->NPCSpawnEntriesContainerObjectField().uClass) {
				auto spawnEntriesContainer = static_cast<UNPCSpawnEntriesContainer*> (zm->NPCSpawnEntriesContainerObjectField().uClass->ClassDefaultObjectField());
				for (auto add : gameData->TheNPCSpawnEntriesContainerAdditionsField()) {
					if (add.SpawnEntriesContainerClass.uClass == zm->NPCSpawnEntriesContainerObjectField().uClass) {
						for (int i = 0; i < add.AdditionalNPCSpawnEntries.Num(); i++) {
							spawnEntriesContainer->NPCSpawnEntriesField().Add(add.AdditionalNPCSpawnEntries[i]);
							if (add.AdditionalNPCSpawnLimits.IsValidIndex(i))
								spawnEntriesContainer->NPCSpawnLimitsField().Add(add.AdditionalNPCSpawnLimits[i]);
						}
					}
				}

				char buff[200];
				nlohmann::json locations;
				bool found = false;
				for (auto spawnVolume : zm->LinkedZoneSpawnVolumeEntriesField()) {
					if (spawnVolume.LinkedZoneSpawnVolume) {
						found = true;
						auto gps = ActorGPS(spawnVolume.LinkedZoneSpawnVolume);
						snprintf(buff, sizeof(buff), "%.2f:%.2f", gps.X, gps.Y);
						locations.push_back({ gps.X, gps.Y });
					}
				}
				if (found) {
					for (auto npc : spawnEntriesContainer->NPCSpawnEntriesField()) {
						nlohmann::json animals;
						for (auto entryClass : npc.NPCsToSpawn) {
							for (auto remap : gameData->GlobalNPCRandomSpawnClassWeightsField()) {
								if (entryClass.uClass == remap.FromClass.uClass) {
									int num = 0;
									for (auto animal : remap.ToClasses) {
										if (remap.Weights[num] >= 1.0f)
											entryClass.uClass = remap.FromClass.uClass;
										else {
											if (animal.uClass)
												animals.push_back(getAnimal(animal.uClass));
										}
									}
									num++;
								}
							}
							if (entryClass.uClass)
								animals.push_back(getAnimal(entryClass));
						}

						json["Animals"][buff].push_back({
							{ "gps", locations },
							{ "spawnLimits", {zm->MinDesiredNumberOfNPCField() * zm->DesiredNumberOfNPCMultiplierField(), zm->AbsoluteMaxNumberOfNPCField(), zm->TheIncreaseNPCIntervalField() * zm->IncreaseNPCIntervalMultiplierField() } },
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
			else {
				for (auto b : type->NPCSpawnEntriesField()) {
					boss = b.AnEntryName;
				}
			}
			Log::GetLog()->info("Boss {}", name.ToString());
			if (json["Boss"][boss.ToString()].is_null())
				json["Boss"][boss.ToString()] = nlohmann::json::array();
			for (auto spawnVolume : zoneManager->LinkedZoneSpawnVolumeEntriesField()) {
				auto gps = ActorGPS(spawnVolume.LinkedZoneSpawnVolume);
				json["Boss"][boss.ToString()].push_back({ gps.X, gps.Y });
			}
		}

		if (name.Contains("FloatsamSupplyCrate")) {
			auto fsc = static_cast<ASupplyCrateSpawningVolume*> (actor);
			//Log::GetLog()->info("Found Flotsam Manager {}", name.ToString());
			json["Flotsam"] = {
				{ "spawnLimits", {fsc->MaxNumCratesField(), fsc->CrateSpawnDensityPerAreaField(), fsc->CreateSpawnDensityMultiplierField()} },
				{ "qualityMultiplier", fsc->ExtraCrateQualityMultiplierField()},
			};
		}

		if (name.Contains("SunkenTreasure")) {
			auto fsc = static_cast<ASupplyCrateSpawningVolume*> (actor);
			//Log::GetLog()->info("Found Sunken Treasure Manager {}", name.ToString());
			json["SunkenTreasure"] = {
				{ "spawnLimits", {fsc->MaxNumCratesField(), fsc->CrateSpawnDensityPerAreaField(), fsc->CreateSpawnDensityMultiplierField()} },
				{ "qualityMultiplier", fsc->ExtraCrateQualityMultiplierField()},
			};
		}

		if (name.Contains("NPCShipZoneManager")) {
			auto zoneManager = static_cast<ANPCZoneManager*> (actor);
			//Log::GetLog()->info("Found Ship Zone Manager {}", name.ToString());
			json["SoTD"] = {
				{ "spawnLimits", {zoneManager->MinDesiredNumberOfNPCField() * zoneManager->DesiredNumberOfNPCMultiplierField(), zoneManager->AbsoluteMaxNumberOfNPCField(), zoneManager->TheIncreaseNPCIntervalField() * zoneManager->IncreaseNPCIntervalMultiplierField() } },
				{ "levelOffset", zoneManager->ExtraNPCLevelOffsetField() },
				{ "levelLerp", zoneManager->NPCLerpToMaxRandomBaseLevelField() },
				{ "levelMultiplier", zoneManager->NPCLevelMultiplierField() },
				{ "levelMinOveride", zoneManager->ForceOverrideNPCBaseLevelField() }
			};
		}

		if (name.Contains("OceanEpicNPCZoneManager")) {
			//Log::GetLog()->info("Found ocean epic {}", name.ToString());
			auto zoneManager = static_cast<ANPCZoneManager*> (actor);
			for (const auto dino : zoneManager->NPCSpawnEntriesField()) {
				int count = 0;
				for (const auto npc : dino.NPCsToSpawn) {
					if (npc.uClass == NULL)
						continue;

					auto type = static_cast<APrimalDinoCharacter*> (npc.uClass->ClassDefaultObjectField());
					type->GetFullName(&name, NULL);
					//Log::GetLog()->info("animal {}", name.ToString());
					auto gps = VectorGPS(dino.NPCsSpawnOffsets[count]);

					if (name.Contains("MeanWhale_SeaMonster")) {
						if (json["Boss"]["MeanWhale"].is_null())
							json["Boss"]["MeanWhale"] = nlohmann::json::array();
						json["Boss"]["MeanWhale"].push_back({ gps.X, gps.Y });
						//Log::GetLog()->info("add boss {}", name.ToString());
					}
					if (name.Contains("GentleWhale_SeaMonster")) {
						if (json["Boss"]["GentleWhale"].is_null())
							json["Boss"]["GentleWhale"] = nlohmann::json::array();
						json["Boss"]["GentleWhale"].push_back({ gps.X, gps.Y });
						//Log::GetLog()->info("add boss {}", name.ToString());
					}
					if (name.Contains("Squid_Character")) {
						if (json["Boss"]["GiantSquid"].is_null())
							json["Boss"]["GiantSquid"] = nlohmann::json::array();
						json["Boss"]["GiantSquid"].push_back({ gps.X, gps.Y });
						//Log::GetLog()->info("add boss {}", name.ToString());
					}
					count++;
				}
			}
		}

		if (name.Contains("SnowCaveBossManager")) {
			auto gps = ActorGPS(actor);
			json["Boss"]["Yeti"] = nlohmann::json::array();
			json["Boss"]["Yeti"].push_back({ gps.X, gps.Y });
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
		}
	}
	found_actors.Empty();

	// Find actual resources, maps, and meshes
	auto levels = getIslandLevels();

	std::unordered_map<std::string, std::unordered_map<std::string, long long>> resources;
	std::unordered_map<std::string, std::unordered_map<std::string, long long>> assets;
	std::unordered_map<std::string, std::unordered_map<std::string, long long>> assetType;
	std::unordered_map<std::string, std::vector<std::vector<float>>> maps;
	std::unordered_map<std::string, std::vector<std::string>> meshes;

	TArray<UObject*> objects;
	Globals::GetObjectsOfClass(UInstancedStaticMeshComponent::GetPrivateStaticClass(NULL), &objects, true, EObjectFlags::RF_NoFlags);
	for (auto object : objects) {
		auto n = reinterpret_cast<UInstancedStaticMeshComponent*> (object);
		if (!n) 
			continue;

		// Get the override key
		auto componentLevel = n->GetComponentLevel();
		FString lvlname;
		componentLevel->GetFullName(&lvlname, NULL);
		std::string island = GetIslandID(lvlname.ToString());
		auto level = levels[island];
		if (!level)
			continue;
		

		UPrimalHarvestingComponent* harvestComponent = nullptr;
		TSubclassOf<UActorComponent> myOverride;
		AFoliageAttachmentOverrideVolume::GetOverridenFoliageAttachment(&myOverride, level, n->FoliageTypeReferenceField());
		if (myOverride.uClass) {
			harvestComponent = static_cast<UPrimalHarvestingComponent*> (myOverride.uClass->GetDefaultObject(true));
		} else {
			if (n->AttachedComponentClassField().uClass) {
				harvestComponent = static_cast<UPrimalHarvestingComponent*> (n->AttachedComponentClassField().uClass->GetDefaultObject(true));
			}
		}
		if (!harvestComponent)
			continue;

		FVector vec;
		n->GetWorldLocation(&vec);
		const FVector2D loc = VectorGPS(vec);

		FString name;
		n->GetFullName(&name, NULL);

		// map key
		char buff[200];
		snprintf(buff, sizeof(buff), "%.2f:%.2f", loc.X, loc.Y);
		std::string key = buff;

		FString assetName, path;
		harvestComponent->GetPathName(NULL, &path);
		if (n->StaticMeshField()) {
			n->StaticMeshField()->NameField().ToString(&assetName);
			assets[key][assetName.ToString()] += n->GetInstanceCount();
			assetType[name.ToString()][path.ToString()] += n->GetInstanceCount();
		}

		for (auto r : harvestComponent->HarvestResourceEntriesField()) {
			TSubclassOf<UPrimalItem> hcSub = r.ResourceItem;
			if (hcSub.uClass) {
				auto pi = static_cast<UPrimalItem*> (hcSub.uClass->GetDefaultObject(true));
				FString type;
				pi->GetItemName(&type, false, false, NULL);
				if (n->GetInstanceCount())
					resources[key][type.ToString()] += n->GetInstanceCount();
			}
		}
	}
	objects.Empty();


	UGameplayStatics::GetAllActorsOfClass(reinterpret_cast<UObject*> (World),
		AActor::GetPrivateStaticClass(NULL), &found_actors);
	for (auto actor : found_actors) {
		FString name;

		actor->GetFullName(&name, NULL);

		// Find the altars
		if (actor->IsA(APrimalStructureStaticNodeContainer::GetPrivateStaticClass(NULL))) {
			auto gps = ActorGPS(actor);
			auto node = static_cast<APrimalStructureStaticNodeContainer*>(actor);
			if (node) {
				auto tags = actor->TagsField();
				//Log::GetLog()->info(" placeholder {} {} - {}/{}", tags.Num(), node->DescriptiveNameField().ToString(), gps.X, gps.Y);
				json["Altar"][node->DescriptiveNameField().ToString()].push_back({ gps.X, gps.Y });
			}
		}

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

	for (auto asset : assetType) {
		json["AssetType"][asset.first] = nullptr;
		for (auto resource : asset.second) {
			json["AssetType"][asset.first][resource.first] = resource.second;
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

	std::ofstream file("resources/" + ServerGrid() + ".json");
	file << json.dump(4, ' ', false, nlohmann::json::error_handler_t::ignore);
	file.flush();
	file.close();
}


void Hook_UShooterGameInstance_StartGameInstance(UShooterGameInstance* a_shooter_game_instance) {
	std::filesystem::create_directory("resources");
	UShooterGameInstance_StartGameInstance_original(a_shooter_game_instance);
	
	extract(1);
	extractAll();
	
	FWindowsPlatformMisc::RequestExit(true);
	exit(0);
	Log::GetLog()->info("would exit");
}


void Load() {
	Log::Get().Init("DumpResources");
	ArkApi::GetHooks().SetHook("UShooterGameInstance.StartGameInstance", &Hook_UShooterGameInstance_StartGameInstance, &UShooterGameInstance_StartGameInstance_original);
}


void Unload() {
	ArkApi::GetHooks().DisableHook("UShooterGameInstance.StartGameInstance", &Hook_UShooterGameInstance_StartGameInstance);
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