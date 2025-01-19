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
FString GetText(UProperty* This, UObject* object) {
	FString text;
	auto me = (T*)(This);
	me->ExportTextItem(&text, (void*)(object + (This->Offset_InternalField())), 0, object, 0, 0);
	return text;
}

template<typename T>
T* GetPtr(UProperty* This, UObject* object) {
	if (sizeof(T*) != This->ElementSizeField())
		throw std::invalid_argument("Expected size does not match property size.");
	return (T*)(object + This->Offset_InternalField());
}

template<typename T>
void Set(UProperty* This, UObject* object, T value)
{
	if (sizeof(T) != This->ElementSizeField())
		throw std::invalid_argument("Expected size does not match property size.");
	*((T*)(object + This->Offset_InternalField())) = value;
}

// Review UPropertyToJsonValue / ConvertScalarUPropertyToJsonValue
nlohmann::json getVariableData(UObject* o, UClass* c, UProperty* p) {
	nlohmann::json json;
	auto type = p->ClassField()->NameField().ToString().ToString();
	/*if (c->NameField().ToString().Compare(FString("AutoFarm_C"))) {
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
	}*/

	if (type == "UserDefinedStruct") {
		Log::GetLog()->info("{} ", c->NameField().ToString().ToString());
		auto p = c->PropertyLinkField();
		while (p != NULL) {
		}
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
	else if (type == "UInt16Property") {
		auto v = Get<uint16>(p, o);
		return  { {"type", type}, {"value" , v} };
	}
	else if (type == "Int16Property") {
		auto v = Get<int16>(p, o);
		return  { {"type", type}, {"value" , v} };
	}
	else if (type == "UInt8Property") {
		auto v = Get<uint8>(p, o);
		return  { {"type", type}, {"value" , v} };
	}
	else if (type == "Int8Property") {
		auto v = Get<int8>(p, o);
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
	else if (type == "ClassProperty") 
		return { {"type", type}, {"value", GetText<UClassProperty>(p, o).ToString() }};
	
	else if (type == "StructProperty") 
		return { {"type", type}, {"value", GetText<UStructProperty>(p, o).ToString() } };

	else if (type == "ArrayProperty")
		return { {"type", type}, {"value", GetText<UArrayProperty>(p, o).ToString() } };

	else if (type == "ObjectProperty")
		return { {"type", type}, {"value", GetText<UObjectProperty>(p, o).ToString() } };

	else if (type == "TextProperty")
		return { {"type", type}, {"value", GetText<UTextProperty>(p, o).ToString() } };

	else if (type == "DelegateProperty")
		return { {"type", type}, {"value", GetText<UDelegateProperty>(p, o).ToString() } };

	else if (type == "MulticastDelegateProperty")
		return { {"type", type}, {"value", GetText<UMulticastDelegateProperty>(p, o).ToString() } };
	
	else if (type == "WeakObjectProperty")
		return { {"type", type}, {"value", GetText<UWeakObjectProperty>(p, o).ToString() } };

	else if (type == "InterfaceProperty")
		return { {"type", type}, {"value", GetText<UInterfaceProperty>(p, o).ToString() } };

	
	
		/*
//		auto s = Get<UStructProperty*>(p, o);
	//	Log::GetLog()->info("{} {}", s->NameField().ToString().ToString());
		nlohmann::json json = {};
		json[o->NameField().ToString().ToString()] = { {"class", c->NameField().ToString().ToString()}, {"properties", {} } };
		Log::GetLog()->info("{} {}", o->NameField().ToString().ToString(), c->NameField().ToString().ToString());
		while (p != NULL) {
			auto v = getVariableData(o, c, p);
			json[o->NameField().ToString().ToString()]["properties"][p->NameField().ToString().ToString()] = v;
			Log::GetLog()->info("{} {}", p->NameField().ToString().ToString(), v);
			p = p->PropertyLinkNextField();
		}
		return  { {"type", type}, {"value" ,  json} };
	}*/
	else {
		return  { {"type", type} , {"size", p->ElementSizeField()} , {"offset", p->Offset_InternalField()}, {"dimensions", p->ArrayDimField()} };
	}
}

nlohmann::json searchObjects() {
	nlohmann::json json = {};
	TArray<UObject*> objects;
	Globals::GetObjectsOfClass(UClass::GetPrivateStaticClass(), &objects, true, EObjectFlags::RF_NoFlags);
	for (auto o : objects) {
		auto c = o->ClassField();
		json[o->NameField().ToString().ToString()] = { {"class", c->NameField().ToString().ToString()}, {"properties", {} } };

		if (c != NULL) {
			Log::GetLog()->info("iterating: {} ", o->NameField().ToString().ToString());
			auto p = c->PropertyLinkField();
			while (p != NULL) {
				auto v = getVariableData(o, c, p);
				
				if (v.contains("value") && (v["value"] != 0  && v["value"] != "")) {
					//Log::GetLog()->info("stuff: {} ", v.dump());
					json[o->NameField().ToString().ToString()]["properties"][p->NameField().ToString().ToString()] = v;
			
				}
				p = p->PropertyLinkNextField();
			}
		}
	}
	return json;
}

void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* This) {
	auto GameState = ArkApi::GetApiUtils().GetGameState();
	Log::GetLog()->info("version: {}.{}", GameState->ServerMajorVersionField(), GameState->ServerMinorVersionField());
	Log::GetLog()->info("searching");
	auto j = searchObjects();
	Log::GetLog()->info("search complete");
	std::filesystem::create_directory("changeanything");

	std::ofstream file("changeanything/classes_" + std::to_string(GameState->ServerMajorVersionField()) + "." + std::to_string(GameState->ServerMinorVersionField()) + ".json");
	file << std::setw(2) << j;
	file.flush();
	file.close();
	Log::GetLog()->info("saved");
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