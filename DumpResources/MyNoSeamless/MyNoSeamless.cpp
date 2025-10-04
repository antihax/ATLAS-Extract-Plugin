#include <API/Atlas/Atlas.h>
#include <string>
#include <random>
#include <iterator>
#include "json.hpp"
#include <thread>

#define _WS2DEF_ // hack we already have these
#define _WINSOCK2API_ // hack we already have these
#include <cpp_redis/cpp_redis>

#pragma comment(lib, "AtlasApi.lib")
#pragma comment(lib, "cpp_redis.lib")
#pragma comment(lib, "tacopie.lib")

// Global state
std::map<int, std::chrono::system_clock::time_point> g_LastSeen;
std::thread heartbeat;
std::atomic<bool> stopbeating{false};

enum PeerServerMessageType
{
    PSM_Ping = 0x0,
    PSM_Pong = 0x1,
    PSM_FetchTreasureLocation = 0x2,
    PSM_FetchedTreasureLocation = 0x3,
    PSM_TravelLog = 0x4,
    PSM_MAX = 0x5,
    PSM_FetchTreasureLocationSpecial = 0x6,
    PSM_HeartBeat = 0x7,
    PSM_GlobalChat = 0x8,
};

static const std::string ChannelCluster = "seamless:cluster";
static const std::string ChannelServer = "seamless:";
cpp_redis::subscriber sub;
cpp_redis::client cli;

// Consolidated utility functions
static unsigned int GetServerId() {
    return static_cast<UShooterGameInstance*>(ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())
        ->GridInfoField()->GetCurrentServerId();
}

static std::string GetServerGrid() {
    static const char* x[17] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q" };
    static const char* y[17] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17" };
    
    const auto serverInfo = static_cast<UShooterGameInstance*>(ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())
        ->GridInfoField()->GetCurrentServerInfo();
    return std::string(x[serverInfo->gridXField()]) + y[serverInfo->gridYField()];
}

static std::string GetChannelForServerId(unsigned int id) {
    return ChannelServer + std::to_string(id);
}

const std::unordered_map<std::string, ULevel*> getIslandLevels() {
   std::unordered_map<std::string, ULevel*> levels;
   for (auto level : ArkApi::GetApiUtils().GetWorld()->LevelsField()) {
      if (level->SubLevelGroupUniqueIDField() < 0)
         continue;

      int id = level->SubLevelGroupUniqueIDField();
      FString name;
      level->NameField().ToString(&name);

      std::string idStr = std::to_string(id);
      levels[idStr] = level;
   }
   return levels;
}

static int GetRandomIslandId() {
   const auto levels = getIslandLevels();

   if (levels.empty()) {
      return 0;
   }

   // Convert level keys to integers and select randomly
   std::vector<int> validIds;
   for (const auto& level : levels) {
      try {
         int levelId = std::stoi(level.first);
         validIds.push_back(levelId);
      }
      catch (const std::exception& e) {
         Log::GetLog()->warn("[Island] Invalid level ID format: '{}'", level.first);
      }
   }

   if (validIds.empty()) {
      return 0;
   }

   static thread_local std::mt19937 gen(std::random_device{}());
   std::uniform_int_distribution<> dis(0, validIds.size() - 1);

   int selectedId = validIds[dis(gen)];
   return selectedId;
}

static float CalculateQualityBoost() {
    static thread_local std::mt19937 gen(std::random_device{}());
    static thread_local std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    
    const float random = dis(gen);
    
    if (random < 0.7f) return (random / 0.7f) * 5.0f;
    if (random < 0.9f) return 5.0f + std::pow((random - 0.7f) / 0.2f, 2.0f) * 5.0f;
    if (random < 0.98f) return 10.0f + std::pow((random - 0.9f) / 0.08f, 3.0f) * 5.0f;
    return 15.0f + std::pow((random - 0.98f) / 0.02f, 5.0f) * 5.0f;
}

static void PublishMessage(const std::string& channel, const nlohmann::json& msg) {
    cli.publish(channel, msg.dump());
    cli.commit();
}

// Unified treasure handling function
static void HandleTreasureRequest(unsigned int sender, int requestId, float quality, bool isSpecial) {
    quality += CalculateQualityBoost();

    const auto sgm = ArkApi::GetApiUtils().GetShooterGameMode();
    const auto tmm = sgm->TreasureMapManagerField();
    const auto grid = static_cast<UShooterGameInstance*>(ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->GridInfoField();

    for (int tries = 3; tries > 0; --tries) {
        const int islandId = GetRandomIslandId();
        if (islandId == 0) {
           continue;
        }

        FVector outLocation, mapGlobal;
        float outQuality;
        bool outCave;

        if (tmm->GenerateTreasureLocationOnIsland(islandId, quality, &outLocation, &outCave, &outQuality, nullptr)) {
            grid->ServerLocationToGlobalLocation(&mapGlobal, GetServerId(), outLocation);

            nlohmann::json response = {
                {"type", PSM_FetchedTreasureLocation},
                {"sender", GetServerId()},
                {"peer", sender},
                {"cave", outCave},
                {"quality", outQuality},
                {"X", mapGlobal.X},
                {"Y", mapGlobal.Y},
                {"Z", mapGlobal.Z},
                {"requestId", requestId},
                {"raw", {{"bytes", {0}}, {"subtype", nullptr}}}
            };

            PublishMessage(GetChannelForServerId(sender), response);
            return;
        }
    }
    Log::GetLog()->warn("[Treasure] Failed to generate treasure for sender {}", sender);
}

// Simplified message handler
void HandleMessage(const std::string& chan, const std::string& msg) {
    auto json = nlohmann::json::parse(msg, nullptr, false, true);
    if (json["raw"].is_null()) return;

    const auto sender = json["sender"].get<unsigned int>();
    const auto type = json["type"].get<unsigned char>();

    // Ignore our own treasure requests
    if ((type == PSM_FetchTreasureLocation || type == PSM_FetchTreasureLocationSpecial) && sender == GetServerId()) {
        return;
    }

    switch (type) {
        case PSM_FetchTreasureLocation: {
            struct { unsigned int IslandId, RequestId; float InQuality; } request;
            auto binary = json["raw"]["bytes"];
            if (binary.size() >= sizeof(request)) {
                std::copy_n(binary.begin(), sizeof(request), reinterpret_cast<unsigned char*>(&request));
                HandleTreasureRequest(sender, request.RequestId, request.InQuality, false);
            }
            break;
        }
        
        case PSM_FetchTreasureLocationSpecial:
            HandleTreasureRequest(sender, json["requestId"], json["quality"], true);
            break;

        case PSM_FetchedTreasureLocation: {
            FVector location{json["X"], json["Y"], json["Z"]};
            ArkApi::GetApiUtils().GetShooterGameMode()->TreasureMapManagerField()
                ->OnTreasureChestLocationFound(json["requestId"], &location, json["cave"], json["quality"]);
            break;
        }

        case PSM_HeartBeat:
            g_LastSeen[sender] = std::chrono::system_clock::now();
            break;

        case PSM_GlobalChat: {
            FString playerName(json["playerName"].get<std::string>());
            ArkApi::GetApiUtils().SendChatMessageToAll(playerName, "[{}] {}", 
                json["serverGrid"].get<std::string>(),
                json["message"].get<std::string>());
            break;
        }
    }
}

// Connection callbacks
void ConnectCallback(const std::string&, std::size_t, cpp_redis::connect_state status) {
   
}

void AuthCallback(cpp_redis::reply& reply) {
    if (reply.is_error()) Log::GetLog()->warn("Redis auth failed {}", reply.error());
}

void ConnectRedisChannel(const FRedisDatabaseConnectionInfo& info) {
    // Connect and authenticate
    sub.connect(info.URL, info.Port, &ConnectCallback, 0U, 10000, 0U);
    cli.connect(info.URL, info.Port, &ConnectCallback, 0U, 10000, 0U);
    
    if (!info.Password.empty()) {
        sub.auth(info.Password, &AuthCallback);
        cli.auth(info.Password, &AuthCallback);
    }

    sub.commit();
    cli.commit();

    // Subscribe to channels
    sub.subscribe(ChannelCluster, &HandleMessage);
    sub.subscribe(GetChannelForServerId(GetServerId()), &HandleMessage);
    sub.commit();
    
    Log::GetLog()->info("Redis channels subscribed for server {}", GetServerId());

    // Start heartbeat
    heartbeat = std::thread([]() {
        while (!stopbeating) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            nlohmann::json heartbeatMsg = {
                {"type", PSM_HeartBeat},
                {"sender", GetServerId()},
                {"peer", 0},
                {"raw", {{"bytes", {0}}, {"subtype", nullptr}}}
            };
            PublishMessage(ChannelCluster, heartbeatMsg);
        }
    });
}

// Hook declarations and implementations
DECLARE_HOOK(AShooterGameMode_BeginPlay, void, AShooterGameMode*, float);
DECLARE_HOOK(ASeamlessVolumeManager_SendPacketToPeerServer, PeerServerConnectionData*, ASeamlessVolumeManager*, unsigned int, PeerServerMessageType, TArray<unsigned char>*, bool);
DECLARE_HOOK(ATreasureMapManager_GiveNewTreasureMapToCharacter, void, ATreasureMapManager*, UPrimalItem*, float, AShooterCharacter*);
DECLARE_HOOK(AShooterPlayerController_ServerSendChatMessage_Impl, void, AShooterPlayerController*, FString*, EChatSendMode::Type);

void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* This, float a2) {
    const auto connectionMap = FDatabaseRedisShared::ConnectionLookups();
    for (const auto& conn : connectionMap) {
        if (conn.Key.Equals("Default")) {
            ConnectRedisChannel(conn.Value);
            break;
        }
    }
    AShooterGameMode_BeginPlay_original(This, a2);
}

void Hook_AShooterPlayerController_ServerSendChatMessage_Impl(AShooterPlayerController* player_controller, FString* message, EChatSendMode::Type mode) {
    if (mode != EChatSendMode::GlobalChat) {
        AShooterPlayerController_ServerSendChatMessage_Impl_original(player_controller, message, mode);
        return;
    }

    FString playerName = "Unknown";
    if (player_controller && player_controller->GetPlayerCharacter()) {
        playerName = player_controller->GetPlayerCharacter()->PlayerNameField();
    }

    nlohmann::json chatMsg = {
        {"type", PSM_GlobalChat},
        {"sender", GetServerId()},
        {"peer", 0},
        {"message", message->ToString()},
        {"playerName", playerName.ToString()},
        {"serverGrid", GetServerGrid()},
        {"raw", {{"bytes", {0}}, {"subtype", nullptr}}}
    };

    PublishMessage(ChannelCluster, chatMsg);
}

void Hook_ATreasureMapManager_GiveNewTreasureMapToCharacter(ATreasureMapManager* This, UPrimalItem* TreasureMapItem, float Quality, AShooterCharacter* ForShooterChar) {
    if (!ForShooterChar) return;

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
        {"raw", {{"bytes", {0}}, {"subtype", nullptr}}}
    };

    PublishMessage(ChannelCluster, treasureMsg);

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

        PublishMessage(ChannelCluster, msg);

        auto* ret = new PeerServerConnectionData();
        ret->Success = true;
        ret->OtherServerId = Peer;
        return ret;
    }

    return ASeamlessVolumeManager_SendPacketToPeerServer_original(This, Peer, Type, Bytes, Remote);
}

// Plugin lifecycle
void Load() {
    Log::Get().Init("NoSeamless");
    
    ArkApi::GetHooks().SetHook("AShooterGameMode.BeginPlay", 
        &Hook_AShooterGameMode_BeginPlay, &AShooterGameMode_BeginPlay_original);
    ArkApi::GetHooks().SetHook("ASeamlessVolumeManager.SendPacketToPeerServer", 
        &Hook_ASeamlessVolumeManager_SendPacketToPeerServer, &ASeamlessVolumeManager_SendPacketToPeerServer_original);
    ArkApi::GetHooks().SetHook("ATreasureMapManager.GiveNewTreasureMapToCharacter", 
        &Hook_ATreasureMapManager_GiveNewTreasureMapToCharacter, &ATreasureMapManager_GiveNewTreasureMapToCharacter_original);
    ArkApi::GetHooks().SetHook("AShooterPlayerController.ServerSendChatMessage_Implementation", 
        &Hook_AShooterPlayerController_ServerSendChatMessage_Impl, &AShooterPlayerController_ServerSendChatMessage_Impl_original);
}

void Unload() {
    ArkApi::GetHooks().DisableHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay);
    ArkApi::GetHooks().DisableHook("ASeamlessVolumeManager.SendPacketToPeerServer", &Hook_ASeamlessVolumeManager_SendPacketToPeerServer);
    ArkApi::GetHooks().DisableHook("ATreasureMapManager.GiveNewTreasureMapToCharacter", &Hook_ATreasureMapManager_GiveNewTreasureMapToCharacter);
    ArkApi::GetHooks().DisableHook("AShooterPlayerController.ServerSendChatMessage_Implementation", &Hook_AShooterPlayerController_ServerSendChatMessage_Impl);
}

BOOL APIENTRY DllMain(HMODULE, DWORD ul_reason_for_call, LPVOID) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        Load();
        break;
    case DLL_PROCESS_DETACH:
        stopbeating = true;
        if (heartbeat.joinable()) heartbeat.join();
        Unload();
        break;
    }
    return TRUE;
}
