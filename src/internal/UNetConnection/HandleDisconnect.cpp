#pragma once
#include "../../external/CppSDK/SDK.hpp"
#include "../Lua/LuaManager.h"
#include "HandleDisconnect.h"

namespace g_UNetConnection {
	void HandleDisconnect(SDK::UNetConnection* UNetConnection) {
		// LuaManager::Get().Lua_OnDisconnect(UNetConnection ? UNetConnection->GetFirstIP() : "?", UNetConnection ? UNetConnection->GetPort() : 0);
	}
}