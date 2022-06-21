#include <API/Atlas/Atlas.h>
#include <string>
#include <random>
#include <iterator>
#include "json.hpp"

#define _WS2DEF_ // hack we already have these
#define _WINSOCK2API_ // hack we already have these
#include <cpp_redis/cpp_redis>

#pragma comment(lib, "AtlasApi.lib")
#pragma comment(lib, "cpp_redis.lib")
#pragma comment(lib, "tacopie.lib")

enum PeerServerMessageType
{
	PSM_Ping = 0x0,
	PSM_Pong = 0x1,
	PSM_FetchTreasureLocation = 0x2,
	PSM_FetchedTreasureLocation = 0x3,
	PSM_TravelLog = 0x4,
	PSM_MAX = 0x5,
};

static const std::string ChannelCluster = "seamless:cluster";
static const std::string ChannelServer = "seamless:";
cpp_redis::subscriber sub;
cpp_redis::client cli;

struct FetchTreasureStruct {
	unsigned int IslandId;					// 4
	unsigned int RequestId;					// 4
	float InQuality;							// 4
}; // 12

struct FetchedTreasureStruct {
	unsigned int ForServerId;				// 4 requestID
	FVector	Location;						// 12
	uint8_t MessageType;						// 4
	unsigned int OriginatingServerId;	// 4
}; // 12

template<typename Iter, typename RandomGenerator>
Iter RandomElement(Iter start, Iter end, RandomGenerator& g) {
	std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
	std::advance(start, dis(g));
	return start;
}
template<typename Iter>
Iter RandomElement(Iter start, Iter end) {
	static std::random_device rd;
	static std::mt19937 gen(rd());
	return RandomElement(start, end, gen);
}

#define POLYNOMIAL 0xD8  /* 11011 followed by 0's */

unsigned crc(unsigned char const* data, size_t len)
{
	unsigned crc = 0;
	if (data == NULL)
		return 0;
	crc = ~crc & 0xff;
	while (len--) {
		crc ^= *data++;
		for (unsigned k = 0; k < 8; k++)
			crc = crc & 1 ? (crc >> 1) ^ 0xb2 : crc >> 1;
	}
	return crc ^ 0xff;
}

DECLARE_HOOK(AShooterGameMode_BeginPlay, void, AShooterGameMode*, float);
DECLARE_HOOK(ASeamlessVolumeManager_SendPacketToPeerServer, PeerServerConnectionData*, ASeamlessVolumeManager*, unsigned int, PeerServerMessageType, TArray<unsigned char>*, bool);
DECLARE_HOOK(ASeamlessVolumeManager_NotifyPeerServerOfTravelLog, PeerServerConnectionData*, ASeamlessVolumeManager*, unsigned int DestinationServerId, unsigned __int64 TravelLogLine, TArray<unsigned char, FDefaultAllocator>* TravelData);

// temp
DECLARE_HOOK(ASeamlessVolumeManager_ApplySeamlessTravelDataBody, char, ASeamlessVolumeManager*, unsigned __int64 LogLineId, const TArray<unsigned char, FDefaultAllocator>* BodyBytes, unsigned int OriginServerId, bool bAbortedTravel);

void Load() {
	Log::Get().Init("NoSeamless");
	ArkApi::GetHooks().SetHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay, &AShooterGameMode_BeginPlay_original);
	ArkApi::GetHooks().SetHook("ASeamlessVolumeManager.SendPacketToPeerServer", &Hook_ASeamlessVolumeManager_SendPacketToPeerServer, &ASeamlessVolumeManager_SendPacketToPeerServer_original);
	ArkApi::GetHooks().SetHook("ASeamlessVolumeManager.NotifyPeerServerOfTravelLog", &Hook_ASeamlessVolumeManager_NotifyPeerServerOfTravelLog, &ASeamlessVolumeManager_NotifyPeerServerOfTravelLog_original);
	
	// temp
	ArkApi::GetHooks().SetHook("ASeamlessVolumeManager.ApplySeamlessTravelDataBody", &Hook_ASeamlessVolumeManager_ApplySeamlessTravelDataBody, &ASeamlessVolumeManager_ApplySeamlessTravelDataBody_original);

}

void Unload() {
	ArkApi::GetHooks().DisableHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay);
	ArkApi::GetHooks().DisableHook("ASeamlessVolumeManager.SendPacketToPeerServer", &Hook_ASeamlessVolumeManager_SendPacketToPeerServer);
	ArkApi::GetHooks().DisableHook("ASeamlessVolumeManager.NotifyPeerServerOfTravelLog", &Hook_ASeamlessVolumeManager_NotifyPeerServerOfTravelLog);
}

// get server packed id
static const unsigned int ServerId() {
	const auto grid = static_cast<UShooterGameInstance*> (ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->GridInfoField();
	return grid->GetCurrentServerId();
};

// get a random island for a server
int RandomIslandIdForServerId(int serverId) {
	const auto sgm = ArkApi::GetApiUtils().GetShooterGameMode();
	const auto islandMap = sgm->AtlasIslandInfoField();
	std::vector<int> islands;

	for (auto i : islandMap) {
		if (ServerId() == i.Value->ParentServerIdField()) {
			islands.push_back(i.Key);
		}
	}
	if (islands.size() == 0)
		return 0;

	return *RandomElement(islands.begin(), islands.end());
};

// get server grid string
static const std::string ServerGrid() {
	const auto grid = static_cast<UShooterGameInstance*> (ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->GridInfoField();
	const char* x[17] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q" };
	const char* y[17] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17" };

	std::string gridStr = x[grid->GetCurrentServerInfo()->gridXField()];
	gridStr += y[grid->GetCurrentServerInfo()->gridYField()];
	return gridStr;
};

// get redis channel
static const std::string ChannelForServerId(unsigned int id) {
	return ChannelServer + std::to_string(id);
};

void ConnectCallback(const std::string& host, std::size_t port, cpp_redis::connect_state status) {
	switch (status) {
	case cpp_redis::connect_state::dropped:
		Log::GetLog()->info("redis connection dropped");
	case cpp_redis::connect_state::ok:
		Log::GetLog()->info("redis ready");
	}
}

void AuthCallback(cpp_redis::reply& reply) {
	if (reply.is_error()) {
		Log::GetLog()->info("invalid redis password");
	}
}

void FetchTreasure(TArray<unsigned char> *bytes, unsigned int sender) {
	nlohmann::json json;
	FetchTreasureStruct dest;
	memcpy(&dest, bytes->GetData(), sizeof(FetchTreasureStruct));
	const auto sgm = ArkApi::GetApiUtils().GetShooterGameMode();
	const auto tmm = sgm->TreasureMapManagerField();
	const auto islandMap = sgm->AtlasIslandInfoField();
	const auto grid = static_cast<UShooterGameInstance*> (ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->GridInfoField();
	FVector outLocation;
	float outQuality;
	bool outCave;
	int tries = 3;
	while (tries > 0) {
		auto island = islandMap.Find(dest.IslandId);
		if (island == NULL) return;
		if (ServerId() == (*island)->ParentServerIdField()) {
			auto out = tmm->GenerateTreasureLocationOnIsland(dest.IslandId, dest.InQuality, &outLocation, &outCave, &outQuality, NULL);
			if (out == 0) {
				dest.IslandId = RandomIslandIdForServerId(ServerId());
				continue;
			}

			// translate to world location
			FVector mapGlobal;
			grid->ServerLocationToGlobalLocation(&mapGlobal, ServerId(), outLocation);
			json = {
				{"type", PSM_FetchedTreasureLocation},
				{"sender", ServerId()},
				{"peer", sender},
				{"cave", outCave},
				{"quality", outQuality},
				{"raw", {
						{"bytes", {0}},
						{"subtype", NULL}
					}
				},
				{"X", mapGlobal.X},
				{"Y", mapGlobal.Y},
				{"Z", mapGlobal.Z},
				{"requestId", dest.RequestId}
			};

			cli.publish(ChannelForServerId(sender), json.dump());
			cli.commit();
			break;
		}
		else {
			dest.IslandId = RandomIslandIdForServerId(ServerId());
		}
		tries--;
	}
}

char FetchedTreasure(int requestId, FVector* location, bool cave, float quality) {
	const auto sgm = ArkApi::GetApiUtils().GetShooterGameMode();
	const auto tmm = sgm->TreasureMapManagerField();
	return tmm->OnTreasureChestLocationFound(requestId, location, cave, quality);
}

void TravelLog(unsigned __int64 TravelLogLine, TArray<unsigned char>* bytes) {
	const auto sgm = ArkApi::GetApiUtils().GetShooterGameMode();
	TSharedPtr<TArray<unsigned char>> copy = MakeShareable(new TArray<unsigned char>(*bytes));
	Log::GetLog()->info("got travel log {} {} {} {}", TravelLogLine, copy->Num(), crc(copy->GetData(), copy->Num()), crc(bytes->GetData(), bytes->Num()));
	
	sgm->SharedLogTravelNotification(TravelLogLine, &copy);
}

void HandleMessage(const std::string& chan, const std::string& msg) {
	// parse and check validity
	auto json = nlohmann::json::parse(msg, NULL, false, true);
	if (json["raw"].is_null())
		return;

	auto binary = json["raw"]["bytes"];
	auto bytes = new TArray<unsigned char>();
	for (auto v : binary) {
		bytes->Push(v);
	}

	unsigned int peer, sender, size;
	unsigned char type;
	json["peer"].get_to(peer);
	json["sender"].get_to(sender);
	json["type"].get_to(type);
	size = bytes->Num();
	switch (type) {
	case PSM_Ping:
	case PSM_Pong:
		break;
	case PSM_TravelLog:
		TravelLog(json["log"], bytes);
		break;
	case PSM_FetchTreasureLocation:
		FetchTreasure(bytes, sender);
		break;
	case PSM_FetchedTreasureLocation:
		FVector location;

		json["X"].get_to(location.X);
		json["Y"].get_to(location.Y);
		json["Z"].get_to(location.Z);

		FetchedTreasure(json["requestId"], &location, json["cave"], json["quality"]);

		break;
	}
}

// Connect to Redis channel and create a client for publishing
void ConnectRedisChannel(FRedisDatabaseConnectionInfo info) {
	// Connect sub and client
	sub.connect(info.URL, info.Port, &ConnectCallback, 0U, 10000, 0U);
	cli.connect(info.URL, info.Port, &ConnectCallback, 0U, 10000, 0U);

	// auth if needed, this silently fails as auth can fail if no password is set and server op leaves defaults...
	if (info.Password != "") {
		sub.auth(info.Password, &AuthCallback);
		cli.auth(info.Password, &AuthCallback);
	}

	// commit the auth calls
	cli.commit();
	sub.commit();

	// subscribe to seamless channels
	Log::GetLog()->info("subscribing to {}", ChannelCluster);
	sub.subscribe(ChannelCluster, &HandleMessage);
	Log::GetLog()->info("subscribing to {}", ChannelForServerId(ServerId()));
	sub.subscribe(ChannelForServerId(ServerId()), &HandleMessage);
	sub.commit();
}

void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* This, float a2) {
	// start up our redis channel
	auto ConnectionMap = FDatabaseRedisShared::ConnectionLookups();

	// connect to the default server
	for (auto k : ConnectionMap) {
		if (k.Key.Equals("Default")) {
			ConnectRedisChannel(k.Value);
			break;
		}
	}
	AShooterGameMode_BeginPlay_original(This, a2);
}

char Hook_ASeamlessVolumeManager_ApplySeamlessTravelDataBody(ASeamlessVolumeManager* This, unsigned __int64 LogLineId, const TArray<unsigned char, FDefaultAllocator>* BodyBytes, unsigned int OriginServerId, bool bAbortedTravel) {
	Log::GetLog()->warn("apply {} {} {} {}", LogLineId, OriginServerId, BodyBytes->Num(), crc(BodyBytes->GetData(), BodyBytes->Num()));
	return ASeamlessVolumeManager_ApplySeamlessTravelDataBody_original(This, LogLineId, BodyBytes, OriginServerId, bAbortedTravel);
}

PeerServerConnectionData* Hook_ASeamlessVolumeManager_NotifyPeerServerOfTravelLog(ASeamlessVolumeManager* This, unsigned int DestinationServerId, unsigned __int64 TravelLogLine, TArray<unsigned char>* TravelData) {
	nlohmann::json::binary_t bytes;
	bytes.resize(TravelData->Num());
	
	for (int i = 0; i < TravelData->Num(); i++) {
		bytes[i] = TravelData->GetData()[i];
	}
	nlohmann::json json = {
		{"type", PSM_TravelLog},
		{"sender", ServerId()},
		{"log", TravelLogLine},
		{"peer", DestinationServerId},
		{"raw", bytes}
	};
	Log::GetLog()->warn("sending player to {} {} {} ", DestinationServerId, TravelData->Num(), crc(TravelData->GetData(), TravelData->Num()));
	cli.publish(ChannelForServerId(DestinationServerId), json.dump());
	cli.commit();
	return ASeamlessVolumeManager_NotifyPeerServerOfTravelLog_original(This, DestinationServerId, TravelLogLine, TravelData);
}

PeerServerConnectionData* Hook_ASeamlessVolumeManager_SendPacketToPeerServer(ASeamlessVolumeManager* This, unsigned int Peer, PeerServerMessageType Type, TArray<unsigned char>* Bytes, bool Remote) {
	const auto tmm = ArkApi::GetApiUtils().GetShooterGameMode()->TreasureMapManagerField();
	const auto instance = static_cast<UShooterGameInstance*> (ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField());
	nlohmann::json json;

	nlohmann::json::binary_t bytes;
	bytes.resize(Bytes->Num());
	for (int i = 0; i < Bytes->Num(); i++) {
		bytes[i] = Bytes->GetData()[i];
	}

	// Pack into json structure
	json = {
		{"type", Type},
		{"sender", ServerId()},
		{"peer", Peer},
		{"raw", bytes}
	};

	switch (Type) {
	case PSM_Ping:
	case PSM_Pong:
	case PSM_TravelLog:

		//Log::GetLog()->warn("send travel log {} {}", Bytes->Num(), crc(Bytes->GetData(), Bytes->Num()));
		break;
	default:
		cli.publish(ChannelCluster, json.dump());
	}

	cli.commit();

	// eat the actual call to ASeamlessVolumeManager_SendPacketToPeerServer_original(This, Peer, Type, Bytes, Remote);
	return NULL;//ASeamlessVolumeManager_SendPacketToPeerServer_original(This, Peer, Type, Bytes, Remote);
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