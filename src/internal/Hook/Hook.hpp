#pragma once

#include "../../external/CppSDK/SDK.hpp"
#include "../../external/ezVMT/include/ezVMT.hpp"
#include "../CheatData/CheatData.hpp"

#include "../../external/Shadow-Gui/src/Shadow.h"
#include "../MEM/MEM.hpp"
#include "../../external/Font/仓耳舒圆体W03.h"
#include "../Menu/Menu.hpp"
#include "../ESP/ESP.hpp"
#include "../Config/Config.hpp"

namespace Hook {
    inline HWND g_hwnd = NULL;
    inline WNDPROC oWndProc = NULL;

    inline LRESULT APIENTRY hkWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        // 始终处理全局热键（菜单打开或关闭都需要）
        Shadow::ProcessGlobalHotkeys(hwnd, uMsg, wParam, lParam);

        // 菜单切换键处理（如果正在分配热键，则不切换菜单）
        if (uMsg == WM_KEYDOWN && wParam == Config::kShowMenu && !Shadow::g_Ctx.AssigningHotkey) {
            bool isFirstPress = ((lParam & (1 << 30)) == 0);
            if (isFirstPress) {
                Config::bShowMenu = !Config::bShowMenu;
            }
        }

        if (Config::bShowMenu) {
            // 键盘消息处理
            if (uMsg == WM_KEYDOWN || uMsg == WM_KEYUP || uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP || uMsg == WM_CHAR) {
                // 如果是切换键本身，放行给游戏（防止菜单无法关闭）
                if (wParam == Config::kShowMenu) {
                    return CallWindowProc(oWndProc, hwnd, uMsg, wParam, lParam);
                }

                // 如果是已注册的全局热键，放行给游戏（热键状态已在 ProcessGlobalHotkeys 中更新）
                if (Shadow::IsHotkeyRegistered(static_cast<int>(wParam))) {
                    return CallWindowProc(oWndProc, hwnd, uMsg, wParam, lParam);
                }

                // 先让框架处理按键（更新 KeyPressed, KeyStates 等）
                Shadow::Input(hwnd, uMsg, wParam, lParam);

                // 如果是允许放行的按键，再转发给游戏
                if (Shadow::IsKeyAllowed(static_cast<int>(wParam))) {
                    // 注意：这里不能再次调用 Shadow::Input，因为上面已经调用过了
                    return CallWindowProc(oWndProc, hwnd, uMsg, wParam, lParam);
                }

                // 非允许按键，被菜单吃掉
                return 1;
            }

            // 如果是鼠标消息，且不在白名单里，直接阻塞
            if (!Shadow::IsMouseMsgAllowed(uMsg)) {
                Shadow::Input(hwnd, uMsg, wParam, lParam);
                return 1;
            }

            // 鼠标消息在白名单里，也需要先处理，再放行
            Shadow::Input(hwnd, uMsg, wParam, lParam);
            return CallWindowProc(oWndProc, hwnd, uMsg, wParam, lParam);
        }

        // 菜单关闭时，也要处理输入以更新全局鼠标状态
        Shadow::Input(hwnd, uMsg, wParam, lParam);
        return CallWindowProc(oWndProc, hwnd, uMsg, wParam, lParam);
    }

    inline BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == GetCurrentProcessId()) {
            if (IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == NULL) {
                LONG style = GetWindowLongPtr(hwnd, GWL_STYLE);
                if ((style & WS_CAPTION) || (style & WS_POPUP)) {
                    g_hwnd = hwnd;
                    return FALSE;
                }
            }
        }
        return TRUE;
    }

    inline void InitWndProc() {
        EnumWindows(EnumWindowsProc, 0);
        if (g_hwnd) {
            oWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
            Shadow::SetAllowedKeys({ 'W', 'A', 'S', 'D', VK_SPACE, VK_SHIFT, VK_LMENU });
        }
    }

    typedef void(__fastcall* tPostRender)(SDK::UGameViewportClient* _this, SDK::UCanvas* Canvas);
	inline tPostRender oPostRender = nullptr;

	inline void __fastcall hkPostRender(SDK::UGameViewportClient* _this, SDK::UCanvas* Canvas) {
        oPostRender(_this, Canvas);

		Shadow::DefaultFont = Shadow::LoadFontFromBuffer(Font::仓耳舒圆体W03, sizeof(Font::仓耳舒圆体W03));
		Shadow::NewFrame(Canvas);

		ESP::Run(Canvas);
		Menu::Run();

		Shadow::Render();
    }

	typedef void(__fastcall* tActorTick)(SDK::AActor* _this, float DeltaTime);
	inline tActorTick oActorTick = nullptr;

	inline void __fastcall hkActorTick(SDK::AActor* _this, float DeltaTime) {
		oActorTick(_this, DeltaTime);
	}

	inline void Init() {
		while (true) {
			SDK::UEngine* Engine = SDK::UEngine::GetEngine();
			SDK::UWorld* World = SDK::UWorld::GetWorld();

			if (Engine && Engine->GameViewport && World && World->PersistentLevel && World->PersistentLevel->Actors[0]) {
                Hook::InitWndProc();

                ezVMT::CreateHook((void*)&hkPostRender, Engine->GameViewport, CheatData::VTableIndex::PostRender, (void**)&oPostRender);
				ezVMT::CreateHook((void*)&hkActorTick, World->PersistentLevel->Actors[0], CheatData::VTableIndex::Tick, (void**)&oActorTick);
				ezVMT::EnableHook(ezVMT::All_Hooks);
				break;
			}

			Sleep(100);
		}
	}

	inline void Uninit() {
		ezVMT::RemoveHook(ezVMT::All_Hooks);
	}
}