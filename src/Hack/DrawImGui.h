#pragma once
#include "../Minimal-D3D12-Hook-ImGui/Main/mdx12_api.h"

namespace g_DrawImGui {
	void MyImGuiDraw(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags);
}