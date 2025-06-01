#include <API/Atlas/Atlas.h>
#pragma comment(lib, "AtlasApi.lib")
#include <limits>

DECLARE_HOOK(APrimalStructureClaimFlag_GetLandClaim, APrimalStructureClaimFlag*, UWorld *ForWorld, const FVector *AtLocation, float SearchRadius);

void Load() {
	Log::Get().Init("SeaTax");
	ArkApi::GetHooks().SetHook("APrimalStructureClaimFlag.GetLandClaim", &Hook_APrimalStructureClaimFlag_GetLandClaim, &APrimalStructureClaimFlag_GetLandClaim_original);
}

void Unload() {
	ArkApi::GetHooks().DisableHook("APrimalStructureClaimFlag.GetLandClaim", &Hook_APrimalStructureClaimFlag_GetLandClaim);
}

APrimalStructureClaimFlag* Hook_APrimalStructureClaimFlag_GetLandClaim(UWorld* ForWorld, const FVector* AtLocation, float SearchRadius) {
	
	auto flag = APrimalStructureClaimFlag_GetLandClaim_original(ForWorld, AtLocation, SearchRadius);
	if (flag)
		return flag;
	
	if (SearchRadius != 25000.0)
		return flag;

	// Find the closest flag
	const auto sgm = ArkApi::GetApiUtils().GetShooterGameMode();
	const auto islandMap = sgm->AtlasIslandInfoField();
	APrimalStructureClaimFlag* closestFlag = nullptr;
    
   float closestDistSq = 100000000000000;

	for (const auto& island : islandMap) {
		APrimalStructureClaimFlag* islandFlag = island.Value->IslandSettlementFlagField().Get();
		if (islandFlag) {
			const FVector& flagLoc = islandFlag->RootComponentField()->RelativeLocationField();
			float distSq = FVector::DistSquared(flagLoc, *AtLocation);
			if (distSq < closestDistSq) {
				closestDistSq = distSq;
				closestFlag = islandFlag;
			}
		}
	}

	if (closestFlag) {
		return closestFlag;
	}
	
	return flag;
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