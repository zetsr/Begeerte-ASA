// DrawESP.cpp
#include "../Minimal-D3D12-Hook-ImGui/Main/mdx12_api.h"
#include "SDK_Headers.hpp"
#include "ESP.h"
#include "Configs.h"
#include "DrawESP.h"
#include "Util.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>
#include <memory>

namespace g_DrawESP {
    static constexpr float FADE_IN_TIME = 0.10f;
    static constexpr float FADE_OUT_TIME = 0.20f;

    struct CachedFlag {
        std::string text;
        ImU32 color;
        g_ESP::FlagPos pos;
    };

    struct CachedBar {
        float currentValue;
        float maxValue;
        ImU32 color;
        g_ESP::BarPos pos;
        g_ESP::BarOrientation orientation;
    };

    struct ESPEntry {
        uintptr_t actorKey = 0;
        SDK::FVector lastWorldLoc{};
        g_ESP::BoxRect cachedRect;
        std::string name;
        std::vector<CachedFlag> flags;
        std::vector<CachedBar> bars;
        ImU32 boxColor = 0;
        ImU32 nameColor = 0;
        float configBoxAlpha = 1.0f;
        float targetAlpha = 0.0f;
        float alpha = 0.0f;
        float lastSeenTime = 0.0f;
        bool aliveThisFrame = false;
        bool isOOF = false;
        bool isItem = false;
        float cachedHP = 0.0f;
        float cachedMaxHP = 0.0f;
        float cachedTorpor = 0.0f;
        float cachedMaxTorpor = 0.0f;

        bool shouldDrawBox = false;
        bool shouldDrawHealthBar = false;
        bool shouldDrawName = false;
        bool shouldDrawDistance = false;
        bool shouldDrawTorpor = false;
    };

    static std::unordered_map<uintptr_t, ESPEntry> s_entries;

    static float ApproachAlpha(float cur, float target, float deltaSeconds, float fadeTime) {
        if (fadeTime <= 0.0f) return target;
        float diff = target - cur;
        float maxStep = deltaSeconds / fadeTime;
        if (fabs(diff) <= maxStep) return target;
        return cur + (diff > 0 ? maxStep : -maxStep);
    }

    void DrawESP() {
        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (!World || !World->GameState || !World->PersistentLevel) return;

        SDK::APlayerController* LocalPC = g_Util::GetLocalPC();
        if (!LocalPC || !LocalPC->Pawn) {
            for (auto& kv : s_entries) {
                kv.second.targetAlpha = 0.0f;
                kv.second.aliveThisFrame = false;
            }
            return;
        }

        SDK::APlayerState* LocalPS = LocalPC->PlayerState;
        if (!LocalPS) {
            for (auto& kv : s_entries) {
                kv.second.targetAlpha = 0.0f;
                kv.second.aliveThisFrame = false;
            }
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        float screenW = io.DisplaySize.x;
        float screenH = io.DisplaySize.y;
        float deltaTime = ImGui::GetIO().DeltaTime;

        SDK::TArray<SDK::AActor*>& Actors = World->PersistentLevel->Actors;

        for (auto& kv : s_entries) {
            kv.second.aliveThisFrame = false;
        }

        std::string searchFilter = g_Config::entitySearchBuf;

        for (int i = 0; i < Actors.Num(); i++) {
            SDK::AActor* TargetActor = Actors[i];

            if (!TargetActor || TargetActor == LocalPC->Pawn) continue;

            uintptr_t key = reinterpret_cast<uintptr_t>(TargetActor);
            ESPEntry& entry = s_entries[key];

            entry.actorKey = key;
            entry.lastSeenTime = 0.0f;

            if (TargetActor->bHidden) {
                entry.targetAlpha = 0.0f;
                entry.lastWorldLoc = TargetActor->K2_GetActorLocation();
                entry.aliveThisFrame = false;
                continue;
            }

            entry.aliveThisFrame = true;
            entry.lastWorldLoc = TargetActor->K2_GetActorLocation();

            if (TargetActor && TargetActor->IsA(SDK::APrimalCharacter::StaticClass())) {
                SDK::APrimalCharacter* TargetChar = (SDK::APrimalCharacter*)TargetActor;
                SDK::APrimalCharacter* LocalChar = (SDK::APrimalCharacter*)LocalPC->Pawn;

                if (!TargetChar || !LocalChar) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
                }

                SDK::APlayerState* TargetPS = TargetChar->PlayerState;

                bool isDead = TargetChar->IsDead();
                if (isDead) {
                    g_ESP::RelationType relation = g_ESP::GetRelation(TargetChar, LocalChar);

                    bool bShowRagdoll = false;
                    float* RagdollCol = nullptr;

                    if (relation == g_ESP::RelationType::Team) {
                        bShowRagdoll = g_Config::bDrawRagdollTeam;
                        RagdollCol = g_Config::RagdollColorTeam;
                    }
                    else {
                        bShowRagdoll = g_Config::bDrawRagdoll;
                        RagdollCol = g_Config::RagdollColor;
                    }

                    if (!bShowRagdoll) {
                        entry.targetAlpha = 0.0f;
                        entry.aliveThisFrame = false;
                        continue;
                    }

                    g_ESP::BoxRect rect = g_ESP::DrawBox(TargetActor,
                        RagdollCol[0] * 255.0f, RagdollCol[1] * 255.0f,
                        RagdollCol[2] * 255.0f, RagdollCol[3] * 255.0f, 0.5f, true);

                    if (!rect.valid) {
                        entry.targetAlpha = 0.0f;
                        continue;
                    }

                    entry.cachedRect = rect;
                    entry.boxColor = g_Util::GetU32Color(RagdollCol);
                    entry.configBoxAlpha = RagdollCol[3];
                    entry.targetAlpha = 1.0f;
                    entry.aliveThisFrame = true;

                    entry.shouldDrawBox = true;
                    entry.shouldDrawHealthBar = false;
                    entry.shouldDrawTorpor = false;
                    entry.shouldDrawName = true;
                    entry.shouldDrawDistance = true;

                    entry.flags.clear();
                    entry.bars.clear();

                    std::string deadName = (TargetPS ? TargetPS->GetPlayerName().ToString() : TargetChar->GetDescriptiveName().ToString());
                    entry.flags.push_back({ deadName, entry.boxColor, g_ESP::FlagPos::Top });

                    float dist = 0.0f;
                    if (LocalPC->Pawn && TargetActor) {
                        dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
                    }
                    entry.flags.push_back({ std::to_string((int)dist) + "m", g_Util::ToImColor(200, 200, 200, 255), g_ESP::FlagPos::Right });

                    continue;
                }

                if (g_Config::bEnableFilter && !searchFilter.empty()) {
                    std::string nameForESP = TargetChar->PlayerState ?
                        TargetChar->PlayerState->GetPlayerName().ToString() :
                        TargetChar->GetDescriptiveName().ToString();

                    if (!g_Util::IsEntityMatch(nameForESP, searchFilter)) {
                        entry.targetAlpha = 0.0f;
                        entry.aliveThisFrame = false;
                        continue;
                    }
                }

                g_ESP::RelationType relation = g_ESP::GetRelation(TargetChar, LocalChar);

                bool bDrawBox = false, bDrawHealthBar = false, bDrawName = false;
                bool bDrawGrowth = false, bDrawDistance = false, bDrawTorpor = false;
                float* BoxColor = nullptr;
                float* NameColor = nullptr;
                float* DistanceColor = nullptr;
                float* TorporColor = nullptr;

                if (relation == g_ESP::RelationType::Team) {
                    bDrawBox = g_Config::bDrawBoxTeam;
                    BoxColor = g_Config::BoxColorTeam;
                    bDrawHealthBar = g_Config::bDrawHealthBarTeam;
                    bDrawName = g_Config::bDrawNameTeam;
                    NameColor = g_Config::NameColorTeam;
                    bDrawGrowth = g_Config::bDrawGrowthTeam;
                    bDrawDistance = g_Config::bDrawDistanceTeam;
                    DistanceColor = g_Config::DistanceColorTeam;
                    bDrawTorpor = g_Config::bDrawTorporTeam;
                    TorporColor = g_Config::TorporColorTeam;
                }
                else {
                    bDrawBox = g_Config::bDrawBox;
                    BoxColor = g_Config::BoxColor;
                    bDrawHealthBar = g_Config::bDrawHealthBar;
                    bDrawName = g_Config::bDrawName;
                    NameColor = g_Config::NameColor;
                    bDrawGrowth = g_Config::bDrawGrowth;
                    bDrawDistance = g_Config::bDrawDistance;
                    DistanceColor = g_Config::DistanceColor;
                    bDrawTorpor = g_Config::bDrawTorpor;
                    TorporColor = g_Config::TorporColor;
                }

                SDK::FVector actorLoc = TargetActor->K2_GetActorLocation();

                g_ESP::BoxRect rect = g_ESP::DrawBox(TargetActor,
                    BoxColor[0] * 255.0f, BoxColor[1] * 255.0f,
                    BoxColor[2] * 255.0f, BoxColor[3] * 255.0f, 0.5f, true);

                entry.cachedRect = rect;
                entry.boxColor = g_Util::GetU32Color(BoxColor);
                entry.nameColor = g_Util::GetU32Color(NameColor);
                entry.configBoxAlpha = BoxColor[3];
                entry.isItem = false;
                entry.isOOF = false;
                entry.cachedHP = TargetChar->GetHealth();
                entry.cachedMaxHP = TargetChar->GetMaxHealth();

                SDK::UPrimalCharacterStatusComponent* StatusComp = TargetChar->GetCharacterStatusComponent();
                if (StatusComp) {
                    entry.cachedTorpor = StatusComp->CurrentStatusValues[(int)SDK::EPrimalCharacterStatusValue::Torpidity];
                    entry.cachedMaxTorpor = StatusComp->MaxStatusValues[(int)SDK::EPrimalCharacterStatusValue::Torpidity];
                }
                else {
                    entry.cachedTorpor = 0.0f;
                    entry.cachedMaxTorpor = 0.0f;
                }

                entry.shouldDrawBox = bDrawBox;
                entry.shouldDrawHealthBar = bDrawHealthBar;
                entry.shouldDrawName = bDrawName;
                entry.shouldDrawDistance = bDrawDistance;
                entry.shouldDrawTorpor = bDrawTorpor;

                if (bDrawName) {
                    std::string gender = "?";
                    if (LocalPC->Pawn && TargetActor) {
                        gender = TargetActor->IsFemale() ? "F" : "M";
                    }

                    if (gender != "?") {
                        entry.name = TargetPS ? TargetPS->GetPlayerName().ToString() + "-" + gender : TargetChar->GetDescriptiveName().ToString() + "-" + gender;
                    }
                    else {
                        entry.name = TargetPS ? TargetPS->GetPlayerName().ToString() : TargetChar->GetDescriptiveName().ToString();
                    }
                }
                else {
                    entry.name.clear();
                }

                entry.flags.clear();
                entry.bars.clear();

                if (bDrawName && !entry.name.empty()) {
                    entry.flags.push_back({ entry.name, entry.nameColor, g_ESP::FlagPos::Top });
                }

                if (bDrawHealthBar) {
                    std::string hpStr = std::to_string((int)entry.cachedHP);
                    float healthPercent = (entry.cachedMaxHP > 0) ? (entry.cachedHP / entry.cachedMaxHP) : 0.0f;
                    ImU32 hpCol = g_Util::GetHealthColor(healthPercent);
                    entry.flags.push_back({ hpStr, hpCol, g_ESP::FlagPos::Left });
                    entry.bars.push_back({ entry.cachedHP, entry.cachedMaxHP, hpCol, g_ESP::BarPos::Left, g_ESP::BarOrientation::Vertical });
                }

                if (bDrawTorpor && entry.cachedMaxTorpor > 0) {
                    std::string torporStr = std::to_string((int)entry.cachedTorpor) + "/" + std::to_string((int)entry.cachedMaxTorpor);
                    float torporPercent = (entry.cachedMaxTorpor > 0) ? (entry.cachedTorpor / entry.cachedMaxTorpor) : 0.0f;
                    ImU32 torporCol = g_Util::GetU32Color(TorporColor);
                    entry.flags.push_back({ torporStr, torporCol, g_ESP::FlagPos::Bottom });
                    entry.bars.push_back({ entry.cachedTorpor, entry.cachedMaxTorpor, torporCol, g_ESP::BarPos::Bottom, g_ESP::BarOrientation::Horizontal });
                }

                if (bDrawDistance) {
                    float dist = 0.0f;
                    if (LocalPC->Pawn && TargetActor) {
                        dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
                    }
                    entry.flags.push_back({ std::to_string((int)dist) + "m", g_Util::GetU32Color(DistanceColor), g_ESP::FlagPos::Right });
                }

                SDK::FVector2D screenPos;
                if (LocalPC->ProjectWorldLocationToScreen(actorLoc, &screenPos, false)) {
                    if (screenPos.X > 0 && screenPos.X < screenW && screenPos.Y > 0 && screenPos.Y < screenH) {
                        entry.targetAlpha = entry.configBoxAlpha;
                        entry.isOOF = false;
                    }
                    else {
                        if (g_Config::bEnableOOF) {
                            entry.isOOF = true;
                            entry.targetAlpha = entry.configBoxAlpha;
                            entry.flags.clear();
                            if (bDrawName && !entry.name.empty()) entry.flags.push_back({ entry.name, entry.nameColor, g_ESP::FlagPos::Right });
                            float dist = 0.0f;
                            if (LocalPC->Pawn && TargetActor) {
                                dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
                            }
                            if (bDrawDistance) entry.flags.push_back({ std::to_string((int)(dist)) + "m", g_Util::GetU32Color(DistanceColor), g_ESP::FlagPos::Right });
                        }
                        else {
                            entry.targetAlpha = 0.0f;
                            entry.aliveThisFrame = false;
                        }
                    }
                }
                else {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                }
            }

            else if (g_Config::bDrawWater && TargetActor && TargetActor->IsA(SDK::APhysicsVolume::StaticClass()) && (((SDK::APhysicsVolume*)TargetActor)->bWaterVolume || ((SDK::APhysicsVolume*)TargetActor)->bDynamicWaterVolume)) {
                SDK::FVector Origin, Extend;
                TargetActor->GetActorBounds(false, &Origin, &Extend, false);

                SDK::FVector WaterSurfaceLocation = { Origin.X, Origin.Y, Origin.Z + Extend.Z };
                float dist = 0.0f;
                if (LocalPC->Pawn && TargetActor) {
                    dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
                }

                if (dist > g_Config::WaterMaxDistance) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
                }

                SDK::FVector2D screenPos;
                if (LocalPC->ProjectWorldLocationToScreen(WaterSurfaceLocation, &screenPos, false)) {
                    if (screenPos.X > 0 && screenPos.X < screenW && screenPos.Y > 0 && screenPos.Y < screenH) {
                        entry.aliveThisFrame = true;
                        entry.targetAlpha = 1.0f;
                        entry.isItem = true;
                        entry.cachedRect.topLeft = ImVec2(screenPos.X - 2, screenPos.Y - 2);
                        entry.cachedRect.bottomRight = ImVec2(screenPos.X + 2, screenPos.Y + 2);
                        entry.cachedRect.valid = true;
                        entry.flags.clear();
                        entry.bars.clear();
                        ImU32 waterColor = g_Util::GetU32Color(g_Config::WaterNameColor);

                        SDK::FString waterString = L"水源";
                        entry.flags.push_back({ waterString.ToString(), waterColor, g_ESP::FlagPos::Right });
                        entry.flags.push_back({ std::to_string((int)dist) + "m", g_Util::GetU32Color(g_Config::WaterDistanceColor), g_ESP::FlagPos::Right });

                        entry.shouldDrawBox = false;
                        entry.shouldDrawHealthBar = false;
                        entry.shouldDrawName = false;
                        entry.shouldDrawDistance = true;
                        entry.shouldDrawTorpor = false;
                    }
                    else {
                        entry.targetAlpha = 0.0f;
                        entry.aliveThisFrame = false;
                    }
                }
                else {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                }
            }

            else if (g_Config::bDrawDroppedItems && TargetActor && TargetActor->IsA(SDK::ADroppedItem::StaticClass())) {
                SDK::ADroppedItem* DroppedItem = (SDK::ADroppedItem*)TargetActor;
                if (!DroppedItem || !DroppedItem->MyItem) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
                }

                SDK::UPrimalItem* Item = DroppedItem->MyItem;
                if (!Item) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
                }

                float dist = 0.0f;
                if (LocalPC->Pawn && TargetActor) {
                    dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
                }
                if (dist > g_Config::DroppedItemMaxDistance) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
                }

                SDK::FVector2D screenPos;
                if (LocalPC->ProjectWorldLocationToScreen(TargetActor->K2_GetActorLocation(), &screenPos, false)) {
                    if (screenPos.X > 0 && screenPos.X < screenW && screenPos.Y > 0 && screenPos.Y < screenH) {
                        entry.targetAlpha = 1.0f;
                        entry.isItem = true;
                        entry.cachedRect.topLeft = ImVec2(screenPos.X - 5, screenPos.Y - 5);
                        entry.cachedRect.bottomRight = ImVec2(screenPos.X + 5, screenPos.Y + 5);
                        entry.cachedRect.valid = true;

                        std::string itemName = "";
                        if (Item->CustomItemName.IsValid() && Item->CustomItemName.ToString() != "") {
                            itemName = Item->CustomItemName.ToString();
                        }
                        else if (Item->DescriptiveNameBase.IsValid()) {
                            itemName = Item->DescriptiveNameBase.ToString();
                        }
                        else {
                            itemName = Item->Class ? Item->Class->GetName() : "Unknown Item";
                        }

                        ImU32 finalCol = g_Util::GetU32Color(g_Config::DroppedItemNameColor);
                        std::string className = Item->Class ? Item->Class->GetName() : "";
                        int quantity = Item->ItemQuantity;

                        // 优先判断特殊容器（如低温仓）
                        if (className.find("PrimalItem_WeaponEmptyCryopod") != std::string::npos) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemCryopodColor);
                        }

                        // 蛋
                        else if (className.find("Egg") != std::string::npos) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemEggColor);
                        }

                        // 堆叠包
                        else if (quantity >= 1000) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemPiledColor);
                        }

                        // 木头
                        else if (
                            className.find("PrimalItemResource_Wood") != std::string::npos || 
                            className.find("PrimalItemResource_FungalWood") != std::string::npos
                            ) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemWoodColor);
                        }

                        // 茅草
                        else if (className.find("PrimalItemResource_Thatch") != std::string::npos) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemThatchColor);
                        }

                        // 兽皮
                        else if (className.find("PrimalItemResource_Hide") != std::string::npos) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemHideColor);
                        }

                        // 毛皮
                        else if (className.find("PrimalItemResource_Pelt") != std::string::npos) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemPeltColor);
                        }

                        // 角质
                        else if (className.find("PrimalItemResource_Keratin") != std::string::npos) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemKeratinColor);
                        }

                        // 甲壳素
                        else if (className.find("PrimalItemResource_Chitin") != std::string::npos) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemChitinColor);
                        }

                        // 腐化瘤
                        else if (className.find("PrimalItemResource_CorruptedPolymer") != std::string::npos) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemCorruptedPolymerColor);
                        }

                        // 有机聚合物
                        else if (className.find("PrimalItemResource_Polymer_Organic") != std::string::npos) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemPolymer_OrganicColor);
                        }

                        // 聚合物
                        else if (className.find("PrimalItemResource_Polymer") != std::string::npos) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemPolymerColor);
                        }

                        // 金属
                        else if (
                            className.find("PrimalItemResource_Metal") != std::string::npos ||
                            className.find("PrimalItemResource_ScrapMetal") != std::string::npos ||
                            className.find("PrimalItemResource_MetalIngot") != std::string::npos ||
                            className.find("PrimalItemResource_ScrapMetalIngot") != std::string::npos
                            ) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemMetalColor);
                        }

                        // 石头
                        else if (className.find("PrimalItemResource_Stone") != std::string::npos) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemStoneColor);
                        }

                        // 水晶
                        else if (className.find("PrimalItemResource_Crystal") != std::string::npos) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemCrystalColor);
                        }

                        // 宝石/精化树脂
                        else if (
                            className.find("PrimalItemResource_Gem_Fertile") != std::string::npos ||
                            className.find("PrimalItemResource_Gem_BioLum") != std::string::npos ||
                            className.find("PrimalItemResource_Gem_Element") != std::string::npos ||
                            className.find("PrimalItemResource_BlueSap") != std::string::npos ||
                            className.find("PrimalItemResource_RedSap") != std::string::npos
                            ) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemGemColor);
                        }

                        // 珍珠
                        else if (
                            className.find("PrimalItemResource_Silicon") != std::string::npos ||
                            className.find("PrimalItemResource_BlackPearl") != std::string::npos
                            ) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemPearlColor);
                        }

                        // 腐肉
                        else if (className.find("PrimalItemConsumable_SpoiledMeat") != std::string::npos) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemSpoiledMeatColor);
                        }

                        // 熟肉
                        else if (
                            className.find("PrimalItemConsumable_RawMeat") != std::string::npos || 
                            className.find("PrimalItemConsumable_RawPrimeMeat") != std::string::npos || 
                            className.find("PrimalItemConsumable_RawMutton") != std::string::npos ||
                            className.find("PrimalItemConsumable_RawPrimeMeat_Fish") != std::string::npos ||
                            className.find("PrimalItemConsumable_RawMeat_Fish") != std::string::npos ||
                            className.find("PrimalItemConsumable_CookedMeat") != std::string::npos ||
                            className.find("PrimalItemConsumable_CookedPrimeMeat") != std::string::npos ||
                            className.find("PrimalItemConsumable_CookedLambChop") != std::string::npos ||
                            className.find("PrimalItemConsumable_CookedPrimeMeat_Fish") != std::string::npos ||
                            className.find("PrimalItemConsumable_CookedMeat_Fish") != std::string::npos ||
                            className.find("PrimalItemConsumable_CookedMeat_Jerky") != std::string::npos ||
                            className.find("PrimalItemConsumable_CookedPrimeMeat_Jerky") != std::string::npos
                            ) {
                            finalCol = g_Util::GetU32Color(g_Config::DroppedItemMeatColor);
                        }

                        // 品质
                        else {
                            float rating = Item->ItemRating;
                            if (rating >= 10.0f)      finalCol = g_Util::ToImColor(0, 255, 255, 255);   // 青色 (传说)
                            else if (rating >= 7.0f)  finalCol = g_Util::ToImColor(255, 255, 0, 255);   // 黄色 (史诗)
                            else if (rating >= 4.5f)  finalCol = g_Util::ToImColor(160, 32, 240, 255);  // 紫色 (卓越)
                            else if (rating >= 2.5f)  finalCol = g_Util::ToImColor(0, 191, 255, 255);   // 蓝色 (精良)
                            else if (rating >= 1.25f) finalCol = g_Util::ToImColor(50, 205, 50, 255);   // 绿色 (优良)
                        }

                        entry.flags.clear();
                        entry.bars.clear();
                        std::string label = "[" + itemName + "]";
                        if (Item->ItemQuantity > 1) label += " x" + std::to_string(Item->ItemQuantity);
                        if (Item->bIsBlueprint) label = "[BP] " + label;

                        entry.flags.push_back({ label, finalCol, g_ESP::FlagPos::Right });
                        entry.flags.push_back({ std::to_string((int)dist) + "m", g_Util::GetU32Color(g_Config::DroppedItemDistanceColor), g_ESP::FlagPos::Right });

                        entry.boxColor = finalCol;
                        entry.nameColor = g_Util::GetU32Color(g_Config::DroppedItemNameColor);

                        entry.shouldDrawBox = false;
                        entry.shouldDrawHealthBar = false;
                        entry.shouldDrawName = false;
                        entry.shouldDrawDistance = true;
                        entry.shouldDrawTorpor = false;
                    }
                    else {
                        entry.targetAlpha = 0.0f;
                        entry.aliveThisFrame = false;
                    }
                }
                else {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                }
            }

            else if (g_Config::bDrawStructures && TargetActor && TargetActor->IsA(SDK::APrimalStructure::StaticClass())) {
                SDK::APrimalStructure* Structure = (SDK::APrimalStructure*)TargetActor;
                if (!Structure) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
                }

                if (Structure->Health <= 0.0f) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
                }

                if (g_Config::bEnableStructureFilter && !g_Config::structureSearchBuf[0] == '\0') {
                    std::string structureName = Structure->GetDescriptiveName().ToString();
                    if (structureName.empty() || structureName == "None") {
                        structureName = "Structure";
                    }

                    if (!g_Util::IsStructureMatch(structureName, g_Config::structureSearchBuf)) {
                        entry.targetAlpha = 0.0f;
                        entry.aliveThisFrame = false;
                        continue;
                    }
                }

                float dist = 0.0f;
                if (LocalPC->Pawn && TargetActor) {
                    dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
                }

                if (dist > g_Config::StructureMaxDistance) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
                }

                SDK::FVector2D screenPos;
                if (LocalPC->ProjectWorldLocationToScreen(TargetActor->K2_GetActorLocation(), &screenPos, false)) {
                    if (screenPos.X > 0 && screenPos.X < screenW && screenPos.Y > 0 && screenPos.Y < screenH) {
                        entry.aliveThisFrame = true;
                        entry.targetAlpha = 1.0f;
                        entry.isItem = true;
                        entry.cachedRect.valid = true;
                        entry.cachedRect.topLeft = ImVec2(screenPos.X - 2, screenPos.Y - 2);
                        entry.cachedRect.bottomRight = ImVec2(screenPos.X + 2, screenPos.Y + 2);

                        std::string sName = Structure->GetDescriptiveName().ToString();
                        if (sName.empty() || sName == "None") sName = "Structure";

                        float curHP = Structure->Health;
                        float maxHP = Structure->MaxHealth;
                        float healthPercent = (maxHP > 0) ? (curHP / maxHP) : 0.0f;
                        int hpPercent = (int)(healthPercent * 100.0f);

                        std::string ownerStr = Structure->OwnerName.ToString();
                        if (ownerStr.empty() || ownerStr == "None") {
                            ownerStr = "*";
                        }

                        entry.flags.clear();
                        entry.bars.clear();
                        entry.flags.push_back({ "[" + sName + "]", g_Util::GetU32Color(g_Config::StructureNameColor), g_ESP::FlagPos::Right });
                        entry.flags.push_back({ ownerStr, g_Util::GetU32Color(g_Config::StructureOwnerColor), g_ESP::FlagPos::Right });
                        ImU32 hpColor = g_Util::GetHealthColor(healthPercent);
                        std::string hpText = std::to_string((int)curHP) + " (" + std::to_string(hpPercent) + "%)";
                        entry.flags.push_back({ hpText, hpColor, g_ESP::FlagPos::Right });
                        entry.flags.push_back({ std::to_string((int)dist) + "m", g_Util::GetU32Color(g_Config::StructureDistanceColor), g_ESP::FlagPos::Right });

                        entry.shouldDrawTorpor = false;
                    }
                    else {
                        entry.targetAlpha = 0.0f;
                        entry.aliveThisFrame = false;
                    }
                }
                else {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                }
            }
            else {
                entry.targetAlpha = 0.0f;
                entry.aliveThisFrame = false;
            }
        }

        for (auto& kv : s_entries) {
            if (!kv.second.aliveThisFrame) {
                kv.second.targetAlpha = 0.0f;
            }
        }

        std::vector<uintptr_t> toErase;
        for (auto& kv : s_entries) {
            ESPEntry& entry = kv.second;

            float fadeTime = (entry.targetAlpha > entry.alpha) ? FADE_IN_TIME : FADE_OUT_TIME;
            entry.alpha = ApproachAlpha(entry.alpha, entry.targetAlpha, deltaTime, fadeTime);

            if (entry.alpha <= 0.001f && entry.targetAlpha <= 0.001f && !entry.aliveThisFrame) {
                toErase.push_back(kv.first);
                continue;
            }

            if (entry.alpha > 0.001f) {
                float alpha255 = entry.alpha * 255.0f;
                ImDrawList* drawList = ImGui::GetBackgroundDrawList();

                if (!entry.isItem && entry.shouldDrawBox && entry.cachedRect.valid) {
                    ImVec4 boxF = ImGui::ColorConvertU32ToFloat4(entry.boxColor);
                    float drawA = entry.configBoxAlpha * entry.alpha;
                    ImU32 bgShadow = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, drawA));
                    ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(boxF.x, boxF.y, boxF.z, drawA));
                    drawList->AddRect(ImVec2(entry.cachedRect.topLeft.x - 1, entry.cachedRect.topLeft.y - 1),
                        ImVec2(entry.cachedRect.bottomRight.x + 1, entry.cachedRect.bottomRight.y + 1),
                        bgShadow, 0.0f, 0, 1.5f);
                    drawList->AddRect(entry.cachedRect.topLeft, entry.cachedRect.bottomRight, col, 0.0f, 0, 1.0f);
                }

                g_ESP::BarManager bm;
                bm.Reset();
                for (auto& bar : entry.bars) {
                    bm.AddBar(entry.cachedRect, bar.currentValue, bar.maxValue, bar.color, bar.pos, bar.orientation, alpha255);
                }

                g_ESP::FlagManager fm;
                fm.Reset();
                for (auto& f : entry.flags) {
                    fm.AddFlag(entry.cachedRect, f.text, f.color, f.pos, entry.alpha, &bm);
                }

                if (entry.isOOF) {
                    std::vector<g_ESP::OOFFlag> oofFlags;
                    for (auto& ff : entry.flags) oofFlags.push_back({ ff.text, ff.color });
                    g_ESP::DrawOutOfFOV(entry.lastWorldLoc, LocalPC, oofFlags, entry.alpha);
                }
            }
        }

        for (uintptr_t k : toErase) s_entries.erase(k);
    }
}