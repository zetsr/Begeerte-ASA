#include "../Minimal-D3D12-Hook-ImGui-1.0.2/Main/mdx12_api.h"
#include "../CppSDK/SDK.hpp"
#include "ESP.h"
#include "Configs.h"
#include "DrawESP.h"
#include "DrawImGui.h"

extern "C" {
#include "../Minimal-D3D12-Hook-ImGui-1.0.2/MinHook/src/buffer.c"
#include "../Minimal-D3D12-Hook-ImGui-1.0.2/MinHook/src/hook.c"
#include "../Minimal-D3D12-Hook-ImGui-1.0.2/MinHook/src/trampoline.c"
#include "../Minimal-D3D12-Hook-ImGui-1.0.2/MinHook/src/hde/hde64.c"
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_MDX12::Initialize();
        g_MDX12::SetSetupImGuiCallback(g_DrawImGui::MyImGuiDraw);
        break;

    case DLL_PROCESS_DETACH:
        g_MDX12::FinalCleanupAll();
        break;
    }
    return TRUE;
}