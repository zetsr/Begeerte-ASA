#pragma once
#include "../../external/CppSDK/SDK.hpp"
#include "../../external/Shadow-Gui/src/Shadow.h"
#include "../../external/Shadow-Gui/src/Backend_Dumper7.h"
#include "../CheatData/CheatCache.hpp"
#include "../ESP/DrawESP.h"
#include "../ESP/Indicators.h"
#include "../Lua/LuaManager.h"
#include "../Util/Util.h"
#include "PostRender.h"
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>

namespace g_UGameViewportClient {
    void LogMessageEng(const char* format, ...)
    {
        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        OutputDebugStringA(buffer);
    }

    void ApplyCrossVersionViewportScan(SDK::UGameViewportClient* rcx)
    {
        if (!rcx) return;

        uintptr_t class_base = (uintptr_t)rcx;

        // 1. 获取 FViewport 物理指针
        uintptr_t* pViewportPtr = (uintptr_t*)(class_base + 0x098);
        void* pRealViewport = pViewportPtr ? (void*)*pViewportPtr : nullptr;

        if (!pRealViewport)
        {
            return;
        }

        // 2. 自适应 100% 唯一定位最新版中的 pEngineShowFlags 起始地址（固定在 0x0B8） [1]
        static unsigned char* pEngineShowFlags = nullptr;
        static int32_t resolved_viewmode_offset = -1;

        if (!pEngineShowFlags)
        {
            // 2.1 定位 ViewModeIndex [1]
            for (int32_t offset = 0x28; offset <= 0x1A0; offset += 4)
            {
                int32_t* pVal = (int32_t*)(class_base + offset);
                if (*pVal == 3)
                {
                    int32_t next_val = *(int32_t*)(class_base + offset + 4);
                    int32_t prev_val = *(int32_t*)(class_base + offset - 4);
                    if (next_val == 0 || prev_val == 0 || next_val == -1)
                    {
                        resolved_viewmode_offset = offset;
                        break;
                    }
                }
            }

            // 2.2 使用精细化对齐掩码过滤出唯一的 ShowFlags 物理基准首部（0x0B8） [1]
            if (resolved_viewmode_offset != -1)
            {
                for (int32_t offset = resolved_viewmode_offset + 4; offset <= resolved_viewmode_offset + 32; offset += 4)
                {
                    unsigned char* pPossibleFlags = (unsigned char*)(class_base + offset);

                    bool bFirstByteFD = (pPossibleFlags[0] == 0xFD);
                    bool bNotZeroAlign = (pPossibleFlags[1] != 0x00 || pPossibleFlags[2] != 0x00);
                    bool bHighBitCheck = (pPossibleFlags[4] >= 0x7F && pPossibleFlags[5] >= 0x7F);

                    if (bFirstByteFD && bNotZeroAlign && bHighBitCheck)
                    {
                        pEngineShowFlags = pPossibleFlags;
                        LogMessageEng("[SUCCESS] Geographically resolved unique ShowFlags at Offset: 0x%03X (Address: 0x%p)\n", offset, pEngineShowFlags);
                        break;
                    }
                }
            }
        }

        if (!pEngineShowFlags)
        {
            return;
        }

        // =========================================================================
        // 3. 静态按键测试诊断状态机（扩展至 48 字节，即 384 个连续的画面控制 bits）
        //    由于后期、体积云等高级渲染标志在新版中被向后移动，扩展此范围是锁定它们的唯一途径！
        // =========================================================================
        static int g_TestBitIndex = 0;           // 当前测试的位索引 (0 ~ 383)
        static bool g_BitDisabled[384] = { false }; // 记录每个 bit 是否被强制关闭
        static unsigned char g_OriginalBytes[48] = { 0 }; // 完整备份 48 字节原始状态 [1]
        static bool bBackupDone = false;

        if (!bBackupDone)
        {
            // 备份这 48 字节的原始数据
            memcpy(g_OriginalBytes, pEngineShowFlags, 48);
            bBackupDone = true;
            LogMessageEng("[*] ShowFlags front 48 bytes backed up successfully.\n");
        }

        // -----------------------------------------------------------------
        // 键盘实时交互诊断：
        //    -> 按 [F5] 键：递增测试的 bit 索引 (0 -> 1 -> ... -> 383)
        //    -> 按 [F6] 键：递减测试的 bit 索引
        //    -> 按 [F7] 键：强行将该单独的 bit 改写为 0
        //    -> 按 [F8] 键：一键还原
        // -----------------------------------------------------------------
        bool bStateChanged = false;

        if (GetAsyncKeyState(VK_F5) & 0x0001)
        {
            g_TestBitIndex = (g_TestBitIndex + 1) % 384;
            int byte_pos = g_TestBitIndex / 8;
            int bit_pos = g_TestBitIndex % 8;
            LogMessageEng("[*] Bit Diagnostic: Active Index [%d] -> (ByteOffset: +0x%X, Bit: %d)\n",
                g_TestBitIndex, byte_pos, bit_pos);
            bStateChanged = true;
        }

        if (GetAsyncKeyState(VK_F6) & 0x0001)
        {
            g_TestBitIndex = (g_TestBitIndex - 1 + 384) % 384;
            int byte_pos = g_TestBitIndex / 8;
            int bit_pos = g_TestBitIndex % 8;
            LogMessageEng("[*] Bit Diagnostic: Active Index [%d] -> (ByteOffset: +0x%X, Bit: %d)\n",
                g_TestBitIndex, byte_pos, bit_pos);
            bStateChanged = true;
        }

        if (GetAsyncKeyState(VK_F7) & 0x0001)
        {
            g_BitDisabled[g_TestBitIndex] = !g_BitDisabled[g_TestBitIndex];
            LogMessageEng("[!] Bit Diagnostic: Toggled Bit [%d] state to: %s\n",
                g_TestBitIndex, g_BitDisabled[g_TestBitIndex] ? "DISABLED (0)" : "ENABLED (1)");
            bStateChanged = true;
        }

        if (GetAsyncKeyState(VK_F8) & 0x0001)
        {
            // 恢复全部 48 字节
            memcpy(pEngineShowFlags, g_OriginalBytes, 48);
            memset(g_BitDisabled, 0, sizeof(g_BitDisabled));
            LogMessageEng("[!] Bit Diagnostic: One-click restore executed. All bits reverted to original values.\n");
            bStateChanged = true;
        }

        // 4. 实时计算并应用当前的 384 位的按位测试覆盖
        if (bStateChanged)
        {
            // 先复原到最原始的数据状态
            memcpy(pEngineShowFlags, g_OriginalBytes, 48);

            // 针对被我们标记为 DISABLED 的 bit，执行精确的按位清除 [1]
            for (int i = 0; i < 384; i++)
            {
                if (g_BitDisabled[i])
                {
                    int byte_offset = i / 8;
                    int bit_position = i % 8;
                    pEngineShowFlags[byte_offset] &= ~(1 << bit_position);
                }
            }
        }
    }

    void PostRender(SDK::UGameViewportClient* rcx, SDK::UCanvas* canvas) {
        //  2 禁用后处理
        //  7 禁用地面光照
        //  61 地面材质降低
        //  88 角色模型
        //  106 彻底禁用光照
        //  126 删掉水源、远景
        //  127 删掉地面
        //  129 删掉远景滤镜
        //  139 让水的颜色变浅一点
        //  220 解决水面闪烁

        // ApplyCrossVersionViewportScan(rcx);

        CheatCache::PostRender::Update();

        if (g_Config::bPotatoGraphics) {
            g_Util::PotatoGraphics(CheatCache::PostRender::World, rcx);
            g_Config::bPotatoGraphics = false;
        }

		Shadow::Dumper7_NewFrame(canvas);
        Shadow::NewFrame();
        g_Util::Welcome(CheatCache::PostRender::World, canvas);
        g_DrawESP::DrawESP(canvas);
        g_Indicators::OnRender();
        LuaManager::Get().Lua_OnPostRender();
        Shadow::Render();
		Shadow::Dumper7_RenderDrawData(Shadow::GetDrawData());
    }
}