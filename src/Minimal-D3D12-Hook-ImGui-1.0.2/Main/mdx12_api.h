#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <vector>
#include <mutex>
#include <string>
#include <atomic>
#include <unordered_set>

#pragma warning(disable: 26451)
#pragma warning(disable: 26812)
#pragma warning(disable: 6387)
#pragma warning(push)
#pragma warning(disable: 26451)
#pragma warning(disable: 26812)
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_impl_win32.h"
#include "../ImGui/imgui_impl_dx12.h"
#include "../MinHook/include/MinHook.h"
#pragma warning(pop)

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main namespace for all globals
namespace g_MDX12 {
    // Hook function pointer types
    typedef HRESULT(STDMETHODCALLTYPE* PFN_Present)(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags);
    typedef void(STDMETHODCALLTYPE* PFN_ExecuteCommandLists)(ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists);
    typedef HRESULT(STDMETHODCALLTYPE* PFN_ResizeBuffers)(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
    typedef UINT(WINAPI* PFN_GetRawInputData)(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader);
    typedef UINT(WINAPI* PFN_GetRawInputBuffer)(PRAWINPUT pData, PUINT pcbSize, UINT cbSizeHeader);
    typedef BOOL(WINAPI* PFN_GetCursorPos)(LPPOINT lpPoint);
    typedef BOOL(WINAPI* PFN_SetCursorPos)(int X, int Y);
    typedef HCURSOR(WINAPI* PFN_SetCursor)(HCURSOR hCursor);
    typedef int(WINAPI* PFN_ShowCursor)(BOOL bShow);
    typedef BOOL(WINAPI* PFN_GetClipCursor)(LPRECT lpRect);
    typedef BOOL(WINAPI* PFN_ClipCursor)(const RECT* lpRect);
    typedef BOOL(WINAPI* PFN_GetMouseMovePointsEx)(UINT cbSize, LPMOUSEMOVEPOINT lppt, LPMOUSEMOVEPOINT lpptBuf, int nBufPoints, DWORD resolution);

    // Hook original function pointers namespace
    namespace g_HookFunctions {
        extern PFN_Present g_oPresent;
        extern PFN_ExecuteCommandLists g_oExecuteCommandLists;
        extern PFN_ResizeBuffers g_oResizeBuffers;
        extern PFN_GetRawInputData g_oGetRawInputData;
        extern PFN_GetRawInputBuffer g_oGetRawInputBuffer;
        extern PFN_GetCursorPos g_oGetCursorPos;
        extern PFN_SetCursorPos g_oSetCursorPos;
        extern PFN_SetCursor g_oSetCursor;
        extern PFN_ShowCursor g_oShowCursor;
        extern PFN_GetClipCursor g_oGetClipCursor;
        extern PFN_ClipCursor g_oClipCursor;
        extern PFN_GetMouseMovePointsEx g_oGetMouseMovePointsEx;
    }

    // Direct3D 12 resources namespace
    namespace g_D3D12Resources {
        extern ID3D12Device* g_pd3dDevice;
        extern ID3D12CommandQueue* g_pd3dCommandQueue;
        extern ID3D12DescriptorHeap* g_pd3dRtvDescHeap;
        extern ID3D12DescriptorHeap* g_pd3dSrvDescHeap;
        extern ID3D12GraphicsCommandList* g_pd3dCommandList;
        extern ID3D12Fence* g_fence;
        extern HANDLE g_fenceEvent;
        extern UINT64 g_fenceValue;
        extern UINT g_bufferCount;

        struct FrameContext {
            ID3D12CommandAllocator* CommandAllocator = nullptr;
            ID3D12Resource* Resource = nullptr;
            D3D12_CPU_DESCRIPTOR_HANDLE Descriptor{};
            UINT64 FenceValue = 0;
        };

        extern std::vector<FrameContext> g_FrameContexts;
    }

    // Initialization state namespace
    namespace g_InitState {
        extern bool g_Initialized;
        extern bool g_AfterFirstPresent;
        extern std::mutex g_InitMutex;
        extern UINT g_waitTimeoutMs;
    }

    // Process and window namespace
    namespace g_ProcessWindow {
        extern std::string g_processName;
        extern HWND g_mainWindow;
        extern RECT g_windowRect;
    }

    // Input state namespace
    namespace g_InputState {
        extern std::atomic<bool> g_blockMouseInput;
        extern std::atomic<bool> g_blockKeyboardInput;
    }

    // Menu state namespace
    namespace g_MenuState {
        extern bool g_isOpen;
        extern bool g_wasOpenLastFrame;
        extern POINT g_lastMousePos;
    }

    // Input hook functions
    namespace inputhook {
        void Init(HWND hWindow);
        void Remove(HWND hWindow);
        void UpdateInputBlockState();
        void ReinstallWindowHook();
    }

    // Raw input hook functions
    namespace rawinputhook {
        void Init();
        void Remove();
    }

    // Cursor hook functions
    namespace cursorhook {
        void Init();
        void Remove();
        void UpdateCursorState();
    }

    // Rendering and cleanup functions
    void InitProcessName();
    void CleanupRenderResources();
    void CleanupRenderResources_NoInput();
    void FinalCleanupAll();
    void SetupImGui(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags);

    // Main thread initialization
    DWORD WINAPI MainThread(LPVOID);

    // Callback function type for custom ImGui drawing
    typedef void(*SetupImGuiCallback)(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags);

    namespace g_Callbacks {
        extern SetupImGuiCallback g_setupImGuiCallback;
    }

    // Public API
    void Initialize();
    void SetSetupImGuiCallback(SetupImGuiCallback callback);
}

// Export for DLL
extern "C" __declspec(dllexport) void SetOverlayWaitTimeout(UINT ms);
extern "C" __declspec(dllexport) void SetSetupImGuiCallback(g_MDX12::SetupImGuiCallback callback);