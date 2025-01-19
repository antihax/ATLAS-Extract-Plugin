#include <API/Atlas/Atlas.h>
#pragma comment(lib, "AtlasApi.lib")
#include "json.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <format>

int gHangTickTime;
float gMasterDeltaTime;
float gFixFPS;

static const std::string ServerGrid() {
	// Get server grid 
	const auto grid = static_cast<UShooterGameInstance*> (ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->GridInfoField();
	const char* x[15] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O" };
	const char* y[15] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15" };
	std::string gridStr = x[grid->GetCurrentServerInfo()->gridXField()];
	gridStr += y[grid->GetCurrentServerInfo()->gridYField()];
	return gridStr;
};

static void DumpDino() {
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

	std::filesystem::create_directory("dump");
	std::ofstream file("dump/" + ServerGrid() + ".json");
	file << std::setw(4) << json;
	file.flush();
	file.close();
};


void DumpDinoData(RCONClientConnection* rconClientConnection, RCONPacket* rconPacket, UWorld* World)
{
	FString reply = L"Successfully dumped\n";
	Log::GetLog()->info(" Dump Data");

	TArray<AActor*> found_actors;
	// Loop all actors first time to find things we are interested in
	UGameplayStatics::GetAllActorsOfClass(reinterpret_cast<UObject*> (World), APrimalDinoCharacter::GetPrivateStaticClass(NULL), &found_actors);
	for (auto actor : found_actors) {
		auto dino = static_cast<APrimalDinoCharacter*>(actor);
		Log::GetLog()->info("{} {} {}", dino->TamingTeamIDField(), dino->DinoID1Field(), dino->DinoID2Field());
	}

	nlohmann::json json;
	std::filesystem::create_directory("dump");
	std::ofstream file("dump/gpsbounds.json");
	file << std::setw(4) << json;
	file.flush();
	file.close();
	
	rconClientConnection->SendMessageW(rconPacket->Id, 0, &reply);
}

void ChatRate(AShooterPlayerController* player_controller, FString* message, EChatSendMode::Type /*unused*/)
{
	TArray<FString> argv;
	message->ParseIntoArray(argv, L" ", true);
	if (!argv.IsValidIndex(1))
		return;
	gHangTickTime = std::stoi(*argv[1]);

	Log::GetLog()->info(" hang for {}", gHangTickTime);
}


void ChatReq(AShooterPlayerController* player_controller, FString* message, EChatSendMode::Type /*unused*/)
{
	Log::GetLog()->info("REQ CALLED");
	TArray<AActor*> found_actors;
	UGameplayStatics::GetAllActorsOfClass(reinterpret_cast<UObject*> (ArkApi::GetApiUtils().GetWorld()), APrimalStructureMonumentThirdTierEgyptTemple::GetPrivateStaticClass(NULL), &found_actors);
	for (auto object : found_actors) {
		auto t = static_cast<APrimalStructureMonumentThirdTierEgyptTemple*> (object);
		if (t) {
			t->UpdateStageBarrierComponents();
			Log::GetLog()->info("ok {}", t->StageRequirementsField().Num());
		}
		for (auto stage : t->StageRequirementsField()) {
			Log::GetLog()->info("stages");
			for (auto r : stage.RequiredResources) {
				Log::GetLog()->info("resources");
				auto item = r.RequiredResourceType;
				if (item.uClass && item.uClass->ClassDefaultObjectField()) {
					auto i = static_cast<UPrimalItem*> (item.uClass->ClassDefaultObjectField());
					FString itemName;
					i->GetItemName(&itemName, false, true, NULL);
					Log::GetLog()->info("\tMonumentThirdTierEgyptTempleC {} {} ", itemName.ToString(), r.RequiredResourceCount);
					//	json[count]["Items"].push_back(fixName(itemName).ToString());

				}
			}
		}
	}
}


void ChatFPS(AShooterPlayerController* player_controller, FString* message, EChatSendMode::Type /*unused*/)
{
	TArray<FString> argv;
	message->ParseIntoArray(argv, L" ", true);
	if (!argv.IsValidIndex(1))
		return;
	gFixFPS = std::stof(*argv[1]);

	Log::GetLog()->info(" setfps {}", gFixFPS);
}

DECLARE_HOOK(AShooterGameMode_BeginPlay, void, AShooterGameMode*);
DECLARE_HOOK(AShooterGameMode_InitOptions, void, AShooterGameMode*, FString);
//DECLARE_HOOK(APrimalStructureSeating_DriverSeat_UpdateDesiredDir, void, APrimalStructureSeating_DriverSeat*, FVector2D);
DECLARE_HOOK(APrimalRaft_SimulatePhysics, void, APrimalRaft*, float);
DECLARE_HOOK(APrimalModularShip_Tick, void, APrimalModularShip*, float);
DECLARE_HOOK(UGameEngine_Tick, void, UGameEngine*, float, bool);
DECLARE_HOOK(UGameEngine_GetMaxTickRate, float, UGameEngine*, float, bool);

void Load() {
	Log::Get().Init("Debug Tools");
	ArkApi::GetHooks().SetHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay, &AShooterGameMode_BeginPlay_original);
	ArkApi::GetHooks().SetHook("AShooterGameMode.InitOptions", &Hook_AShooterGameMode_InitOptions, &AShooterGameMode_InitOptions_original);
	ArkApi::GetHooks().SetHook("APrimalRaft.SimulatePhysics", &Hook_APrimalRaft_SimulatePhysics, &APrimalRaft_SimulatePhysics_original);
	ArkApi::GetHooks().SetHook("APrimalModularShip.Tick", &Hook_APrimalModularShip_Tick, &APrimalModularShip_Tick_original);
	ArkApi::GetHooks().SetHook("UGameEngine.Tick", &Hook_UGameEngine_Tick, &UGameEngine_Tick_original);
	ArkApi::GetHooks().SetHook("UGameEngine.GetMaxTickRate", &Hook_UGameEngine_GetMaxTickRate, &UGameEngine_GetMaxTickRate_original);

	//ArkApi::GetHooks().SetHook("APrimalStructureSeating_DriverSeat.UpdateDesiredDir", &Hook_APrimalStructureSeating_DriverSeat_UpdateDesiredDir, &APrimalStructureSeating_DriverSeat_UpdateDesiredDir_original);

	ArkApi::GetCommands().AddChatCommand("/rate", &ChatRate);
	ArkApi::GetCommands().AddChatCommand("/fps", &ChatFPS);
	ArkApi::GetCommands().AddChatCommand("/req", &ChatReq);
	ArkApi::GetCommands().AddRconCommand("dumpdino", &DumpDinoData);
	
}

void Unload() {
	ArkApi::GetHooks().DisableHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay);
	ArkApi::GetHooks().DisableHook("AShooterGameMode.InitOptions", &Hook_AShooterGameMode_InitOptions);
	ArkApi::GetHooks().DisableHook("APrimalRaft.SimulatePhysics", &Hook_APrimalRaft_SimulatePhysics);
	ArkApi::GetHooks().DisableHook("APrimalRaft.SimulatePhysics", &Hook_APrimalRaft_SimulatePhysics);
	ArkApi::GetHooks().DisableHook("UGameEngine.Tick", &Hook_UGameEngine_Tick);
	ArkApi::GetHooks().DisableHook("UGameEngine.GetMaxTickRate", &Hook_UGameEngine_GetMaxTickRate);

	//ArkApi::GetHooks().DisableHook("APrimalStructureSeating_DriverSeat.UpdateDesiredDir", &Hook_APrimalStructureSeating_DriverSeat_UpdateDesiredDir);
}

void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* This) {
	AShooterGameMode_BeginPlay_original(This);
	//Log::GetLog()->info("bUseNewStructureFoundationSupportChecks = {}", This->bUseNewStructureFoundationSupportChecksField());
	//Log::GetLog()->info("bAllowUnlimitedRespecs = {}", This->bAllowUnlimitedRespecsField());
}

void Hook_AShooterGameMode_InitOptions(AShooterGameMode* This, FString Options) {
	AShooterGameMode_InitOptions_original(This, Options);

	//This->bUseNewStructureFoundationSupportChecksField() = false;
	//This->bAllowUnlimitedRespecsField() = true;
}

/*void Hook_APrimalStructureSeating_DriverSeat_UpdateDesiredDir(APrimalStructureSeating_DriverSeat* This, FVector2D dir) {
	Log::GetLog()->info("update dir {}  {}", dir.X, dir.Y);
	APrimalStructureSeating_DriverSeat_UpdateDesiredDir_original(This, dir);
}*/

void Hook_APrimalRaft_SimulatePhysics(APrimalRaft* This, float deltaTime) {
	//ArkApi::GetApiUtils().SendChatMessageToAll("DeltaTime", "{0:0.3} \t {1:0.3}", deltaTime, gMasterDeltaTime);
	APrimalRaft_SimulatePhysics_original(This, deltaTime);
}

void Hook_APrimalModularShip_Tick(APrimalModularShip* This, float deltaTime) {
	
	auto sinkRate = ArkApi::GetApiUtils().GetWorld()->TimeSince(This->StartLeakingTimeField());

	ArkApi::GetApiUtils().SendChatMessageToAll("StartLeak", "{0} \t {1:0.2}% \t TSR {2}", This->StartLeakingTimeField(), This->SinkPercentageField(), sinkRate);
	APrimalModularShip_Tick_original(This, deltaTime);
}

void Hook_UGameEngine_Tick(UGameEngine* This, float deltaTime, bool sleep) {
	gMasterDeltaTime = deltaTime;
	if (gHangTickTime > 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(gHangTickTime));
	}

	UGameEngine_Tick_original(This, deltaTime, sleep);
}

float Hook_UGameEngine_GetMaxTickRate(UGameEngine* This, float deltaTime, bool allowSmoothing) {
	if (gFixFPS > 0.0f)
		return gFixFPS;

	auto fps = UGameEngine_GetMaxTickRate_original(This, deltaTime, allowSmoothing);

	//Log::GetLog()->info(" setfps {} {}", fps, allowSmoothing);
	return fps;
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