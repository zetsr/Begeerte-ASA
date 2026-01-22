#pragma once
#include "../Minimal-D3D12-Hook-ImGui-1.0.2/Main/mdx12_api.h"

namespace g_DrawImGui {
	void MyImGuiDraw(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags);
}