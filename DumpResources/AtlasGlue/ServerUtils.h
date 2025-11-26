#pragma once // This program is licensed under the GNU GPL v3.0, except that commercial use is prohibited.
#include <API/Atlas/Atlas.h>
#include <string>

// Server utility functions
unsigned int GetServerId() {
	return static_cast<UShooterGameInstance*>(ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())
		->GridInfoField()->GetCurrentServerId();
}

std::string GetServerGrid() {
	static const char* x[17] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q" };
	static const char* y[17] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17" };

	const auto serverInfo = static_cast<UShooterGameInstance*>(ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())
		->GridInfoField()->GetCurrentServerInfo();
	return std::string(x[serverInfo->gridXField()]) + y[serverInfo->gridYField()];
}

std::string GetChannelForServerId(unsigned int id) {
	return Config::GetConfig().channelServer + std::to_string(id);
}
