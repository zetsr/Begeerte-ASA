#include "../Minimal-D3D12-Hook-ImGui-1.0.2/Main/mdx12_api.h"
#include "../CppSDK/SDK.hpp"
#include "ESP.h"
#include "Configs.h"
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

namespace g_ESP {
    SDK::APlayerController* GetLocalPC() {
        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (!World || !World->OwningGameInstance || World->OwningGameInstance->LocalPlayers.Num() == 0) return nullptr;
        return World->OwningGameInstance->LocalPlayers[0]->PlayerController;
    }

    inline ImU32 ToImColor(float r, float g, float b, float a) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
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

    void FlagManager::AddFlag(BoxRect rect, const std::string& text, ImU32 color, FlagPos pos) {
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

        // ÒõÓ° + ÎÄ±¾
        drawList->AddText(ImVec2(drawPos.x + 1, drawPos.y + 1), ToImColor(0, 0, 0, 255), text.c_str());
        drawList->AddText(drawPos, color, text.c_str());
    }

    void DrawOutOfFOV(SDK::AActor* entity, SDK::APlayerController* LocalPC, const std::vector<OOFFlag>& flags) {
        if (!entity || !LocalPC || !LocalPC->Pawn || !LocalPC->PlayerCameraManager) return;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 screenSize = io.DisplaySize;
        ImVec2 screenCenter = ImVec2(screenSize.x * 0.5f, screenSize.y * 0.5f);

        SDK::FVector playerLoc = LocalPC->Pawn->K2_GetActorLocation();
        SDK::FVector targetLoc = entity->K2_GetActorLocation();
        SDK::FRotator cameraRot = LocalPC->PlayerCameraManager->GetCameraRotation();

        SDK::FVector delta = targetLoc - playerLoc;
        float distance3D = sqrtf(delta.X * delta.X + delta.Y * delta.Y + delta.Z * delta.Z);

        const float PI = 3.1415926535f;
        float yawRad = cameraRot.Yaw * (PI / 180.0f);
        float pitchRad = cameraRot.Pitch * (PI / 180.0f);

        float tempX = delta.X * cosf(yawRad) + delta.Y * sinf(yawRad);
        float tempY = delta.Y * cosf(yawRad) - delta.X * sinf(yawRad);
        float tempZ = delta.Z;

        float forward = tempX * cosf(pitchRad) + tempZ * sinf(pitchRad);
        float right = tempY;
        float up = tempZ * cosf(pitchRad) - tempX * sinf(pitchRad);

        float relativeAngle = atan2f(right, forward);

        float radiusX = (screenSize.x * 0.45f) * g_Config::OOFRadius;
        float radiusY = (screenSize.y * 0.45f) * g_Config::OOFRadius;

        ImVec2 drawPos;
        drawPos.x = screenCenter.x + sinf(relativeAngle) * radiusX;
        drawPos.y = screenCenter.y - cosf(relativeAngle) * radiusY;

        static float breathTime = 0.0f;
        breathTime += ImGui::GetIO().DeltaTime;

        float maxDistance = 500.0f;
        float distanceRatio = 1.0f - std::clamp(distance3D / maxDistance, 0.0f, 1.0f);
        float dynamicSpeed = g_Config::OOFBreathSpeed * (1.0f + distanceRatio * 2.0f);

        float breathCycle = sinf(breathTime * dynamicSpeed) * 0.5f + 0.5f;
        float alphaValue = g_Config::OOFMinAlpha + (g_Config::OOFMaxAlpha - g_Config::OOFMinAlpha) * breathCycle;

        ImVec4 baseColor = *(ImVec4*)g_Config::OOFColor;
        baseColor.w = alphaValue;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(baseColor);

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        float size = g_Config::OOFSize;

        float triAngle = relativeAngle;
        ImVec2 p1 = ImVec2(drawPos.x + sinf(triAngle) * size, drawPos.y - cosf(triAngle) * size);
        ImVec2 p2 = ImVec2(drawPos.x + sinf(triAngle + 2.3f) * size, drawPos.y - cosf(triAngle + 2.3f) * size);
        ImVec2 p3 = ImVec2(drawPos.x + sinf(triAngle - 2.3f) * size, drawPos.y - cosf(triAngle - 2.3f) * size);

        drawList->AddTriangleFilled(p1, p2, p3, col);

        float textOffsetY = size + 5.0f;
        for (const auto& flag : flags) {
            ImVec2 textSize = ImGui::CalcTextSize(flag.text.c_str());
            float tx = std::clamp(drawPos.x - (textSize.x * 0.5f), 10.0f, screenSize.x - textSize.x - 10.0f);
            float ty = std::clamp(drawPos.y + textOffsetY, 10.0f, screenSize.y - textSize.y - 10.0f);

            drawList->AddText(ImVec2(tx + 1, ty + 1), ToImColor(0, 0, 0, 200), flag.text.c_str());
            drawList->AddText(ImVec2(tx, ty), flag.color, flag.text.c_str());
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