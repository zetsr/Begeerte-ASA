#include "windows.h"
#include "external/CppSDK/SDK.hpp"
#include "internal/Hook/Hook.hpp"

void MainThread(LPVOID lpParam) {
    Hook::Init();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        if (HANDLE h = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, nullptr)) CloseHandle(h);
    }

	if (ul_reason_for_call == DLL_PROCESS_DETACH) {
		Hook::Uninit();
	}

    return TRUE;
}