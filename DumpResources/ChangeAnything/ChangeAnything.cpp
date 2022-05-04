#include <API/Atlas/Atlas.h>
#include "json.hpp"
#include <filesystem>
#include <fstream>
#pragma comment(lib, "AtlasApi.lib")

DECLARE_HOOK(AShooterGameMode_BeginPlay, void, AShooterGameMode*);
DECLARE_HOOK(AShooterGameMode_InitOptions, void, AShooterGameMode*, FString);

void Load() {
	Log::Get().Init("Change Anything");
	ArkApi::GetHooks().SetHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay, &AShooterGameMode_BeginPlay_original);
	ArkApi::GetHooks().SetHook("AShooterGameMode.InitOptions", &Hook_AShooterGameMode_InitOptions, &AShooterGameMode_InitOptions_original);
}

void Unload() {
	ArkApi::GetHooks().DisableHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay);
	ArkApi::GetHooks().DisableHook("AShooterGameMode.InitOptions", &Hook_AShooterGameMode_InitOptions);
}

template<typename T>
T Get(UProperty* This, UObject* object) {
	if (sizeof(T) != This->ElementSizeField())
		throw std::invalid_argument("Expected size does not match property size.");
	return *((T*)(object + This->Offset_InternalField()));
}

template<typename T>
void Set(UProperty* This, UObject* object, T value)
{
	if (sizeof(T) != This->ElementSizeField())
		throw std::invalid_argument("Expected size does not match property size.");
	*((T*)(object + This->Offset_InternalField())) = value;
}

nlohmann::json getVariableData(UObject* o, UClass* c, UProperty* p) {
	nlohmann::json json;
	auto type = p->ClassField()->NameField().ToString().ToString();
	if (c->NameField().ToString().Compare(FString("AutoFarm_C"))) {
		if (p->NameField().ToString().ToString() == "HarvestInterval")
			Set<float>(p, o, 2.0f);
		else if (p->NameField().ToString().ToString() == "HarvestRadius")
			Set<float>(p, o, 50000.0f);
		else if (p->NameField().ToString().ToString() == "HarvestQuantity")
			Set<int>(p, o, 50);
	}
	if (c->NameField().ToString().ToString() == "Storehouse_C" || c->NameField().ToString().ToString() == "PrimalStructureStorehouse") {
		if (p->NameField().ToString().ToString() == "CollectionPercentage") {
			Log::GetLog()->info("Class: {} Object: {}", c->NameField().ToString().ToString(), o->NameField().ToString().ToString());
			Set<float>(p, o, 0.90f);
		}
		else if (p->NameField().ToString().ToString() == "TransportInterval")
			Set<float>(p, o, 2.0f);
	}

	if (type == "BoolProperty") {
		auto v = Get<bool>(p, o);
		return  { {"type", type}, {"value" , v} };
	}
	else if (type == "FloatProperty") {
		auto v = Get<float>(p, o);
		return  { {"type", type}, {"value" , v} };
	}
	else if (type == "DoubleProperty") {
		auto v = Get<double>(p, o);
		return  { {"type", type}, {"value" , v} };
	}
	else if (type == "UInt64Property") {
		auto v = Get<uint64>(p, o);
		return  { {"type", type}, {"value" , v} };
	}
	else if (type == "Int64Property") {
		auto v = Get<int64>(p, o);
		return  { {"type", type}, {"value" , v} };
	}
	else if (type == "UInt32Property") {
		auto v = Get<uint32>(p, o);
		return  { {"type", type}, {"value" , v} };
	}
	else if (type == "Int32Property") {
		auto v = Get<int32>(p, o);
		return  { {"type", type}, {"value" , v} };
	}
	else if (type == "IntProperty") {
		auto v = Get<int>(p, o);
		return  { {"type", type}, {"value" , v} };
	}
	else if (type == "ByteProperty") {
		auto v = Get<byte>(p, o);
		return  { {"type", type}, {"value" , v} };
	}
	else if (type == "StrProperty") {
		auto v = Get<FString>(p, o);
		return  { {"type", type}, {"value" , v.ToString()} };
	}
	else if (type == "NameProperty") {
		auto v = Get<FName>(p, o);
		return { {"type", type}, {"value" ,  v.ToString().ToString()} };
	}
	else {
		return  { {"type", type} };
	}
}

nlohmann::json searchObjects() {
	nlohmann::json json = {};
	TArray<UObject*> objects;
	Globals::GetObjectsOfClass(UObject::GetPrivateStaticClass(), &objects, true, EObjectFlags::RF_NoFlags);
	for (auto o : objects) {
		auto c = o->ClassField();
		json[o->NameField().ToString().ToString()] = { {"class", c->NameField().ToString().ToString()}, {"properties", {} } };

		if (c != NULL) {
			auto p = c->PropertyLinkField();
			while (p != NULL) {
				auto v = getVariableData(o, c, p);
				json[o->NameField().ToString().ToString()]["properties"][p->NameField().ToString().ToString()] = v;
				p = p->PropertyLinkNextField();
			}
		}
	}
	return json;
}

void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* This) {
	auto j = searchObjects();
	std::filesystem::create_directory("changeanything");
	std::ofstream file("changeanything/objects.json");
	file << std::setw(2) << j;
	file.flush();
	file.close();
	AShooterGameMode_BeginPlay_original(This);
}


void Hook_AShooterGameMode_InitOptions(AShooterGameMode* This, FString Options) {
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