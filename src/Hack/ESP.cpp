// ESP.cpp
#include "../Minimal-D3D12-Hook-ImGui-1.0.2/Main/mdx12_api.h"
#include "SDK_Headers.hpp"
#include "ESP.h"
#include "Configs.h"
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

namespace g_ESP {
    SDK::APlayerController* GetLocalPC() {
        SDK::UWorld* World = SDK::UWorld::GetWorld();

        if (!World) {
            return nullptr;
        }

        if (!World->OwningGameInstance) {
            return nullptr;
        }

        if (World->OwningGameInstance->LocalPlayers.Num() == 0) {
            return nullptr;
        }

        auto LocalPlayer = World->OwningGameInstance->LocalPlayers[0];
        if (!LocalPlayer) {
            return nullptr;
        }

        auto PC = LocalPlayer->PlayerController;
        if (!PC) {
            return nullptr;
        }

        return PC;
    }

    inline ImU32 ToImColor(float r, float g, float b, float a) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
    }

    std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return std::tolower(c);
            });
        return s;
    }

    bool IsEntityMatch(std::string displayName, std::string filter) {
        if (filter.empty()) return true;

        std::string nameLower = ToLower(displayName);
        std::string filterLower = ToLower(filter);

        size_t lastPos = 0;
        for (size_t j = 0; j < filterLower.length(); ) {
            unsigned char c = static_cast<unsigned char>(filterLower[j]);
            int charLen = 1;
            if (c >= 0xf0) charLen = 4;
            else if (c >= 0xe0) charLen = 3;
            else if (c >= 0xc0) charLen = 2;

            std::string sub = filterLower.substr(j, charLen);
            size_t foundPos = nameLower.find(sub, lastPos);

            if (foundPos == std::string::npos) return false;

            lastPos = foundPos + charLen;
            j += charLen;
        }
        return true;
    }

    RelationType GetRelation(SDK::APrimalCharacter* TargetChar, SDK::APrimalCharacter* LocalChar) {
        if (!TargetChar || !LocalChar) return RelationType::Enemy;

        int targetTeam = TargetChar->TargetingTeam;
        int localTeam = LocalChar->TargetingTeam;

        if (targetTeam != 0 && targetTeam == localTeam) {
            return RelationType::Team;
        }

        return RelationType::Enemy;
    }

    void FlagManager::AddFlag(BoxRect rect, const std::string& text, ImU32 color, FlagPos pos, float alphaMult /*=1.0f*/) {
        if (!rect.valid || text.empty()) return;

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();

        ImFont* font = ImGui::GetFont();
        float fontSize = ImGui::GetFontSize();
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());

        ImVec2 drawPos;

        if (pos == FlagPos::Right) {
            drawPos = ImVec2(rect.bottomRight.x + 5.0f, rect.topLeft.y + rightY);
            rightY += textSize.y + 1.0f;
        }
        else {
            drawPos = ImVec2(rect.topLeft.x - 8.0f - textSize.x, rect.topLeft.y + leftY);
            leftY += textSize.y + 1.0f;
        }

        ImVec4 baseCol = ImGui::ColorConvertU32ToFloat4(color);
        baseCol.w *= alphaMult;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(baseCol);

        ImVec4 shadowCol = ImVec4(0, 0, 0, 0.8f * alphaMult);
        ImU32 sCol = ImGui::ColorConvertFloat4ToU32(shadowCol);

        drawList->AddText(ImVec2(drawPos.x + 1, drawPos.y + 1), sCol, text.c_str());
        drawList->AddText(drawPos, col, text.c_str());
    }

    void DrawOutOfFOV(const SDK::FVector& targetLoc, SDK::APlayerController* LocalPC, const std::vector<OOFFlag>& flags, float alphaMult /*=1.0f*/) {
        if (targetLoc.X == 0 && targetLoc.Y == 0 && targetLoc.Z == 0) return;
        if (!LocalPC || !LocalPC->Pawn || !LocalPC->PlayerCameraManager) return;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 screenSize = io.DisplaySize;
        ImVec2 screenCenter = ImVec2(screenSize.x * 0.5f, screenSize.y * 0.5f);

        SDK::FVector playerLoc = LocalPC->Pawn->K2_GetActorLocation();
        SDK::FVector delta = targetLoc - playerLoc;

        const float PI = 3.1415926535f;
        SDK::FRotator cameraRot = LocalPC->PlayerCameraManager->GetCameraRotation();
        float yawRad = cameraRot.Yaw * (PI / 180.0f);

        float planarAngle = atan2f(delta.Y, delta.X) - yawRad;

        while (planarAngle > PI) planarAngle -= 2.0f * PI;
        while (planarAngle < -PI) planarAngle += 2.0f * PI;

        float radiusX = (screenSize.x * 0.45f) * g_Config::OOFRadius;
        float radiusY = (screenSize.y * 0.45f) * g_Config::OOFRadius;

        ImVec2 drawPos;
        drawPos.x = screenCenter.x + sinf(planarAngle) * radiusX;
        drawPos.y = screenCenter.y - cosf(planarAngle) * radiusY;

        static float breathTime = 0.0f;
        breathTime += ImGui::GetIO().DeltaTime;

        float distance3D = sqrtf(delta.X * delta.X + delta.Y * delta.Y + delta.Z * delta.Z);
        float maxDistance = 500.0f;
        float distanceRatio = 1.0f - std::clamp(distance3D / maxDistance, 0.0f, 1.0f);
        float dynamicSpeed = g_Config::OOFBreathSpeed * (1.0f + distanceRatio * 2.0f);

        float breathCycle = sinf(breathTime * dynamicSpeed) * 0.5f + 0.5f;
        float alphaValue = g_Config::OOFMinAlpha + (g_Config::OOFMaxAlpha - g_Config::OOFMinAlpha) * breathCycle;

        ImVec4 baseColor = *(ImVec4*)g_Config::OOFColor;
        baseColor.w = alphaValue * alphaMult;
        ImU32 triCol = ImGui::ColorConvertFloat4ToU32(baseColor);

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        float size = g_Config::OOFSize;

        float triAngle = planarAngle;
        ImVec2 p1 = ImVec2(drawPos.x + sinf(triAngle) * size, drawPos.y - cosf(triAngle) * size);
        ImVec2 p2 = ImVec2(drawPos.x + sinf(triAngle + 2.3f) * size, drawPos.y - cosf(triAngle + 2.3f) * size);
        ImVec2 p3 = ImVec2(drawPos.x + sinf(triAngle - 2.3f) * size, drawPos.y - cosf(triAngle - 2.3f) * size);

        drawList->AddTriangleFilled(p1, p2, p3, triCol);

        float textOffsetY = size + 5.0f;
        for (const auto& flag : flags) {
            ImVec2 textSize = ImGui::CalcTextSize(flag.text.c_str());
            float tx = std::clamp(drawPos.x - (textSize.x * 0.5f), 10.0f, screenSize.x - textSize.x - 10.0f);
            float ty = std::clamp(drawPos.y + textOffsetY, 10.0f, screenSize.y - textSize.y - 10.0f);

            ImVec4 fcol = ImGui::ColorConvertU32ToFloat4(flag.color);
            fcol.w *= alphaMult;
            ImU32 fcolU = ImGui::ColorConvertFloat4ToU32(fcol);

            drawList->AddText(ImVec2(tx + 1, ty + 1), ToImColor(0, 0, 0, 200 * alphaMult), flag.text.c_str());
            drawList->AddText(ImVec2(tx, ty), fcolU, flag.text.c_str());
            textOffsetY += textSize.y + 1.0f;
        }
    }

    BoxRect DrawBox(SDK::AActor* entity, float r, float g, float b, float a, float width_scale, bool bTestOnly) {
        BoxRect rect;
        if (!entity || entity->bHidden) return rect;
        auto PC = GetLocalPC();
        if (!PC) return rect;

        SDK::FVector origin, extent;
        entity->GetActorBounds(true, &origin, &extent, false);

        SDK::FVector worldTop = { origin.X, origin.Y, origin.Z + extent.Z };
        SDK::FVector worldBottom = { origin.X, origin.Y, origin.Z - extent.Z };
        SDK::FVector2D screenTop, screenBottom;

        if (PC->ProjectWorldLocationToScreen(worldTop, &screenTop, false) &&
            PC->ProjectWorldLocationToScreen(worldBottom, &screenBottom, false)) {

            float height = abs(screenBottom.Y - screenTop.Y);
            float width = height * width_scale;
            rect.topLeft = ImVec2(screenTop.X - width / 2.0f, screenTop.Y);
            rect.bottomRight = ImVec2(screenTop.X + width / 2.0f, screenBottom.Y);
            rect.valid = true;

            if (!bTestOnly && a > 0.1f) {
                ImDrawList* drawList = ImGui::GetBackgroundDrawList();
                drawList->AddRect(ImVec2(rect.topLeft.x - 1, rect.topLeft.y - 1),
                    ImVec2(rect.bottomRight.x + 1, rect.bottomRight.y + 1),
                    ToImColor(0, 0, 0, a), 0.0f, 0, 1.5f);
                drawList->AddRect(rect.topLeft, rect.bottomRight, ToImColor(r, g, b, a), 0.0f, 0, 1.0f);
            }
        }
        return rect;
    }

    void DrawHealthBar(BoxRect rect, float healthPercent, float maxHealth, float a) {
        if (!rect.valid || maxHealth <= 0) return;
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        float boxHeight = rect.bottomRight.y - rect.topLeft.y;
        float barWidth = 2.5f;
        float barMargin = 4.0f;
        ImVec2 barBgTop = ImVec2(rect.topLeft.x - barMargin - barWidth, rect.topLeft.y);
        ImVec2 barBgBottom = ImVec2(rect.topLeft.x - barMargin, rect.bottomRight.y);

        drawList->AddRectFilled(ImVec2(barBgTop.x - 1, barBgTop.y - 1), ImVec2(barBgBottom.x + 1, barBgBottom.y + 1), ToImColor(0, 0, 0, a * 0.7f));

        float percentage = healthPercent / maxHealth;
        percentage = std::clamp(percentage, 0.0f, 1.0f);

        ImVec4 col;
        if (percentage > 0.5f)
            col = ImVec4((1.0f - percentage) * 2.0f, 1.0f, 0.0f, a / 255.0f);
        else
            col = ImVec4(1.0f, percentage * 2.0f, 0.0f, a / 255.0f);

        ImU32 hpColor = ImGui::ColorConvertFloat4ToU32(col);

        if (healthPercent > 0) {
            float dynamicHeight = boxHeight * percentage;
            drawList->AddRectFilled(ImVec2(barBgTop.x, barBgBottom.y - dynamicHeight), barBgBottom, hpColor);
        }
    }

    void DrawName(SDK::AActor* entity, BoxRect rect, float r, float g, float b, float a) {
        if (!rect.valid || !entity) return;

        auto ShooterChar = static_cast<SDK::AShooterCharacter*>(entity);
        if (!ShooterChar) return;

        std::string nameStr;
        if (ShooterChar->PlayerState) {
            nameStr = ShooterChar->PlayerState->GetPlayerName().ToString();
        }
        else {
            auto Dino = static_cast<SDK::APrimalDinoCharacter*>(entity);
            if (Dino) nameStr = Dino->GetDescriptiveName().ToString();
        }

        if (nameStr.empty()) return;

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        ImVec2 textSize = ImGui::CalcTextSize(nameStr.c_str());
        ImVec2 textPos = ImVec2(rect.topLeft.x + (rect.bottomRight.x - rect.topLeft.x) / 2.0f - textSize.x / 2.0f, rect.topLeft.y - textSize.y - 5.0f);

        drawList->AddText(ImVec2(textPos.x + 1, textPos.y + 1), ToImColor(0, 0, 0, a), nameStr.c_str());
        drawList->AddText(textPos, ToImColor(r, g, b, a), nameStr.c_str());
    }
}
