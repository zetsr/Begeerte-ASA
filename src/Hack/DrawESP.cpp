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

    // 淡入/淡出时长（秒）
    static constexpr float FADE_IN_TIME = 0.12f;
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

        // per-entry draw toggles (snapshot at observation)
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
        if (!LocalPC || !LocalPC->Pawn) return;

        SDK::APlayerState* LocalPS = LocalPC->PlayerState;
        if (!LocalPS) return;

        ImGuiIO& io = ImGui::GetIO();
        float screenW = io.DisplaySize.x;
        float screenH = io.DisplaySize.y;
        float deltaTime = ImGui::GetIO().DeltaTime;

        SDK::TArray<SDK::AActor*>& Actors = World->PersistentLevel->Actors;

        // Mark all entries not seen this frame
        for (auto& kv : s_entries) {
            kv.second.aliveThisFrame = false;
        }

        std::string searchFilter = g_Config::entitySearchBuf;

        // 1) Scan current actors and update/create entries
        for (int i = 0; i < Actors.Num(); i++) {
            SDK::AActor* TargetActor = Actors[i];

            if (!TargetActor || TargetActor == LocalPC->Pawn) continue;

            uintptr_t key = reinterpret_cast<uintptr_t>(TargetActor);
            ESPEntry& entry = s_entries[key];

            entry.actorKey = key;
            entry.lastSeenTime = 0.0f;

            // If hidden, treat as should fade out (and don't mark aliveThisFrame)
            if (TargetActor->bHidden) {
                entry.targetAlpha = 0.0f;
                entry.lastWorldLoc = TargetActor->K2_GetActorLocation();
                entry.aliveThisFrame = false;
                continue;
            }

            // Else mark alive this frame
            entry.aliveThisFrame = true;
            entry.lastWorldLoc = TargetActor->K2_GetActorLocation();

            // Case A: Character (players/dinos)
            if (TargetActor->IsA(SDK::APrimalCharacter::StaticClass())) {
                SDK::APrimalCharacter* TargetChar = (SDK::APrimalCharacter*)TargetActor;
                SDK::APrimalCharacter* LocalChar = (SDK::APrimalCharacter*)LocalPC->Pawn;

                bool isDead = TargetChar->IsDead();
                if (isDead) {
                    // entity dead -> start fading out, but cache last rect so fade uses last position
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

                // compute cached rect (test only)
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

                // store toggles (this is the fix: store per-entry desired draw flags)
                entry.shouldDrawBox = bDrawBox;
                entry.shouldDrawHealthBar = bDrawHealthBar;
                entry.shouldDrawName = bDrawName;
                entry.shouldDrawSpecies = bDrawSpecies;
                entry.shouldDrawDistance = bDrawDistance;

                // Ensure we cache the name if name drawing is enabled
                if (bDrawName) {
                    entry.name = TargetPS ? TargetPS->GetPlayerName().ToString() : TargetChar->GetDescriptiveName().ToString();
                }
                else {
                    entry.name.clear();
                }

                // prepare flags only if relevant (e.g. health text only if health bar enabled)
                entry.flags.clear();
                if (bDrawHealthBar) {
                    std::string hpStr = std::to_string((int)entry.cachedHP);
                    ImU32 hpCol = (entry.cachedMaxHP > 0 && (entry.cachedHP / entry.cachedMaxHP) > 0.5f) ? ToImColor(100, 255, 100, 255) : ToImColor(255, 100, 100, 255);
                    entry.flags.push_back({ hpStr, hpCol, g_ESP::FlagPos::Left });
                }

                if (bDrawSpecies) {
                    std::string species = TargetActor->IsA(SDK::APrimalDinoCharacter::StaticClass()) ? ((SDK::APrimalDinoCharacter*)TargetActor)->GetDescriptiveName().ToString() : "Human";
                    entry.flags.push_back({ species, ToImColor(220, 220, 220, 255), g_ESP::FlagPos::Right });
                }

                if (bDrawDistance) {
                    float dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
                    entry.flags.push_back({ std::to_string((int)dist) + "m", g_Config::GetU32Color(DistanceColor), g_ESP::FlagPos::Right });
                }

                // On-screen / off-screen
                SDK::FVector2D screenPos;
                if (LocalPC->ProjectWorldLocationToScreen(actorLoc, &screenPos, false)) {
                    if (screenPos.X > 0 && screenPos.X < screenW && screenPos.Y > 0 && screenPos.Y < screenH) {
                        entry.targetAlpha = 1.0f;
                        entry.isOOF = false;
                    }
                    else {
                        if (g_Config::bEnableOOF) {
                            // OOF: keep name/distance flags based on toggles
                            entry.isOOF = true;
                            entry.targetAlpha = 1.0f;
                            entry.flags.clear();
                            if (bDrawName) entry.flags.push_back({ entry.name.empty() ? (TargetPS ? TargetPS->GetPlayerName().ToString() : TargetChar->GetDescriptiveName().ToString()) : entry.name, entry.nameColor, g_ESP::FlagPos::Right });
                            if (bDrawDistance) entry.flags.push_back({ std::to_string((int)(LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f)) + "m", g_Config::GetU32Color(DistanceColor), g_ESP::FlagPos::Right });
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
            } // end character handling

            // Case B: Dropped items
            else if (g_Config::bDrawDroppedItems && TargetActor->IsA(SDK::ADroppedItem::StaticClass())) {
                SDK::ADroppedItem* DroppedItem = (SDK::ADroppedItem*)TargetActor;
                if (!DroppedItem || !DroppedItem->MyItem) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
                }

                SDK::UPrimalItem* Item = DroppedItem->MyItem;
                float dist = LocalPC->Pawn ? LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f : 0.0f;
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

                        // Items: only flags (name + distance). Do NOT draw icon/box.
                        entry.flags.push_back({ label, finalCol, g_ESP::FlagPos::Right });
                        entry.flags.push_back({ std::to_string((int)dist) + "m", ToImColor(200, 200, 200, 200), g_ESP::FlagPos::Right });

                        entry.boxColor = finalCol;
                        entry.nameColor = ToImColor(220, 220, 220, 255);

                        // set draw toggles for items (no main box, no health)
                        entry.shouldDrawBox = false;
                        entry.shouldDrawHealthBar = false;
                        entry.shouldDrawName = false; // avoid duplicating with flags
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

            // Case C: Supply crates / containers
            else if (g_Config::bDrawSupplyDrops && TargetActor->IsA(SDK::APrimalStructureItemContainer::StaticClass())) {
                if (TargetActor->IsA(SDK::APrimalStructureTurret::StaticClass())) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
                }

                std::string actorName = TargetActor->Class ? TargetActor->Class->GetName() : "";
                if (actorName.find("Crate") == std::string::npos && actorName.find("SupplyDrop") == std::string::npos) {
                    entry.targetAlpha = 0.0f;
                    entry.aliveThisFrame = false;
                    continue;
                }

                float dist = LocalPC->Pawn ? LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f : 0.0f;
                if (dist > g_Config::SupplyDropMaxDistance) {
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

                        ImU32 crateCol = ToImColor(255, 255, 255, 255);
                        std::string levelTag = "[Lvl 3]";

                        if (actorName.find("Level60") != std::string::npos || actorName.find("Red") != std::string::npos) {
                            crateCol = ToImColor(255, 50, 50, 255); levelTag = "[Lvl 60]";
                        }
                        else if (actorName.find("Level45") != std::string::npos || actorName.find("Yellow") != std::string::npos) {
                            crateCol = ToImColor(255, 255, 0, 255); levelTag = "[Lvl 45]";
                        }
                        else if (actorName.find("Level35") != std::string::npos || actorName.find("Purple") != std::string::npos) {
                            crateCol = ToImColor(160, 32, 240, 255); levelTag = "[Lvl 35]";
                        }
                        else if (actorName.find("Level25") != std::string::npos || actorName.find("Blue") != std::string::npos) {
                            crateCol = ToImColor(0, 191, 255, 255); levelTag = "[Lvl 25]";
                        }
                        else if (actorName.find("Level15") != std::string::npos || actorName.find("Green") != std::string::npos) {
                            crateCol = ToImColor(50, 255, 50, 255); levelTag = "[Lvl 15]";
                        }
                        else if (actorName.find("Cave") != std::string::npos) {
                            crateCol = ToImColor(0, 255, 255, 255); levelTag = "[Cave]";
                        }

                        SDK::APrimalStructureItemContainer* Container = (SDK::APrimalStructureItemContainer*)TargetActor;
                        std::string cDisplayName = Container->GetDescriptiveName().ToString();
                        if (cDisplayName.empty() || cDisplayName == "None") cDisplayName = "Supply Crate";

                        entry.flags.clear();
                        entry.flags.push_back({ levelTag + " " + cDisplayName, crateCol, g_ESP::FlagPos::Right });
                        entry.flags.push_back({ std::to_string((int)dist) + "m", ToImColor(220, 220, 220, 255), g_ESP::FlagPos::Right });

                        entry.boxColor = crateCol;
                        entry.nameColor = ToImColor(220, 220, 220, 255);

                        // crates treated like items — only flags
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
            else {
                // other types: don't show
                entry.targetAlpha = 0.0f;
                entry.aliveThisFrame = false;
            }
        } // end actors loop

        // Ensure all entries not seen this frame begin fade out
        for (auto& kv : s_entries) {
            if (!kv.second.aliveThisFrame) {
                kv.second.targetAlpha = 0.0f;
            }
        }

        // 2) Update alpha and draw cached entries
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

                // Draw box only if entry.shouldDrawBox and rect valid and not an item
                if (!entry.isItem && entry.shouldDrawBox && entry.cachedRect.valid) {
                    ImVec4 boxF = ImGui::ColorConvertU32ToFloat4(entry.boxColor);
                    float drawA = entry.configBoxAlpha * entry.alpha; // 0..1
                    ImU32 bgShadow = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, drawA));
                    ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(boxF.x, boxF.y, boxF.z, drawA));
                    drawList->AddRect(ImVec2(entry.cachedRect.topLeft.x - 1, entry.cachedRect.topLeft.y - 1),
                        ImVec2(entry.cachedRect.bottomRight.x + 1, entry.cachedRect.bottomRight.y + 1),
                        bgShadow, 0.0f, 0, 1.5f);
                    drawList->AddRect(entry.cachedRect.topLeft, entry.cachedRect.bottomRight, col, 0.0f, 0, 1.0f);
                }

                // Health bar: only draw if we recorded shouldDrawHealthBar
                if (!entry.isItem && entry.shouldDrawHealthBar && entry.cachedMaxHP > 0.0f) {
                    g_ESP::DrawHealthBar(entry.cachedRect, entry.cachedHP, entry.cachedMaxHP, alpha255);
                }

                // Name: draw only if shouldDrawName (we draw cached name if present)
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

                // Flags: use FlagManager, pass entry.alpha as alphaMult
                g_ESP::FlagManager fm;
                fm.Reset();
                for (auto& f : entry.flags) {
                    fm.AddFlag(entry.cachedRect, f.text, f.color, f.pos, entry.alpha);
                }

                // OOF: draw using lastWorldLoc, passing alpha
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
