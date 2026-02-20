#include "../Minimal-D3D12-Hook-ImGui/Main/mdx12_api.h"
#include "SDK_Headers.hpp"
#include "ESP.h"
#include "Configs.h"
#include "DrawESP.h"
#include "DrawImGui.h"
#include "LuaManager.h"
#include "ConfigManager.h"

extern "C" {
#include "../Minimal-D3D12-Hook-ImGui/MinHook/src/buffer.c"
#include "../Minimal-D3D12-Hook-ImGui/MinHook/src/hook.c"
#include "../Minimal-D3D12-Hook-ImGui/MinHook/src/trampoline.c"
#include "../Minimal-D3D12-Hook-ImGui/MinHook/src/hde/hde64.c"
}

void init() {
    g_MDX12::Initialize();
    g_MDX12::SetSetupImGuiCallback(g_DrawImGui::MyImGuiDraw);

    ConfigManager::Get().Initialize("cfg");
    LuaManager::Get().Initialize("lua");
    LuaManager::Get().FetchWorkshopScripts();
}

void MainThread(LPVOID lpParam) {
    init();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        if (HANDLE h = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, nullptr)) CloseHandle(h);
        break;

    case DLL_PROCESS_DETACH:
        g_MDX12::FinalCleanupAll();
        break;
    }
    return TRUE;
}