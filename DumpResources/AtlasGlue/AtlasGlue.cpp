#include <API/Atlas/Atlas.h>
#include <string>
#include <iterator>
#include <algorithm>
#include "json.hpp"
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <chrono>
#include "Config.h"
#include "RedisManager.h"
#include "DiscordWebhookLeaderElector.h"
#include "TribeChannelSubscriptionManager.h"
#include "DiscordWebhookManager.h"
#include "GlobalChatHandler.h"
#include "TreasureUtils.h"
#include "ServerUtils.h"
#pragma comment(lib, "AtlasApi.lib")

RedisManager* g_Redis;
DiscordWebhookLeaderElector* g_DiscordLeader = nullptr;
TribeChannelSubscriptionManager* g_TribeChannelManager = nullptr;
DiscordWebhookManager* g_DiscordWebhook = nullptr;
GlobalChatHandler* g_ChatHandler = nullptr;

// Global state
std::map<int, std::chrono::system_clock::time_point> g_LastSeen;
std::thread heartbeat;
std::atomic<bool> stopbeating{ false };
std::atomic<int> connectionRetries{ 0 };

// Utility function to strip rich text formatting from user input
std::string StripRichText(const std::string& text) {
	std::string result = text;

	// Remove all <RichColor ...> tags
	size_t pos = 0;
	while ((pos = result.find("<RichColor", pos)) != std::string::npos) {
		size_t endPos = result.find(">", pos);
		if (endPos != std::string::npos) {
			result.erase(pos, endPos - pos + 1);
		}
		else {
			// Malformed tag, just remove the opening
			result.erase(pos, 10); // "<RichColor"
		}
	}

	// Remove all </> closing tags
	while ((pos = result.find("</>")) != std::string::npos) {
		result.erase(pos, 3);
	}

	// Remove any other < > tags that might be used for formatting
	while ((pos = result.find("<")) != std::string::npos) {
		size_t endPos = result.find(">", pos);
		if (endPos != std::string::npos) {
			result.erase(pos, endPos - pos + 1);
		}
		else {
			// Incomplete tag, just remove the <
			result.erase(pos, 1);
		}
	}

	return result;
}

template <typename T, typename... Args>
void SendChatMessage(const FString& sender_name, const T* msg, Args&&... args)
{
	const FString text(FString::Format(msg, std::forward<Args>(args)...));

	FChatMessage chat_message = FChatMessage();
	chat_message.SenderName = sender_name;
	chat_message.Message = text;

	const auto& player_controllers = ArkApi::GetApiUtils().GetWorld()->PlayerControllerListField();
	for (TWeakObjectPtr<APlayerController> player_controller : player_controllers)
	{
		AShooterPlayerController* shooter_pc = static_cast<AShooterPlayerController*>(player_controller.Get());
		if (shooter_pc)
		{
			shooter_pc->ClientChatMessage(chat_message);
		}
	}
}

// Simplified message handler
void HandleMessage(const std::string& chan, const std::string& msg) {
	try {
		auto json = nlohmann::json::parse(msg, nullptr, false, true);
		if (json.is_discarded()) {
			Log::GetLog()->warn("[Redis] Failed to parse message from channel '{}'", chan);
			return;
		}

		if (json["raw"].is_null()) {
			Log::GetLog()->debug("[Redis] Message missing 'raw' field from channel '{}'", chan);
			return;
		}

		const auto sender = json["sender"].get<unsigned int>();
		const auto type = json["type"].get<unsigned char>();

		switch (type) {
		case PSM_FetchTreasureLocation: {
			try {
				struct { unsigned int IslandId, RequestId; float InQuality; } request;
				auto binary = json["raw"]["bytes"];
				if (binary.size() >= sizeof(request)) {
					std::copy_n(binary.begin(), sizeof(request), reinterpret_cast<unsigned char*>(&request));
					HandleTreasureRequest(sender, request.RequestId, request.InQuality, false, json["region"]);
				}
				else {
					Log::GetLog()->warn("[Redis] Invalid treasure location request size from sender {}", sender);
				}
			}
			catch (const std::exception& e) {
				Log::GetLog()->error("[Redis] Error handling treasure location request: {}", e.what());
			}
			break;
		}

		case PSM_FetchTreasureLocationSpecial:
			try {
				HandleTreasureRequest(sender, json["requestId"], json["quality"], true, json["region"]);
			}
			catch (const std::exception& e) {
				Log::GetLog()->error("[Redis] Error handling special treasure request: {}", e.what());
			}
			break;

		case PSM_FetchedTreasureLocation: {
			try {
				int requestId = json["requestId"].get<int>();
				TreasureCandidate cnd{
				FVector{ json["X"], json["Y"], json["Z"] },
				json["quality"],
				json["cave"],
				sender
				};

				bool firstResponse = false;
				int bufferedCount = 0;
				{
					std::lock_guard<std::mutex> lg(g_TreasureMutex);
					if (g_TreasureProcessed.count(requestId)) {
						break; // already completed; ignore late responses
					}
					auto& list = g_TreasureCandidates[requestId];
					firstResponse = list.empty();
					list.push_back(std::move(cnd));
					bufferedCount = static_cast<int>(list.size());
				}

				if (Config::GetConfig().enableDetailedLogging) {
					Log::GetLog()->info("[Redis] Buffered treasure location from server {} for request {} ({} so far)",
						sender, requestId, bufferedCount);
				}

				if (firstResponse) {
					ScheduleTreasurePick(requestId);
				}
			}
			catch (const std::exception& e) {
				Log::GetLog()->error("[Redis] Error handling fetched treasure location: {}", e.what());
			}
			break;
		}

		case PSM_HeartBeat:
			g_LastSeen[sender] = std::chrono::system_clock::now();
			break;

		case PSM_GlobalChat: {
			try {
				FString playerName(json["playerName"].get<std::string>());
				std::string serverGrid = json["serverGrid"].get<std::string>();
				std::string message = json["message"].get<std::string>();
				bool isAdmin = json.contains("isAdmin") && json["isAdmin"].get<bool>();

				// Build display name with rich text formatting for admins
				std::string displayName = json["playerName"].get<std::string>();
				if (isAdmin) {
					// Use red/orange color for admin with rich text formatting
					// <RichColor Color="1.0, 0.65, 0.0, 1.0">[ADMIN]</> for orange
					// displayName = "<RichColor Color=\"1.0, 0.0, 0.0, 1.0\">[ADMIN]</> " + displayName;
				}

				// Send chat message with rich text
				FString displayNameStr(displayName);
				SendChatMessage(displayNameStr, "[{}] {}",
					serverGrid.c_str(),
					message.c_str());

				// Delegate Discord webhook handling to GlobalChatHandler
				// Strip rich text formatting for Discord
				std::string discordName = json["playerName"].get<std::string>();
				if (isAdmin) {
					discordName = "[ADMIN] " + discordName;
				}

				if (g_ChatHandler) {
					g_ChatHandler->HandleChatMessage(
						serverGrid,
						discordName,  // Use plain text with admin prefix for Discord
						message
					);
				}
			}
			catch (const std::exception& e) {
				Log::GetLog()->error("[Redis] Error handling global chat: {}", e.what());
			}
			break;
		}

		default:
			Log::GetLog()->debug("[Redis] Unknown message type {} from sender {}", type, sender);
			break;
		}
	}
	catch (const std::exception& e) {
		Log::GetLog()->error("[Redis] Exception in HandleMessage: {}", e.what());
	}
}

// Hook declarations and implementations
DECLARE_HOOK(AShooterGameMode_BeginPlay, void, AShooterGameMode*, float);
DECLARE_HOOK(ASeamlessVolumeManager_SendPacketToPeerServer, PeerServerConnectionData*, ASeamlessVolumeManager*, unsigned int, PeerServerMessageType, TArray<unsigned char>*, bool);
DECLARE_HOOK(ATreasureMapManager_GiveNewTreasureMapToCharacter, void, ATreasureMapManager*, UPrimalItem*, float, AShooterCharacter*);
DECLARE_HOOK(AShooterPlayerController_ServerSendChatMessage_Impl, void, AShooterPlayerController*, FString*, EChatSendMode::Type);
DECLARE_HOOK(APrimalStructureClaimFlag_GetLandClaim, APrimalStructureClaimFlag*, UWorld* ForWorld, const FVector* AtLocation, float SearchRadius);
DECLARE_HOOK(UGameEngine_Tick, void, UGameEngine*, float, bool);
DECLARE_HOOK(AShooterGameMode_InitOptions, void, AShooterGameMode*, FString);

void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* This, float a2) {
	AShooterGameMode_BeginPlay_original(This, a2);

	g_Redis = &RedisManager::Instance();
	g_Redis->SetMessageHandler(HandleMessage);
	g_Redis->Initialize();

	// Start Discord webhook leader election after Redis is connected
	if (!g_DiscordLeader) {
		g_DiscordLeader = new DiscordWebhookLeaderElector(g_Redis, std::to_string(GetServerId()));
		g_DiscordLeader->Start();
	}
	if (!g_DiscordWebhook) {
		g_DiscordWebhook = new DiscordWebhookManager();
	}
	if (!g_ChatHandler) {
		g_ChatHandler = new GlobalChatHandler(g_DiscordLeader, g_DiscordWebhook);
	}

	// Start tribe channel subscription manager (if player notifications enabled)
	if (Config::GetConfig().enablePlayerNotifications && !g_TribeChannelManager) {
		g_TribeChannelManager = new TribeChannelSubscriptionManager(g_Redis, g_DiscordLeader, g_DiscordWebhook);
		g_TribeChannelManager->Start();
		Log::GetLog()->info("[TribeChannelManager] Started tribe channel subscription manager");
	}

	Log::GetLog()->info("Plugin initialized on server ID {}", GetServerId());

	// Start heartbeat thread
	if (!heartbeat.joinable()) {
		heartbeat = std::thread([]() {
			while (!stopbeating) {
				std::this_thread::sleep_for(std::chrono::seconds(Config::GetConfig().heartbeatInterval));
				nlohmann::json heartbeatMsg = {
						{"type", PSM_HeartBeat},
						{"sender", GetServerId()},
						{"peer", 0},
						{"raw", {{"bytes", {0}}, {"subtype", nullptr}}}
				};
				g_Redis->PublishMessage(Config::GetConfig().channelCluster, heartbeatMsg);
			}
			});
	}
}

// Flush pending treasure picks on game thread each tick
void Hook_UGameEngine_Tick(UGameEngine* This, float deltaTime, bool bIdleMode) {
	// Process any pending picks first
	UGameEngine_Tick_original(This, deltaTime, bIdleMode);

	static auto lastInternalTick = std::chrono::steady_clock::now();
	auto now = std::chrono::steady_clock::now();
	if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastInternalTick).count() < 250) {
		return;
	}
	lastInternalTick = now;

	// Drive Discord webhook leader election from game loop
	if (g_DiscordLeader) {
		g_DiscordLeader->Tick();
	}

	std::vector<std::pair<int, TreasureCandidate>> toProcess;
	{
		std::lock_guard<std::mutex> ql(g_PickMutex);
		if (!g_PendingPicks.empty()) {
			toProcess.swap(g_PendingPicks);
		}
	}
	if (!toProcess.empty()) {
		for (const auto& pr : toProcess) {
			const int requestId = pr.first;
			const TreasureCandidate& pick = pr.second;
			if (auto sgm = ArkApi::GetApiUtils().GetShooterGameMode()) {
				if (auto tmm = sgm->TreasureMapManagerField()) {
					FVector loc = pick.location;
					tmm->OnTreasureChestLocationFound(requestId, &loc, pick.cave, pick.quality);
				}
			}
			// After delivering, mark processed and cleanup tracking structures
			{
				std::lock_guard<std::mutex> lg(g_TreasureMutex);
				g_TreasureProcessed.insert(requestId);
				g_TreasureCandidates.erase(requestId);
			}
		}
	}

	// Process Discord webhook batching (delegate to GlobalChatHandler)
	if (g_ChatHandler) {
		g_ChatHandler->ProcessBatch();
	}

	// Process tribe message batching (delegate to TribeChannelSubscriptionManager)
	if (g_TribeChannelManager) {
		g_TribeChannelManager->ProcessBatches();
	}
}

APrimalStructureClaimFlag* Hook_APrimalStructureClaimFlag_GetLandClaim(UWorld* ForWorld, const FVector* AtLocation, float SearchRadius) {
	// Try the original function first
	auto flag = APrimalStructureClaimFlag_GetLandClaim_original(ForWorld, AtLocation, SearchRadius);
	if (flag || SearchRadius != 25000.0f)
		return flag;

	// Find the closest island settlement flag
	const auto sgm = ArkApi::GetApiUtils().GetShooterGameMode();
	if (!sgm)
		return flag;

	const auto islandMap = sgm->AtlasIslandInfoField();
	float closestDistSq = FLT_MAX;

	// Search for the nearest island settlement flag
	for (const auto& island : islandMap) {
		APrimalStructureClaimFlag* islandFlag = island.Value->IslandSettlementFlagField().Get();
		if (!islandFlag) continue;
		float distSq = FVector::DistSquared(islandFlag->RootComponentField()->RelativeLocationField(), *AtLocation);
		if (distSq < closestDistSq) {
			closestDistSq = distSq;
			flag = islandFlag;
		}
	}

	return flag;
}

void Hook_AShooterPlayerController_ServerSendChatMessage_Impl(AShooterPlayerController* player_controller, FString* message, EChatSendMode::Type mode) {
	if (mode != EChatSendMode::GlobalChat || !Config::GetConfig().enableGlobalChat) {
		AShooterPlayerController_ServerSendChatMessage_Impl_original(player_controller, message, mode);
		return;
	}

	FString playerName = "Unknown";
	bool isAdmin = false;

	if (player_controller && player_controller->GetShooterPlayerState()) {
		player_controller->GetShooterPlayerState()->GetPlayerName(&playerName);

		// Check if player is an admin (has cheat manager enabled)
		auto cheatManager = player_controller->CheatManagerField();
		if (cheatManager) {
			auto shooterCheatManager = static_cast<UShooterCheatManager*>(cheatManager);
			if (shooterCheatManager) {
				isAdmin = true;
			}
		}
	}

	// Sanitize player name and message to prevent rich text injection
	std::string sanitizedPlayerName = StripRichText(playerName.ToString());
	std::string sanitizedMessage = StripRichText(message->ToString());

	nlohmann::json chatMsg = {
		 {"type", PSM_GlobalChat},
		 {"sender", GetServerId()},
		 {"peer", 0},
		 {"message", sanitizedMessage},
		 {"playerName", sanitizedPlayerName},
		 {"serverGrid", GetServerGrid()},
		 {"isAdmin", isAdmin},
		 {"raw", {{"bytes", {0}}, {"subtype", nullptr}}}
	};

	g_Redis->PublishMessage(Config::GetConfig().channelCluster, chatMsg);

	// Don't buffer locally - let it come back through Redis to avoid duplicates
	// The Redis message handler (PSM_GlobalChat case) will buffer it for Discord
}

void Hook_ATreasureMapManager_GiveNewTreasureMapToCharacter(ATreasureMapManager* This, UPrimalItem* TreasureMapItem, float Quality, AShooterCharacter* ForShooterChar) {
	// Safeguards from original function
	if (!ForShooterChar) return;
	if (!TreasureMapItem) return;
	if (Quality <= 0.0f) return;
	if (!This) return;
	UWorld* world = This->GetWorld();
	if (!world) return;
	if (ForShooterChar->IsPendingKillPending()) return;

	auto settings = world->GetWorldSettings(false, true);
	if (!settings) return;
	if (!settings->bSeamlessTravelWorldField()) return;

	const auto grid = static_cast<UShooterGameInstance*>(ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->GridInfoField();
	if (!grid) return;

	const auto serverInfo = grid->GetCurrentServerInfo();
	if (!serverInfo) return;
	FString region = serverInfo->HiddenAtlasIdField();

	if (Config::GetConfig().enableDetailedLogging)
		Log::GetLog()->info("[Treasure] Intercepting treasure map request with quality {} in reg {} ", Quality, region.ToString());

	FPendingTreasureMapSpawnInfo spawn;
	spawn.RequestId = This->RequestCounterField();
	spawn.TreasureMapItem = TreasureMapItem;
	spawn.Quality = Quality;
	spawn.MapGiveToCharacter = ForShooterChar;

	This->PendingSpawnTreasureMapsField().Add(spawn);

	nlohmann::json treasureMsg = {
		 {"type", PSM_FetchTreasureLocationSpecial},
		 {"sender", GetServerId()},
		 {"peer", 0},
		 {"requestId", spawn.RequestId},
		 {"quality", spawn.Quality},
		 {"region", region.ToString()},
		 {"raw", {{"bytes", {0}}, {"subtype", nullptr}}}
	};

	g_Redis->PublishMessage(Config::GetConfig().channelCluster, treasureMsg);
	// Do not schedule here; wait until first response is received to start delay window
	// Safe counter increment
	auto& counter = This->RequestCounterField();
	counter = (counter >= 0x7FFFFFFF) ? 0 : counter + 1;
}

PeerServerConnectionData* Hook_ASeamlessVolumeManager_SendPacketToPeerServer(ASeamlessVolumeManager* This, unsigned int Peer, PeerServerMessageType Type, TArray<unsigned char>* Bytes, bool Remote) {
	if (Type == PSM_FetchTreasureLocation || Type == PSM_FetchTreasureLocationSpecial) {
		nlohmann::json msg = {
				 {"type", Type},
				 {"sender", GetServerId()},
				 {"peer", Peer},
				 {"raw", {{"bytes", std::vector<unsigned char>(Bytes->GetData(), Bytes->GetData() + Bytes->Num())}, {"subtype", nullptr}}}
		};

		g_Redis->PublishMessage(Config::GetConfig().channelCluster, msg);

		auto* ret = new PeerServerConnectionData();
		ret->Success = true;
		ret->OtherServerId = Peer;
		return ret;
	}

	return ASeamlessVolumeManager_SendPacketToPeerServer_original(This, Peer, Type, Bytes, Remote);
}

void Hook_AShooterGameMode_InitOptions(AShooterGameMode* This, FString Options) {
	AShooterGameMode_InitOptions_original(This, Options);
	if (Config::GetConfig().enableOldStructureChecks) {
		Log::GetLog()->info("Disabling 'new' building foundation support checks");
		This->bUseNewStructureFoundationSupportChecksField() = false;
	}
	if (Config::GetConfig().enableUnlimitedRespecs) {
		Log::GetLog()->info("Enabling unlimited respecs");
		This->bAllowUnlimitedRespecsField() = true;
	}
}

// Plugin lifecycle
void Load() {
	Log::Get().Init("Atlas Glue");

	Config::Instance().Load();

	ArkApi::GetHooks().SetHook("AShooterGameMode.BeginPlay",
		&Hook_AShooterGameMode_BeginPlay, &AShooterGameMode_BeginPlay_original);
	ArkApi::GetHooks().SetHook("ASeamlessVolumeManager.SendPacketToPeerServer",
		&Hook_ASeamlessVolumeManager_SendPacketToPeerServer, &ASeamlessVolumeManager_SendPacketToPeerServer_original);

	if (Config::GetConfig().enableGlobalChat) {
		Log::GetLog()->info("Global chat enabled");
		ArkApi::GetHooks().SetHook("AShooterPlayerController.ServerSendChatMessage_Implementation",
			&Hook_AShooterPlayerController_ServerSendChatMessage_Impl, &AShooterPlayerController_ServerSendChatMessage_Impl_original);
	}

	if (Config::GetConfig().enableExtendedTax) {
		Log::GetLog()->info("Extended tax system enabled");
		ArkApi::GetHooks().SetHook("APrimalStructureClaimFlag.GetLandClaim", &Hook_APrimalStructureClaimFlag_GetLandClaim, &APrimalStructureClaimFlag_GetLandClaim_original);
	}

	if (Config::GetConfig().enableTreasureBottlesFix) {
		Log::GetLog()->info("Treasure bottle fix enabled");
		ArkApi::GetHooks().SetHook("ATreasureMapManager.GiveNewTreasureMapToCharacter",
			&Hook_ATreasureMapManager_GiveNewTreasureMapToCharacter, &ATreasureMapManager_GiveNewTreasureMapToCharacter_original);
	}

	ArkApi::GetHooks().SetHook("AShooterGameMode.InitOptions", &Hook_AShooterGameMode_InitOptions, &AShooterGameMode_InitOptions_original);

	// Always hook engine tick to flush treasure picks on game thread
	ArkApi::GetHooks().SetHook("UGameEngine.Tick", &Hook_UGameEngine_Tick, &UGameEngine_Tick_original);
}

void Unload() {
	ArkApi::GetHooks().DisableHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay);
	ArkApi::GetHooks().DisableHook("ASeamlessVolumeManager.SendPacketToPeerServer", &Hook_ASeamlessVolumeManager_SendPacketToPeerServer);
	if (Config::GetConfig().enableGlobalChat)
		ArkApi::GetHooks().DisableHook("AShooterPlayerController.ServerSendChatMessage_Implementation", &Hook_AShooterPlayerController_ServerSendChatMessage_Impl);

	if (Config::GetConfig().enableExtendedTax)
		ArkApi::GetHooks().DisableHook("APrimalStructureClaimFlag.GetLandClaim", &Hook_APrimalStructureClaimFlag_GetLandClaim);

	if (Config::GetConfig().enableTreasureBottlesFix)
		ArkApi::GetHooks().DisableHook("ATreasureMapManager.GiveNewTreasureMapToCharacter", &Hook_ATreasureMapManager_GiveNewTreasureMapToCharacter);

	ArkApi::GetHooks().DisableHook("AShooterGameMode.InitOptions", &Hook_AShooterGameMode_InitOptions);
	ArkApi::GetHooks().DisableHook("UGameEngine.Tick", &Hook_UGameEngine_Tick);

	if (g_ChatHandler) { delete g_ChatHandler; g_ChatHandler = nullptr; }
	if (g_TribeChannelManager) { g_TribeChannelManager->Stop(); delete g_TribeChannelManager; g_TribeChannelManager = nullptr; }
	if (g_DiscordLeader) { g_DiscordLeader->Stop(); delete g_DiscordLeader; g_DiscordLeader = nullptr; }
	if (g_DiscordWebhook) { delete g_DiscordWebhook; g_DiscordWebhook = nullptr; }
	stopbeating = true;
}

// Enhanced cleanup in DllMain
BOOL APIENTRY DllMain(HMODULE, DWORD ul_reason_for_call, LPVOID) {
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
