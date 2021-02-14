#include <API/Atlas/Atlas.h>
#pragma comment(lib, "AtlasApi.lib")

DECLARE_HOOK(AShooterGameMode_BeginPlay, void, AShooterGameMode*);
DECLARE_HOOK(AShooterGameMode_InitOptions, void, AShooterGameMode*, FString);

void Load() {
	Log::Get().Init("Force Old Foundation Checks");
	ArkApi::GetHooks().SetHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay, &AShooterGameMode_BeginPlay_original);
	ArkApi::GetHooks().SetHook("AShooterGameMode.InitOptions", &Hook_AShooterGameMode_InitOptions, &AShooterGameMode_InitOptions_original);
}

void Unload() {
	ArkApi::GetHooks().DisableHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay);
	ArkApi::GetHooks().DisableHook("AShooterGameMode.InitOptions", &Hook_AShooterGameMode_InitOptions);
}

void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* This) {
	AShooterGameMode_BeginPlay_original(This);
	Log::GetLog()->info("bUseNewStructureFoundationSupportChecks = {}", This->bUseNewStructureFoundationSupportChecksField());
}

void Hook_AShooterGameMode_InitOptions(AShooterGameMode* This, FString Options) {
	This->bUseNewStructureFoundationSupportChecksField() = false;
	AShooterGameMode_InitOptions_original(This, Options);
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