#include "mdx12_api.h"

#pragma warning(push)
#pragma warning(disable: 26451)
#pragma warning(disable: 26812)
#pragma warning(disable: 6387)
#pragma warning(pop)

namespace g_MDX12 {
    namespace cursorhook {
        static int g_cursorShowCount = 0;
        static HCURSOR g_lastCursor = nullptr;
        static bool g_initialized = false;
        static POINT g_lastReportedPos = { 0, 0 };

        BOOL WINAPI hkGetCursorPos(LPPOINT lpPoint) {
            if (!lpPoint) {
                if (g_HookFunctions::g_oGetCursorPos) return g_HookFunctions::g_oGetCursorPos(nullptr);
                return FALSE;
            }

            if (g_MenuState::g_isOpen && g_InputState::g_blockMouseInput) {
                RECT rect;
                GetWindowRect(g_ProcessWindow::g_mainWindow, &rect);
                lpPoint->x = rect.left + (rect.right - rect.left) / 2;
                lpPoint->y = rect.top + (rect.bottom - rect.top) / 2;
                g_lastReportedPos = *lpPoint;
                return TRUE;
            }

            BOOL result = g_HookFunctions::g_oGetCursorPos ? g_HookFunctions::g_oGetCursorPos(lpPoint) : FALSE;

            if (result) {
                g_lastReportedPos = *lpPoint;
            }

            return result;
        }

        BOOL WINAPI hkSetCursorPos(int X, int Y) {
            if (g_MenuState::g_isOpen && g_InputState::g_blockMouseInput) {
                return TRUE;
            }

            return g_HookFunctions::g_oSetCursorPos ? g_HookFunctions::g_oSetCursorPos(X, Y) : FALSE;
        }

        HCURSOR WINAPI hkSetCursor(HCURSOR hCursor) {
            if (g_MenuState::g_isOpen) {
                return g_HookFunctions::g_oSetCursor ? g_HookFunctions::g_oSetCursor(nullptr) : nullptr;
            }

            g_lastCursor = hCursor;
            return g_HookFunctions::g_oSetCursor ? g_HookFunctions::g_oSetCursor(hCursor) : nullptr;
        }

        int WINAPI hkShowCursor(BOOL bShow) {
            if (!g_MenuState::g_isOpen) {
                g_cursorShowCount += bShow ? 1 : -1;
            }

            if (g_MenuState::g_isOpen) {
                if (g_HookFunctions::g_oShowCursor) {
                    while (g_HookFunctions::g_oShowCursor(FALSE) >= 0);
                }
                return -100;
            }

            return g_HookFunctions::g_oShowCursor ? g_HookFunctions::g_oShowCursor(bShow) : 0;
        }

        BOOL WINAPI hkClipCursor(const RECT* lpRect) {
            if (g_MenuState::g_isOpen) {
                return g_HookFunctions::g_oClipCursor ? g_HookFunctions::g_oClipCursor(nullptr) : FALSE;
            }

            return g_HookFunctions::g_oClipCursor ? g_HookFunctions::g_oClipCursor(lpRect) : FALSE;
        }

        BOOL WINAPI hkGetMouseMovePointsEx(UINT cbSize, LPMOUSEMOVEPOINT lppt, LPMOUSEMOVEPOINT lpptBuf, int nBufPoints, DWORD resolution) {
            if (g_MenuState::g_isOpen && g_InputState::g_blockMouseInput) {
                if (lppt) {
                    memset(lppt, 0, cbSize);
                }
                return 0;
            }

            return g_HookFunctions::g_oGetMouseMovePointsEx ? g_HookFunctions::g_oGetMouseMovePointsEx(cbSize, lppt, lpptBuf, nBufPoints, resolution) : 0;
        }

        void UpdateCursorState() {
            if (g_MenuState::g_isOpen) {
                if (g_HookFunctions::g_oShowCursor) {
                    while (g_HookFunctions::g_oShowCursor(FALSE) >= 0);
                }

                if (g_HookFunctions::g_oClipCursor) {
                    g_HookFunctions::g_oClipCursor(nullptr);
                }
            }
            else {
                if (g_cursorShowCount >= 0) {
                    if (g_HookFunctions::g_oShowCursor) {
                        while (g_HookFunctions::g_oShowCursor(TRUE) < 0);
                    }
                }
                else {
                    if (g_HookFunctions::g_oShowCursor) {
                        while (g_HookFunctions::g_oShowCursor(FALSE) >= 0);
                    }
                }
            }
        }

        void Init() {
            HMODULE user32 = GetModuleHandleA("user32.dll");
            if (!user32) return;

            FARPROC getCursorPosAddr = GetProcAddress(user32, "GetCursorPos");
            FARPROC setCursorPosAddr = GetProcAddress(user32, "SetCursorPos");
            FARPROC setCursorAddr = GetProcAddress(user32, "SetCursor");
            FARPROC showCursorAddr = GetProcAddress(user32, "ShowCursor");
            FARPROC clipCursorAddr = GetProcAddress(user32, "ClipCursor");
            FARPROC getMouseMovePointsExAddr = GetProcAddress(user32, "GetMouseMovePointsEx");
            FARPROC getClipCursorAddr = GetProcAddress(user32, "GetClipCursor");

            if (getCursorPosAddr) {
                MH_CreateHook(getCursorPosAddr, reinterpret_cast<LPVOID>(hkGetCursorPos), reinterpret_cast<LPVOID*>(&g_HookFunctions::g_oGetCursorPos));
            }

            if (setCursorPosAddr) {
                MH_CreateHook(setCursorPosAddr, reinterpret_cast<LPVOID>(hkSetCursorPos), reinterpret_cast<LPVOID*>(&g_HookFunctions::g_oSetCursorPos));
            }

            if (setCursorAddr) {
                MH_CreateHook(setCursorAddr, reinterpret_cast<LPVOID>(hkSetCursor), reinterpret_cast<LPVOID*>(&g_HookFunctions::g_oSetCursor));
            }

            if (showCursorAddr) {
                MH_CreateHook(showCursorAddr, reinterpret_cast<LPVOID>(hkShowCursor), reinterpret_cast<LPVOID*>(&g_HookFunctions::g_oShowCursor));
            }

            if (clipCursorAddr) {
                MH_CreateHook(clipCursorAddr, reinterpret_cast<LPVOID>(hkClipCursor), reinterpret_cast<LPVOID*>(&g_HookFunctions::g_oClipCursor));
            }

            if (getMouseMovePointsExAddr) {
                MH_CreateHook(getMouseMovePointsExAddr, reinterpret_cast<LPVOID>(hkGetMouseMovePointsEx), reinterpret_cast<LPVOID*>(&g_HookFunctions::g_oGetMouseMovePointsEx));
            }

            if (getClipCursorAddr) {
                g_HookFunctions::g_oGetClipCursor = reinterpret_cast<PFN_GetClipCursor>(getClipCursorAddr);
            }

            if (g_HookFunctions::g_oGetCursorPos) {
                g_HookFunctions::g_oGetCursorPos(&g_lastReportedPos);
            }
            else {
                GetCursorPos(&g_lastReportedPos);
            }

            MH_EnableHook(MH_ALL_HOOKS);
            g_initialized = true;
        }

        void Remove() {
            if (!g_initialized) return;

            HMODULE user32 = GetModuleHandleA("user32.dll");
            if (!user32) return;

            FARPROC getCursorPosAddr = GetProcAddress(user32, "GetCursorPos");
            FARPROC setCursorPosAddr = GetProcAddress(user32, "SetCursorPos");
            FARPROC setCursorAddr = GetProcAddress(user32, "SetCursor");
            FARPROC showCursorAddr = GetProcAddress(user32, "ShowCursor");
            FARPROC clipCursorAddr = GetProcAddress(user32, "ClipCursor");
            FARPROC getMouseMovePointsExAddr = GetProcAddress(user32, "GetMouseMovePointsEx");

            if (getCursorPosAddr) MH_DisableHook(getCursorPosAddr);
            if (setCursorPosAddr) MH_DisableHook(setCursorPosAddr);
            if (setCursorAddr) MH_DisableHook(setCursorAddr);
            if (showCursorAddr) MH_DisableHook(showCursorAddr);
            if (clipCursorAddr) MH_DisableHook(clipCursorAddr);
            if (getMouseMovePointsExAddr) MH_DisableHook(getMouseMovePointsExAddr);

            g_HookFunctions::g_oGetCursorPos = nullptr;
            g_HookFunctions::g_oSetCursorPos = nullptr;
            g_HookFunctions::g_oSetCursor = nullptr;
            g_HookFunctions::g_oShowCursor = nullptr;
            g_HookFunctions::g_oClipCursor = nullptr;
            g_HookFunctions::g_oGetClipCursor = nullptr;
            g_HookFunctions::g_oGetMouseMovePointsEx = nullptr;
            g_initialized = false;
        }
    }

    namespace rawinputhook {
        UINT WINAPI hkGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader) {
            if (g_MenuState::g_isOpen && g_InputState::g_blockMouseInput) {
                if (pcbSize) *pcbSize = 0;
                if (pData && pcbSize && *pcbSize > 0) {
                    memset(pData, 0, *pcbSize);
                }
                return 0;
            }

            return g_HookFunctions::g_oGetRawInputData ? g_HookFunctions::g_oGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader) : 0;
        }

        UINT WINAPI hkGetRawInputBuffer(PRAWINPUT pData, PUINT pcbSize, UINT cbSizeHeader) {
            if (g_MenuState::g_isOpen && g_InputState::g_blockMouseInput) {
                if (pcbSize) {
                    *pcbSize = 0;
                }

                if (pData && pcbSize && *pcbSize > 0) {
                    memset(pData, 0, *pcbSize);
                }

                return 0;
            }

            return g_HookFunctions::g_oGetRawInputBuffer ? g_HookFunctions::g_oGetRawInputBuffer(pData, pcbSize, cbSizeHeader) : 0;
        }

        void Init() {
            HMODULE user32 = GetModuleHandleA("user32.dll");
            if (!user32) return;

            FARPROC getRawInputDataAddr = GetProcAddress(user32, "GetRawInputData");
            FARPROC getRawInputBufferAddr = GetProcAddress(user32, "GetRawInputBuffer");

            if (getRawInputDataAddr) {
                MH_CreateHook(getRawInputDataAddr, reinterpret_cast<LPVOID>(hkGetRawInputData), reinterpret_cast<LPVOID*>(&g_HookFunctions::g_oGetRawInputData));
            }

            if (getRawInputBufferAddr) {
                MH_CreateHook(getRawInputBufferAddr, reinterpret_cast<LPVOID>(hkGetRawInputBuffer), reinterpret_cast<LPVOID*>(&g_HookFunctions::g_oGetRawInputBuffer));
            }

            if (getRawInputDataAddr) MH_EnableHook(getRawInputDataAddr);
            if (getRawInputBufferAddr) MH_EnableHook(getRawInputBufferAddr);
        }

        void Remove() {
            HMODULE user32 = GetModuleHandleA("user32.dll");
            if (!user32) return;

            FARPROC getRawInputDataAddr = GetProcAddress(user32, "GetRawInputData");
            FARPROC getRawInputBufferAddr = GetProcAddress(user32, "GetRawInputBuffer");

            if (getRawInputDataAddr && g_HookFunctions::g_oGetRawInputData) {
                MH_DisableHook(getRawInputDataAddr);
                MH_RemoveHook(getRawInputDataAddr);
            }

            if (getRawInputBufferAddr && g_HookFunctions::g_oGetRawInputBuffer) {
                MH_DisableHook(getRawInputBufferAddr);
                MH_RemoveHook(getRawInputBufferAddr);
            }

            g_HookFunctions::g_oGetRawInputData = nullptr;
            g_HookFunctions::g_oGetRawInputBuffer = nullptr;
        }
    }

    namespace inputhook {
        static WNDPROC sOriginalWndProc = nullptr;
        static bool g_f1Down = false;

        void UpdateInputBlockState() {
            HWND foreground = GetForegroundWindow();
            bool isGameForeground = (foreground == g_ProcessWindow::g_mainWindow);
            char className[256] = { 0 };

            if (foreground && foreground != g_ProcessWindow::g_mainWindow) {
                GetClassNameA(foreground, className, sizeof(className));
            }

            std::unordered_set<std::string> menuClasses = {
                "#32770",
                "ConsoleWindowClass",
                "Edit",
                "ListBox",
            };

            bool gameHasMenuOpen = false;

            if (foreground && foreground != g_ProcessWindow::g_mainWindow) {
                DWORD pid;
                GetWindowThreadProcessId(foreground, &pid);
                DWORD gamePid = GetCurrentProcessId();

                if (pid == gamePid) {
                    gameHasMenuOpen = true;
                }
            }

            g_InputState::g_blockMouseInput = (g_MenuState::g_isOpen && !gameHasMenuOpen);
        }

        LRESULT APIENTRY WndProcHook(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
            if (uMsg == WM_KEYDOWN && wParam == VK_F1 && !g_f1Down) {
                g_f1Down = true;
                g_MenuState::g_isOpen = !g_MenuState::g_isOpen;

                if (g_MenuState::g_isOpen) {
                    GetCursorPos(&g_MenuState::g_lastMousePos);
                    RECT rect;
                    GetWindowRect(g_ProcessWindow::g_mainWindow, &rect);
                    int centerX = rect.left + (rect.right - rect.left) / 2;
                    int centerY = rect.top + (rect.bottom - rect.top) / 2;
                    SetCursorPos(centerX, centerY);
                }
                else {
                    SetCursorPos(g_MenuState::g_lastMousePos.x, g_MenuState::g_lastMousePos.y);
                }

                return 0;
            }
            else if (uMsg == WM_KEYUP && wParam == VK_F1) {
                g_f1Down = false;
                return 0;
            }

            if (uMsg == WM_INPUT) {
                if (g_MenuState::g_isOpen && g_InputState::g_blockMouseInput) {
                    return 0;
                }
            }

            if (uMsg == WM_INPUT_DEVICE_CHANGE) {
                if (g_MenuState::g_isOpen && g_InputState::g_blockMouseInput) {
                    return 0;
                }
            }

            if (g_MenuState::g_isOpen) {
                ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam);
                ImGuiIO& io = ImGui::GetIO();

                if (g_InputState::g_blockMouseInput) {
                    switch (uMsg) {
                    case WM_MOUSEMOVE:
                    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
                    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
                    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
                    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
                    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
                        return 0;
                    }
                }

                if (io.WantCaptureKeyboard) {
                    switch (uMsg) {
                    case WM_KEYDOWN: case WM_KEYUP:
                    case WM_SYSKEYDOWN: case WM_SYSKEYUP:
                    case WM_CHAR: case WM_SYSCHAR:
                    case WM_DEADCHAR: case WM_SYSDEADCHAR:
                    case WM_HOTKEY:
                        return 0;
                    case WM_IME_SETCONTEXT:
                    case WM_IME_NOTIFY:
                    case WM_IME_STARTCOMPOSITION:
                    case WM_IME_ENDCOMPOSITION:
                    case WM_IME_COMPOSITION:
                    case WM_IME_CHAR:
                        return 0;
                    }
                }
            }

            if (sOriginalWndProc) {
                return CallWindowProc(sOriginalWndProc, hwnd, uMsg, wParam, lParam);
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        void Init(HWND hWindow) {
            if (!hWindow) return;
            g_ProcessWindow::g_mainWindow = hWindow;

            if (sOriginalWndProc) {
                Remove(hWindow);
            }

            sOriginalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook)));
        }

        void Remove(HWND hWindow) {
            if (!hWindow || !sOriginalWndProc) return;

            SetWindowLongPtr(hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(sOriginalWndProc));
            sOriginalWndProc = nullptr;
        }

        void ReinstallWindowHook() {
            if (!g_ProcessWindow::g_mainWindow) return;

            if (!sOriginalWndProc) {
                Init(g_ProcessWindow::g_mainWindow);
            }
        }
    }
}