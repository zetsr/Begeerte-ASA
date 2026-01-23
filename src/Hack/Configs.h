#pragma once

namespace g_Config {

    // 辅助函数：将 float[4] 转换为 ImU32
    inline ImU32 GetU32Color(float color[4]) {
        return ImGui::ColorConvertFloat4ToU32(*(ImVec4*)color);
    }
    inline char entitySearchBuf[256] = { 0 }; // 声明全局过滤字符数组
    inline bool bEnableFilter = false;        // 勾选框：是否只显示筛选后的内容

    inline bool bAimbotEnabled = false;
    inline float AimbotFOV = 180.0f;
    inline float AimbotSmooth = 5.0f;    // 1-100

    inline bool bTriggerbotEnabled = false;
    inline float TriggerDelay = 0.0f;   // 基础延迟 ms
    inline float TriggerRandomPercent = 0.0f; // 随机波动
    inline float TriggerHitChance = 100.0f;

    inline bool bDrawDroppedItems = true;          // 掉落物开关
    inline float DroppedItemMaxDistance = 500.0f;       // 掉落物最大显示距离

    inline bool bDrawSupplyDrops = true; // 空投 宝箱
    inline float SupplyDropMaxDistance = 1500.0f;

    // --- 敌人 ESP 设置 ---
    inline bool bDrawBox = true;
    inline float BoxColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    inline bool bDrawHealthBar = true;
    inline bool bDrawName = true;
    inline float NameColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    inline bool bDrawSpecies = false;
    inline bool bDrawGrowth = true;
    inline bool bDrawDistance = true;
    inline float DistanceColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // --- 队友 ESP 设置 ---
    inline bool bDrawBoxTeam = true;
    inline float BoxColorTeam[4] = { 0.0f, 0.5f, 1.0f, 1.0f };
    inline bool bDrawHealthBarTeam = true;
    inline bool bDrawNameTeam = true;
    inline float NameColorTeam[4] = { 0.0f, 0.5f, 1.0f, 1.0f };
    inline bool bDrawSpeciesTeam = false;
    inline bool bDrawGrowthTeam = true;
    inline bool bDrawDistanceTeam = true;
    inline float DistanceColorTeam[4] = { 0.0f, 0.5f, 1.0f, 1.0f };

    // --- OOF 设置 (共享) ---
    inline bool bEnableOOF = false;
    inline float OOFColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    inline float OOFRadius = 1.00f;
    inline float OOFSize = 12.0f;
    inline float OOFBreathSpeed = 0.1f;
    inline float OOFMinAlpha = 0.25f;
    inline float OOFMaxAlpha = 1.0f;
}