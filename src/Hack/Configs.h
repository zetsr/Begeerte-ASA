#pragma once

namespace g_Config {
    // 生物列表
    inline char entitySearchBuf[256] = { 0 };
    inline bool bEnableFilter = false;

    // 建筑列表
    inline char structureSearchBuf[256] = "";
    inline bool bEnableStructureFilter = false;

    // 自瞄
    inline bool bAimbotEnabled = false;
    inline float AimbotFOV = 30.0f;
    inline float AimbotSmooth = 50.0f;

    // 扳机
    inline bool bTriggerbotEnabled = false;
    inline float TriggerDelay = 0.0f;
    inline float TriggerRandomPercent = 0.0f;
    inline float TriggerHitChance = 100.0f;

    // 掉落物
    inline bool bDrawDroppedItems = true;
    inline float DroppedItemMaxDistance = 500.0f;
    inline float DroppedItemNameColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    inline float DroppedItemDistanceColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    inline float DroppedItemMeatColor[4] = { 1.0f, 0.62f, 0.34f, 1.0f };
    inline float DroppedItemCryopodColor[4] = { 1.0f, 0.39f, 1.0f, 1.0f };
    inline float DroppedItemEggColor[4] = { 1.0f, 0.96f, 0.92f, 1.0f };
    inline float DroppedItemPiledColor[4] = { 0.19f, 1.0f, 0.19f, 1.0f };
    inline float DroppedItemWoodColor[4] = { 0.92f, 0.53f, 0.2f, 1.0f };
    inline float DroppedItemThatchColor[4] = { 0.94f, 0.94f, 0.71f, 1.0f };
    inline float DroppedItemHideColor[4] = { 1.0f, 0.82f, 0.67f, 1.0f };
    inline float DroppedItemPeltColor[4] = { 0.69f, 0.69f, 0.69f, 1.0f };
    inline float DroppedItemKeratinColor[4] = { 0.57f, 1.0f, 0.88f, 1.0f };
    inline float DroppedItemChitinColor[4] = { 0.57f, 1.0f, 0.88f, 1.0f };
    inline float DroppedItemCorruptedPolymerColor[4] = { 0.57f, 1.0f, 0.88f, 1.0f };
    inline float DroppedItemPolymer_OrganicColor[4] = { 0.57f, 1.0f, 0.88f, 1.0f };
    inline float DroppedItemPolymerColor[4] = { 0.57f, 1.0f, 0.88f, 1.0f };
    inline float DroppedItemMetalColor[4] = { 0.75f, 0.75f, 0.75f, 1.0f };
    inline float DroppedItemStoneColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
    inline float DroppedItemCrystalColor[4] = { 0.78f, 0.96f, 1.0f, 1.0f };
    inline float DroppedItemGemColor[4] = { 0.0f, 1.0f, 0.5f, 1.0f };
    inline float DroppedItemPearlColor[4] = { 0.29f, 0.0f, 0.51f, 1.0f };
    inline float DroppedItemSpoiledMeatColor[4] = { 0.39f, 0.69f, 0.39f, 1.0f };

    // 宝箱
    inline bool bDrawSupplyDrops = true;
    inline float SupplyDropMaxDistance = 10000.0f;

    // 建筑
    inline bool bDrawStructures = true;
    inline float StructureMaxDistance = 10000.0f;
    inline float StructureNameColor[4] = { 1.0f, 1.0f, 0.7f, 1.0f };
    inline float StructureOwnerColor[4] = { 0.4f, 1.0f, 1.0f, 1.0f };
    inline float StructureDistanceColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // 水源
    inline bool bDrawWater = true;
    inline float WaterMaxCount = 3;
    inline float WaterMaxDistance = 500.0f;
    inline float WaterNameColor[4] = { 0.0f, 0.75f, 1.0f, 1.0f };
    inline float WaterDistanceColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // 全局
    inline bool bDrawBox = true;
    inline float BoxColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    inline bool bDrawHealthBar = true;
    inline bool bDrawName = true;
    inline float NameColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    inline bool bDrawSpecies = false;
    inline bool bDrawGrowth = true;
    inline bool bDrawTorpor = true;
    inline float TorporColor[4] = { 0.95f, 0.5f, 0.95f, 1.0f };
    inline bool bDrawRagdoll = true;
    inline float RagdollColor[4] = { 1.0f, 0.0f, 0.25f, 0.75f };
    inline bool bDrawDistance = true;
    inline float DistanceColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    inline bool bDrawAimPoints = true;
    inline float AimPointsColor[4] = { 1.0f, 0.0f, 0.0f, 0.5f };
    inline bool bDrawAimSkeleton = true;
    inline float AimSkeletonColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // 队友
    inline bool bDrawBoxTeam = true;
    inline float BoxColorTeam[4] = { 0.0f, 0.5f, 1.0f, 1.0f };
    inline bool bDrawHealthBarTeam = true;
    inline bool bDrawNameTeam = true;
    inline float NameColorTeam[4] = { 0.0f, 0.5f, 1.0f, 1.0f };
    inline bool bDrawSpeciesTeam = false;
    inline bool bDrawGrowthTeam = true;
    inline bool bDrawTorporTeam = true;
    inline float TorporColorTeam[4] = { 0.0f, 0.9f, 1.0f, 1.0f };
    inline bool bDrawRagdollTeam = true;
    inline float RagdollColorTeam[4] = { 1.0f, 0.0f, 0.25f, 0.75f };
    inline bool bDrawDistanceTeam = true;
    inline float DistanceColorTeam[4] = { 0.0f, 0.5f, 1.0f, 1.0f };

    // OOF
    inline bool bEnableOOF = false;
    inline float OOFColor[4] = { 1.0f, 1.0f, 1.0f, 0.75f };
    inline float OOFRadius = 1.00f;
    inline float OOFSize = 12.0f;
    inline float OOFBreathSpeed = 0.1f;
    inline float OOFMinAlpha = 0.25f;
    inline float OOFMaxAlpha = 1.0f;
}