#pragma once

namespace g_Config {

    // 辅助函数：将 float[4] 转换为 ImU32
    inline ImU32 GetU32Color(float color[4]) {
        return ImGui::ColorConvertFloat4ToU32(*(ImVec4*)color);
    }

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