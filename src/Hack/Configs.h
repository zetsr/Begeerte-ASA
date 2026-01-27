#pragma once

namespace g_Config {
    // 辅助函数：将 float[4] 转换为 ImU32
    inline ImU32 GetU32Color(float color[4]) {
        return ImGui::ColorConvertFloat4ToU32(*(ImVec4*)color);
    }

    // 生物列表
    inline char entitySearchBuf[256] = { 0 };
    inline bool bEnableFilter = false;

    // 自瞄
    inline bool bAimbotEnabled = false;
    inline float AimbotFOV = 180.0f;
    inline float AimbotSmooth = 5.0f;

    // 扳机
    inline bool bTriggerbotEnabled = false;
    inline float TriggerDelay = 0.0f;
    inline float TriggerRandomPercent = 0.0f;
    inline float TriggerHitChance = 100.0f;

    // 掉落物
    inline bool bDrawDroppedItems = true;
    inline float DroppedItemMaxDistance = 500.0f;

    // 宝箱
    inline bool bDrawSupplyDrops = true;
    inline float SupplyDropMaxDistance = 1500.0f;

    // 建筑
    inline bool bDrawStructures = true;
    inline float StructureMaxDistance = 5.0f;

    // 全局
    inline bool bDrawBox = true;
    inline float BoxColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    inline bool bDrawHealthBar = true;
    inline bool bDrawName = true;
    inline float NameColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    inline bool bDrawSpecies = false;
    inline bool bDrawGrowth = true;
    inline bool bDrawDistance = true;
    inline float DistanceColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // 队友
    inline bool bDrawBoxTeam = true;
    inline float BoxColorTeam[4] = { 0.0f, 0.5f, 1.0f, 1.0f };
    inline bool bDrawHealthBarTeam = true;
    inline bool bDrawNameTeam = true;
    inline float NameColorTeam[4] = { 0.0f, 0.5f, 1.0f, 1.0f };
    inline bool bDrawSpeciesTeam = false;
    inline bool bDrawGrowthTeam = true;
    inline bool bDrawDistanceTeam = true;
    inline float DistanceColorTeam[4] = { 0.0f, 0.5f, 1.0f, 1.0f };

    // OOF
    inline bool bEnableOOF = false;
    inline float OOFColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    inline float OOFRadius = 1.00f;
    inline float OOFSize = 12.0f;
    inline float OOFBreathSpeed = 0.1f;
    inline float OOFMinAlpha = 0.25f;
    inline float OOFMaxAlpha = 1.0f;
}