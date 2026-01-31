// DrawESP.cpp
#include "../Minimal-D3D12-Hook-ImGui-1.0.2/Main/mdx12_api.h"
#include "SDK_Headers.hpp"
#include "ESP.h"
#include "Configs.h"
#include "DrawESP.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>
#include <memory>

namespace g_DrawESP {
    inline ImU32 ToImColor(float r, float g, float b, float a) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
    }

    inline ImU32 GetHealthColor(float healthPercent) {
        healthPercent = (healthPercent < 0.0f) ? 0.0f : (healthPercent > 1.0f) ? 1.0f : healthPercent;

        float r, g, b;
        if (healthPercent > 0.5f) {
            float t = (healthPercent - 0.5f) * 2.0f;
            r = 1.0f - t;
            g = 1.0f;
            b = 0.0f;
        }
        else {
            float t = healthPercent * 2.0f;
            r = 1.0f;
            g = t;
            b = 0.0f;
        }

        return ToImColor(r * 255.0f, g * 255.0f, b * 255.0f, 255.0f);
    }

    static constexpr float FADE_IN_TIME = 0.10f;
    static constexpr float FADE_OUT_TIME = 0.20f;

    struct CachedFlag {
        std::string text;
        ImU32 color;
        g_ESP::FlagPos pos;
    };

    struct ESPEntry {
        uintptr_t actorKey = 0;
        SDK::FVector lastWorldLoc{};
        g_ESP::BoxRect cachedRect;
        std::string name;
        std::vector<CachedFlag> flags;
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

        bool shouldDrawBox = false;
        bool shouldDrawHealthBar = false;
        bool shouldDrawName = false;
        bool shouldDrawSpecies = false;
        bool shouldDrawDistance = false;
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

        SDK::APlayerController* LocalPC = g_ESP::GetLocalPC();
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

            if (TargetActor->IsA(SDK::APrimalCharacter::StaticClass())) {
                SDK::APrimalCharacter* TargetChar = (SDK::APrimalCharacter*)TargetActor;
                SDK::APrimalCharacter* LocalChar = (SDK::APrimalCharacter*)LocalPC->Pawn;

                if (!TargetChar || !LocalChar) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
                }

                bool isDead = TargetChar->IsDead();
                if (isDead) {
                    entry.targetAlpha = 0.0f;
                    g_ESP::BoxRect rect = g_ESP::DrawBox(TargetActor,
                        255.0f, 255.0f, 255.0f, 255.0f, 0.5f, true);
                    if (rect.valid) entry.cachedRect = rect;
                    continue;
                }

                if (g_Config::bEnableFilter && !searchFilter.empty()) {
                    std::string nameForESP = TargetChar->PlayerState ?
                        TargetChar->PlayerState->GetPlayerName().ToString() :
                        TargetChar->GetDescriptiveName().ToString();

                    if (!g_ESP::IsEntityMatch(nameForESP, searchFilter)) {
                        entry.targetAlpha = 0.0f;
                        entry.aliveThisFrame = false;
                        continue;
                    }
                }

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

                SDK::FVector actorLoc = TargetActor->K2_GetActorLocation();

                g_ESP::BoxRect rect = g_ESP::DrawBox(TargetActor,
                    BoxColor[0] * 255.0f, BoxColor[1] * 255.0f,
                    BoxColor[2] * 255.0f, BoxColor[3] * 255.0f, 0.5f, true);

                entry.cachedRect = rect;
                entry.boxColor = g_Config::GetU32Color(BoxColor);
                entry.nameColor = g_Config::GetU32Color(NameColor);
                entry.configBoxAlpha = BoxColor[3];
                entry.isItem = false;
                entry.isOOF = false;
                entry.cachedHP = TargetChar->GetHealth();
                entry.cachedMaxHP = TargetChar->GetMaxHealth();

                entry.shouldDrawBox = bDrawBox;
                entry.shouldDrawHealthBar = bDrawHealthBar;
                entry.shouldDrawName = bDrawName;
                entry.shouldDrawSpecies = bDrawSpecies;
                entry.shouldDrawDistance = bDrawDistance;

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
                if (bDrawHealthBar) {
                    std::string hpStr = std::to_string((int)entry.cachedHP);
                    float healthPercent = (entry.cachedMaxHP > 0) ? (entry.cachedHP / entry.cachedMaxHP) : 0.0f;
                    ImU32 hpCol = GetHealthColor(healthPercent);
                    entry.flags.push_back({ hpStr, hpCol, g_ESP::FlagPos::Left });
                }

                if (bDrawSpecies) {
                    std::string species = TargetActor->IsA(SDK::APrimalDinoCharacter::StaticClass()) ? ((SDK::APrimalDinoCharacter*)TargetActor)->GetDescriptiveName().ToString() : "Human";
                    entry.flags.push_back({ species, ToImColor(220, 220, 220, 255), g_ESP::FlagPos::Right });
                }

                if (bDrawDistance) {
                    float dist = 0.0f;
                    if (LocalPC->Pawn && TargetActor) {
                        dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
                    }
                    entry.flags.push_back({ std::to_string((int)dist) + "m", g_Config::GetU32Color(DistanceColor), g_ESP::FlagPos::Right });
                }

                SDK::FVector2D screenPos;
                if (LocalPC->ProjectWorldLocationToScreen(actorLoc, &screenPos, false)) {
                    if (screenPos.X > 0 && screenPos.X < screenW && screenPos.Y > 0 && screenPos.Y < screenH) {
                        entry.targetAlpha = 1.0f;
                        entry.isOOF = false;
                    }
                    else {
                        if (g_Config::bEnableOOF) {
                            entry.isOOF = true;
                            entry.targetAlpha = 1.0f;
                            entry.flags.clear();
                            if (bDrawName) entry.flags.push_back({ entry.name.empty() ? (TargetPS ? TargetPS->GetPlayerName().ToString() : TargetChar->GetDescriptiveName().ToString()) : entry.name, entry.nameColor, g_ESP::FlagPos::Right });
                            float dist = 0.0f;
                            if (LocalPC->Pawn && TargetActor) {
                                dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
                            }
                            if (bDrawDistance) entry.flags.push_back({ std::to_string((int)(dist)) + "m", g_Config::GetU32Color(DistanceColor), g_ESP::FlagPos::Right });
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

            else if (g_Config::bDrawWater && TargetActor->IsA(SDK::APhysicsVolume::StaticClass()) && ((SDK::APhysicsVolume*)TargetActor)->bWaterVolume) {
                SDK::APhysicsVolume* WaterVolume = (SDK::APhysicsVolume*)TargetActor;
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
                        ImU32 waterColor = ToImColor(0, 200, 255, 255);

                        entry.flags.push_back({ "[Water]", waterColor, g_ESP::FlagPos::Right });
                        entry.flags.push_back({ std::to_string((int)dist) + "m", ToImColor(200, 200, 200, 255), g_ESP::FlagPos::Right });

                        entry.shouldDrawBox = false;
                        entry.shouldDrawHealthBar = false;
                        entry.shouldDrawName = false;
                        entry.shouldDrawDistance = true;
                        entry.shouldDrawSpecies = false;
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

            else if (g_Config::bDrawDroppedItems && TargetActor->IsA(SDK::ADroppedItem::StaticClass())) {
                SDK::ADroppedItem* DroppedItem = (SDK::ADroppedItem*)TargetActor;
                if (!DroppedItem || !DroppedItem->MyItem) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
                }

                SDK::UPrimalItem* Item = DroppedItem->MyItem;
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

                        ImU32 finalCol = ToImColor(220, 220, 220, 255);
                        std::string className = Item->Class ? Item->Class->GetName() : "";
                        int quantity = Item->ItemQuantity;

                        if (className.find("Cryopod") != std::string::npos) {
                            finalCol = ToImColor(255, 100, 255, 255);
                        }
                        else if (quantity >= 1000) {
                            finalCol = ToImColor(50, 255, 50, 255);
                        }
                        else if (className.find("Meat") != std::string::npos) {
                            finalCol = ToImColor(139, 69, 19, 255);
                        }
                        else {
                            float rating = Item->ItemRating;
                            if (rating >= 10.0f)      finalCol = ToImColor(0, 255, 255, 255);
                            else if (rating >= 7.0f)  finalCol = ToImColor(255, 255, 0, 255);
                            else if (rating >= 4.5f)  finalCol = ToImColor(160, 32, 240, 255);
                            else if (rating >= 2.5f)  finalCol = ToImColor(0, 191, 255, 255);
                            else if (rating >= 1.25f) finalCol = ToImColor(50, 205, 50, 255);
                        }

                        entry.flags.clear();
                        std::string label = "[" + itemName + "]";
                        if (Item->ItemQuantity > 1) label += " x" + std::to_string(Item->ItemQuantity);
                        if (Item->bIsBlueprint) label = "[BP] " + label;

                        entry.flags.push_back({ label, finalCol, g_ESP::FlagPos::Right });
                        entry.flags.push_back({ std::to_string((int)dist) + "m", ToImColor(200, 200, 200, 200), g_ESP::FlagPos::Right });

                        entry.boxColor = finalCol;
                        entry.nameColor = ToImColor(220, 220, 220, 255);

                        entry.shouldDrawBox = false;
                        entry.shouldDrawHealthBar = false;
                        entry.shouldDrawName = false;
                        entry.shouldDrawDistance = true;
                        entry.shouldDrawSpecies = false;
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

            else if (g_Config::bDrawStructures && TargetActor->IsA(SDK::APrimalStructure::StaticClass())) {
                SDK::APrimalStructure* Structure = (SDK::APrimalStructure*)TargetActor;

                if (Structure->Health <= 0.0f) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
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
                        entry.flags.push_back({ "[" + sName + "]", ToImColor(255, 255, 180, 255), g_ESP::FlagPos::Right });
                        entry.flags.push_back({ ownerStr, ToImColor(100, 255, 255, 255), g_ESP::FlagPos::Right });
                        ImU32 hpColor = GetHealthColor(healthPercent);
                        std::string hpText = std::to_string((int)curHP) + " (" + std::to_string(hpPercent) + "%)";
                        entry.flags.push_back({ hpText, hpColor, g_ESP::FlagPos::Right });
                        entry.flags.push_back({ std::to_string((int)dist) + "m", ToImColor(255, 255, 255, 255), g_ESP::FlagPos::Right });
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

                if (!entry.isItem && entry.shouldDrawHealthBar && entry.cachedMaxHP > 0.0f) {
                    g_ESP::DrawHealthBar(entry.cachedRect, entry.cachedHP, entry.cachedMaxHP, alpha255);
                }

                if (entry.shouldDrawName && !entry.name.empty() && entry.cachedRect.valid) {
                    ImVec4 nf = ImGui::ColorConvertU32ToFloat4(entry.nameColor);
                    nf.w *= entry.alpha;
                    ImU32 ncol = ImGui::ColorConvertFloat4ToU32(nf);
                    ImU32 shadow = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, entry.alpha));
                    ImVec2 textSize = ImGui::CalcTextSize(entry.name.c_str());
                    ImVec2 textPos = ImVec2(entry.cachedRect.topLeft.x + (entry.cachedRect.bottomRight.x - entry.cachedRect.topLeft.x) / 2.0f - textSize.x / 2.0f,
                        entry.cachedRect.topLeft.y - textSize.y - 5.0f);
                    drawList->AddText(ImVec2(textPos.x + 1, textPos.y + 1), shadow, entry.name.c_str());
                    drawList->AddText(textPos, ncol, entry.name.c_str());
                }

                g_ESP::FlagManager fm;
                fm.Reset();
                for (auto& f : entry.flags) {
                    fm.AddFlag(entry.cachedRect, f.text, f.color, f.pos, entry.alpha);
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