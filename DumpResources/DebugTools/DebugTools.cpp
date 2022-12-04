#include <API/Atlas/Atlas.h>
#pragma comment(lib, "AtlasApi.lib")

int gHangTickTime;
float gMasterDeltaTime;
float gFixFPS;

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