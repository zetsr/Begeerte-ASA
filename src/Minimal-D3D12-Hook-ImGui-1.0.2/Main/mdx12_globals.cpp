#include "mdx12_api.h"

namespace g_MDX12 {
    // Hook original function pointers
    namespace g_HookFunctions {
        PFN_Present g_oPresent = nullptr;
        PFN_ExecuteCommandLists g_oExecuteCommandLists = nullptr;
        PFN_ResizeBuffers g_oResizeBuffers = nullptr;
        PFN_GetRawInputData g_oGetRawInputData = nullptr;
        PFN_GetRawInputBuffer g_oGetRawInputBuffer = nullptr;
        PFN_GetCursorPos g_oGetCursorPos = nullptr;
        PFN_SetCursorPos g_oSetCursorPos = nullptr;
        PFN_SetCursor g_oSetCursor = nullptr;
        PFN_ShowCursor g_oShowCursor = nullptr;
        PFN_GetClipCursor g_oGetClipCursor = nullptr;
        PFN_ClipCursor g_oClipCursor = nullptr;
        PFN_GetMouseMovePointsEx g_oGetMouseMovePointsEx = nullptr;
    }

    // Direct3D 12 resources
    namespace g_D3D12Resources {
        ID3D12Device* g_pd3dDevice = nullptr;
        ID3D12CommandQueue* g_pd3dCommandQueue = nullptr;
        ID3D12DescriptorHeap* g_pd3dRtvDescHeap = nullptr;
        ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;
        ID3D12GraphicsCommandList* g_pd3dCommandList = nullptr;
        ID3D12Fence* g_fence = nullptr;
        HANDLE g_fenceEvent = nullptr;
        UINT64 g_fenceValue = 0;
        UINT g_bufferCount = 0;
        std::vector<FrameContext> g_FrameContexts;
    }

    // Initialization state
    namespace g_InitState {
        bool g_Initialized = false;
        bool g_AfterFirstPresent = false;
        std::mutex g_InitMutex;
        UINT g_waitTimeoutMs = 2000;
    }

    // Process and window
    namespace g_ProcessWindow {
        std::string g_processName;
        HWND g_mainWindow = nullptr;
        RECT g_windowRect = { 0 };
    }

    // Input state
    namespace g_InputState {
        std::atomic<bool> g_blockMouseInput{ false };
        std::atomic<bool> g_blockKeyboardInput{ false };
    }

    // Menu state
    namespace g_MenuState {
        bool g_isOpen = true;
        bool g_wasOpenLastFrame = true;
        POINT g_lastMousePos = { 0, 0 };
    }

    // Callback functions
    namespace g_Callbacks {
        SetupImGuiCallback g_setupImGuiCallback = nullptr;
    }
}