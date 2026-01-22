#include "mdx12_api.h"

#pragma warning(push)
#pragma warning(disable: 26451)
#pragma warning(disable: 26812)
#pragma warning(disable: 6387)
#pragma warning(pop)

namespace g_MDX12 {
    void CleanupRenderResources() {
        if (g_D3D12Resources::g_pd3dCommandQueue && g_D3D12Resources::g_fence && g_D3D12Resources::g_fenceEvent) {
            UINT64 localFence = ++g_D3D12Resources::g_fenceValue;
            HRESULT hr = g_D3D12Resources::g_pd3dCommandQueue->Signal(g_D3D12Resources::g_fence, localFence);

            if (SUCCEEDED(hr)) {
                if (g_D3D12Resources::g_fence->GetCompletedValue() < localFence) {
                    g_D3D12Resources::g_fence->SetEventOnCompletion(localFence, g_D3D12Resources::g_fenceEvent);
                    WaitForSingleObject(g_D3D12Resources::g_fenceEvent, g_InitState::g_waitTimeoutMs);
                }
            }
        }

        g_InitState::g_Initialized = false;

        if (ImGui::GetCurrentContext()) {
            ImGui_ImplDX12_InvalidateDeviceObjects();
            ImGui_ImplDX12_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }

        if (g_D3D12Resources::g_pd3dCommandList) { g_D3D12Resources::g_pd3dCommandList->Release(); g_D3D12Resources::g_pd3dCommandList = nullptr; }
        if (g_D3D12Resources::g_pd3dRtvDescHeap) { g_D3D12Resources::g_pd3dRtvDescHeap->Release(); g_D3D12Resources::g_pd3dRtvDescHeap = nullptr; }
        if (g_D3D12Resources::g_pd3dSrvDescHeap) { g_D3D12Resources::g_pd3dSrvDescHeap->Release(); g_D3D12Resources::g_pd3dSrvDescHeap = nullptr; }
        if (g_D3D12Resources::g_fence) { g_D3D12Resources::g_fence->Release(); g_D3D12Resources::g_fence = nullptr; }
        if (g_D3D12Resources::g_fenceEvent) { CloseHandle(g_D3D12Resources::g_fenceEvent); g_D3D12Resources::g_fenceEvent = nullptr; }

        for (auto& frame : g_D3D12Resources::g_FrameContexts) {
            if (frame.Resource) { frame.Resource->Release(); frame.Resource = nullptr; }
            if (frame.CommandAllocator) { frame.CommandAllocator->Release(); frame.CommandAllocator = nullptr; }

            frame.FenceValue = 0;
        }

        g_D3D12Resources::g_FrameContexts.clear();
        g_D3D12Resources::g_bufferCount = 0;
        rawinputhook::Remove();
        cursorhook::Remove();
    }

    void CleanupRenderResources_NoInput() {
        if (g_D3D12Resources::g_pd3dCommandQueue && g_D3D12Resources::g_fence && g_D3D12Resources::g_fenceEvent) {
            UINT64 localFence = ++g_D3D12Resources::g_fenceValue;

            if (SUCCEEDED(g_D3D12Resources::g_pd3dCommandQueue->Signal(g_D3D12Resources::g_fence, localFence))) {
                if (g_D3D12Resources::g_fence->GetCompletedValue() < localFence) {

                    g_D3D12Resources::g_fence->SetEventOnCompletion(localFence, g_D3D12Resources::g_fenceEvent);
                    WaitForSingleObject(g_D3D12Resources::g_fenceEvent, g_InitState::g_waitTimeoutMs);
                }
            }
        }

        if (ImGui::GetCurrentContext()) {
            ImGui_ImplDX12_InvalidateDeviceObjects();
        }

        if (g_D3D12Resources::g_pd3dCommandList) { g_D3D12Resources::g_pd3dCommandList->Release(); g_D3D12Resources::g_pd3dCommandList = nullptr; }
        if (g_D3D12Resources::g_pd3dRtvDescHeap) { g_D3D12Resources::g_pd3dRtvDescHeap->Release(); g_D3D12Resources::g_pd3dRtvDescHeap = nullptr; }
        if (g_D3D12Resources::g_pd3dSrvDescHeap) { g_D3D12Resources::g_pd3dSrvDescHeap->Release(); g_D3D12Resources::g_pd3dSrvDescHeap = nullptr; }

        for (auto& frame : g_D3D12Resources::g_FrameContexts) {
            if (frame.Resource) { frame.Resource->Release(); frame.Resource = nullptr; }
            if (frame.CommandAllocator) { frame.CommandAllocator->Release(); frame.CommandAllocator = nullptr; }

            frame.FenceValue = 0;
        }

        g_D3D12Resources::g_FrameContexts.clear();
        g_D3D12Resources::g_bufferCount = 0;
        g_InitState::g_Initialized = false;
    }

    void FinalCleanupAll() {
        std::lock_guard<std::mutex> lock(g_InitState::g_InitMutex);

        if (g_InitState::g_Initialized) CleanupRenderResources();
        if (g_D3D12Resources::g_pd3dCommandQueue && g_D3D12Resources::g_fence && g_D3D12Resources::g_fenceEvent) {
            UINT64 localFence = ++g_D3D12Resources::g_fenceValue;
            g_D3D12Resources::g_pd3dCommandQueue->Signal(g_D3D12Resources::g_fence, localFence);

            if (g_D3D12Resources::g_fence->GetCompletedValue() < localFence) {

                g_D3D12Resources::g_fence->SetEventOnCompletion(localFence, g_D3D12Resources::g_fenceEvent);
                WaitForSingleObject(g_D3D12Resources::g_fenceEvent, INFINITE);
            }
        }

        if (g_ProcessWindow::g_mainWindow) inputhook::Remove(g_ProcessWindow::g_mainWindow);
        if (g_D3D12Resources::g_pd3dCommandQueue) { g_D3D12Resources::g_pd3dCommandQueue->Release(); g_D3D12Resources::g_pd3dCommandQueue = nullptr; }
        if (g_D3D12Resources::g_pd3dDevice) { g_D3D12Resources::g_pd3dDevice->Release(); g_D3D12Resources::g_pd3dDevice = nullptr; }
    }

    void InitProcessName() {
        // Implementation here
    }

    void Initialize() {
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
}

// Export C functions for external usage
extern "C" {
    __declspec(dllexport) void SetOverlayWaitTimeout(UINT ms) {
        g_MDX12::g_InitState::g_waitTimeoutMs = ms;
    }

    __declspec(dllexport) void SetSetupImGuiCallback(g_MDX12::SetupImGuiCallback callback) {
        g_MDX12::SetSetupImGuiCallback(callback);
    }
}