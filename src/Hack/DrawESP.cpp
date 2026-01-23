#include "../Minimal-D3D12-Hook-ImGui-1.0.2/Main/mdx12_api.h"
#include "../CppSDK/SDK.hpp"
#include "ESP.h"
#include "Configs.h"
#include "DrawESP.h"
#include <vector>
#include <string>

namespace g_DrawESP {
    inline ImU32 ToImColor(float r, float g, float b, float a) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
    }

    void DrawESP() {
        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (!World || !World->GameState || !World->PersistentLevel) return;

        SDK::APlayerController* LocalPC = g_ESP::GetLocalPC();
        if (!LocalPC || !LocalPC->Pawn) return;

        SDK::APlayerState* LocalPS = LocalPC->PlayerState;
        if (!LocalPS) return;

        ImGuiIO& io = ImGui::GetIO();
        float screenW = io.DisplaySize.x;
        float screenH = io.DisplaySize.y;

        SDK::TArray<SDK::AActor*>& Actors = World->PersistentLevel->Actors;

        for (int i = 0; i < Actors.Num(); i++) {
            SDK::AActor* TargetActor = Actors[i];

            if (!TargetActor || TargetActor == LocalPC->Pawn || TargetActor->bHidden) continue;

            // 情况 A：处理生物 ESP (玩家、恐龙)
            if (TargetActor->IsA(SDK::APrimalCharacter::StaticClass())) {
                SDK::APrimalCharacter* TargetChar = (SDK::APrimalCharacter*)TargetActor;
                SDK::APrimalCharacter* LocalChar = (SDK::APrimalCharacter*)LocalPC->Pawn;

                if (TargetChar->IsDead()) continue;

                SDK::APlayerState* TargetPS = TargetChar->PlayerState;
                g_ESP::RelationType relation = g_ESP::GetRelation(TargetChar, LocalChar);

                bool bDrawBox = false, bDrawHealthBar = false, bDrawName = false;
                bool bDrawSpecies = false, bDrawGrowth = false, bDrawDistance = false;
                float* BoxColor = nullptr;
                float* NameColor = nullptr;
                float* DistanceColor = nullptr;

                if (relation == g_ESP::RelationType::Team) {
                    bDrawBox = g_Config::bDrawBoxTeam;
                    BoxColor = g_Config::BoxColorTeam;
                    bDrawHealthBar = g_Config::bDrawHealthBarTeam;
                    bDrawName = g_Config::bDrawNameTeam;
                    NameColor = g_Config::NameColorTeam;
                    bDrawSpecies = g_Config::bDrawSpeciesTeam;
                    bDrawGrowth = g_Config::bDrawGrowthTeam;
                    bDrawDistance = g_Config::bDrawDistanceTeam;
                    DistanceColor = g_Config::DistanceColorTeam;
                }
                else {
                    bDrawBox = g_Config::bDrawBox;
                    BoxColor = g_Config::BoxColor;
                    bDrawHealthBar = g_Config::bDrawHealthBar;
                    bDrawName = g_Config::bDrawName;
                    NameColor = g_Config::NameColor;
                    bDrawSpecies = g_Config::bDrawSpecies;
                    bDrawGrowth = g_Config::bDrawGrowth;
                    bDrawDistance = g_Config::bDrawDistance;
                    DistanceColor = g_Config::DistanceColor;
                }

                SDK::FVector2D screenPos;
                SDK::FVector actorLoc = TargetActor->K2_GetActorLocation();

                if (LocalPC->ProjectWorldLocationToScreen(actorLoc, &screenPos, false)) {
                    if (screenPos.X > 0 && screenPos.X < screenW && screenPos.Y > 0 && screenPos.Y < screenH) {

                        g_ESP::FlagManager flagMgr;
                        flagMgr.Reset();

                        g_ESP::BoxRect rect = g_ESP::DrawBox(TargetActor,
                            BoxColor[0] * 255.0f, BoxColor[1] * 255.0f,
                            BoxColor[2] * 255.0f, BoxColor[3] * 255.0f, 0.5f, !bDrawBox);

                        if (rect.valid) {
                            if (bDrawHealthBar) {
                                float currentHP = TargetChar->GetHealth();
                                float maxHP = TargetChar->GetMaxHealth();
                                g_ESP::DrawHealthBar(rect, currentHP, maxHP, 255.0f);

                                std::string hpStr = std::to_string((int)currentHP);
                                float hpPercent = (maxHP > 0) ? (currentHP / maxHP) : 0.0f;
                                ImU32 hpCol = (hpPercent > 0.5f) ? ToImColor(100, 255, 100, 255) : ToImColor(255, 100, 100, 255);
                                flagMgr.AddFlag(rect, hpStr, hpCol, g_ESP::FlagPos::Left);
                            }

                            if (bDrawName) {
                                std::string displayName = TargetPS ? TargetPS->GetPlayerName().ToString() : TargetChar->GetDescriptiveName().ToString();
                                g_ESP::DrawName(TargetActor, rect, NameColor[0] * 255.0f, NameColor[1] * 255.0f, NameColor[2] * 255.0f, NameColor[3] * 255.0f);
                            }

                            if (bDrawSpecies) {
                                std::string species = TargetActor->IsA(SDK::APrimalDinoCharacter::StaticClass()) ? ((SDK::APrimalDinoCharacter*)TargetActor)->GetDescriptiveName().ToString() : "Human";
                                flagMgr.AddFlag(rect, species, ToImColor(220, 220, 220, 255), g_ESP::FlagPos::Right);
                            }

                            if (bDrawDistance) {
                                float dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
                                flagMgr.AddFlag(rect, std::to_string((int)dist) + "m", g_Config::GetU32Color(DistanceColor), g_ESP::FlagPos::Right);
                            }
                        }
                    }
                    else if (g_Config::bEnableOOF) {
                        std::vector<g_ESP::OOFFlag> oofFlags;
                        std::string oofName = TargetPS ? TargetPS->GetPlayerName().ToString() : TargetChar->GetDescriptiveName().ToString();
                        if (bDrawName) oofFlags.push_back({ oofName, g_Config::GetU32Color(NameColor) });
                        if (bDrawDistance) oofFlags.push_back({ std::to_string((int)(LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f)) + "m", g_Config::GetU32Color(DistanceColor) });
                        g_ESP::DrawOutOfFOV(TargetActor, LocalPC, oofFlags);
                    }
                }
            }

            // 情况 B：处理掉落物品 ESP
            else if (g_Config::bDrawDroppedItems && TargetActor->IsA(SDK::ADroppedItem::StaticClass())) {
                SDK::ADroppedItem* DroppedItem = (SDK::ADroppedItem*)TargetActor;
                if (!DroppedItem || !DroppedItem->MyItem) continue;

                SDK::UPrimalItem* Item = DroppedItem->MyItem;
                float dist = LocalPC->Pawn ? LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f : 0.0f;
                if (dist > g_Config::DroppedItemMaxDistance) continue;

                SDK::FVector2D screenPos;
                if (LocalPC->ProjectWorldLocationToScreen(TargetActor->K2_GetActorLocation(), &screenPos, false)) {
                    if (screenPos.X > 0 && screenPos.X < screenW && screenPos.Y > 0 && screenPos.Y < screenH) {

                        // 1. 获取显示名称 (优先自定义，其次描述名)
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

                        // 2. 颜色逻辑：优先判断特殊物品，其次判断品质
                        ImU32 finalCol = ToImColor(220, 220, 220, 255); // 默认灰白色 (Primitive)

                        std::string className = Item->Class ? Item->Class->GetName() : "";
                        int quantity = Item->ItemQuantity;

                        // 2.1. 优先级最高：低温仓 (无论多少个，紫色最醒目)
                        if (className.find("Cryopod") != std::string::npos) {
                            finalCol = ToImColor(255, 100, 255, 255); // 亮紫色
                        }
                        // 2.2. 优先级第二：大堆叠物品 ( > 1000 )
                        else if (quantity >= 1000) {
                            finalCol = ToImColor(50, 255, 50, 255);   // 亮绿色
                        }
                        // 2.3. 优先级第三：所有肉类 (Raw Meat, Cooked Meat, Prime Meat 等)
                        // 包含 "Meat" 且不是高品质（防止某些特殊Mod的高级肉被覆盖颜色）
                        else if (className.find("Meat") != std::string::npos) {
                            finalCol = ToImColor(139, 69, 19, 255);   // 褐色 (Saddle/Meat Brown)
                        }
                        // 2.4. 优先级最低：按品质分级
                        else {
                            float rating = Item->ItemRating;
                            if (rating >= 10.0f)      finalCol = ToImColor(0, 255, 255, 255);   // 神话 (青色)
                            else if (rating >= 7.0f)  finalCol = ToImColor(255, 255, 0, 255);   // 传说 (黄色)
                            else if (rating >= 4.5f)  finalCol = ToImColor(160, 32, 240, 255);  // 史诗 (紫色)
                            else if (rating >= 2.5f)  finalCol = ToImColor(0, 191, 255, 255);   // 卓越 (蓝色)
                            else if (rating >= 1.25f) finalCol = ToImColor(50, 205, 50, 255);   // 精良 (绿色)
                            // 默认颜色已在上方定义
                        }

                        // 3. 绘制逻辑
                        g_ESP::BoxRect itemRect;
                        itemRect.topLeft = ImVec2(screenPos.X - 5, screenPos.Y - 5);
                        itemRect.bottomRight = ImVec2(screenPos.X + 5, screenPos.Y + 5);
                        itemRect.valid = true;

                        g_ESP::FlagManager itemFlag;
                        itemFlag.Reset();

                        std::string label = "[" + itemName + "]";
                        if (Item->ItemQuantity > 1) label += " x" + std::to_string(Item->ItemQuantity);

                        // 如果是蓝图，加个标记
                        if (Item->bIsBlueprint) {
                            label = "[BP] " + label;
                        }

                        itemFlag.AddFlag(itemRect, label, finalCol, g_ESP::FlagPos::Right);
                        itemFlag.AddFlag(itemRect, std::to_string((int)dist) + "m", ToImColor(200, 200, 200, 200), g_ESP::FlagPos::Right);
                    }
                }
            }
        }
    }
}