// ESP.h
#pragma once
#include "../../external/CppSDK/SDK.hpp"
#include "../../external/Shadow-Gui/src/Shadow.h";

namespace ESP {
    struct BoxRect {
        Shadow::Vec2 topLeft;
        Shadow::Vec2 bottomRight;
        bool valid = false;
    };

    enum class FlagPos {
        Left,
        Right,
        Top,
        Bottom
    };

    struct FlagManager {
        float leftY = 0.0f;
        float rightY = 0.0f;
        float topY = 0.0f;
        float bottomY = 0.0f;

        inline void Reset() {
            leftY = 0.0f;
            rightY = 0.0f;
            topY = 0.0f;
            bottomY = 0.0f;
        }

        inline void AddFlag(SDK::UCanvas* Canvas, BoxRect rect, std::string_view text, Shadow::Color color, FlagPos pos, float alphaMult) {
            if (!rect.valid || text.empty() || !Canvas) return;

            // Shadow::PushFont(Shadow::DefaultFont, g_Config::fESPFontSize);
            Shadow::Vec2 textSize = Shadow::MeasureTextSize(text);
            // Shadow::PopFont();

            Shadow::Vec2 drawPos = { 0.f, 0.f };

            if (pos == FlagPos::Right) {
                drawPos = { (float)(rect.bottomRight.x + 5.0f), (float)(rect.topLeft.y + rightY) };
                rightY += textSize.y + 1.0f;
            }
            else if (pos == FlagPos::Left) {
                drawPos = { (float)(rect.topLeft.x - 6.0f - textSize.x), (float)(rect.topLeft.y + leftY) };
                leftY += textSize.y + 1.0f;
            }
            else if (pos == FlagPos::Top) {
                float centerX = (rect.topLeft.x + rect.bottomRight.x) * 0.5f;
                drawPos = { (float)(centerX - textSize.x * 0.5f), (float)(rect.topLeft.y - topY - textSize.y - 3.0f) };
                topY += textSize.y + 1.0f;
            }
            else if (pos == FlagPos::Bottom) {
                float centerX = (rect.topLeft.x + rect.bottomRight.x) * 0.5f;
                drawPos = { (float)(centerX - textSize.x * 0.5f), (float)(rect.bottomRight.y + bottomY + 3.0f) };
                bottomY += textSize.y + 1.0f;
            }

            // 防止像素绘制在非整数坐标上（浮点错位），确保drawPos必定是整数
            drawPos.x = std::round(drawPos.x);
            drawPos.y = std::round(drawPos.y);

            color.a *= alphaMult;
            Shadow::Color shadowCol = { 0.0f, 0.0f, 0.0f, color.a };

            Shadow::GetBackgroundDrawList()->AddText(Shadow::DefaultFont, 1.f /*g_Config::fESPFontSize*/, shadowCol, shadowCol, drawPos, color, text, true);
        }

        inline void AddTexture(SDK::UCanvas* Canvas, BoxRect rect, SDK::UTexture* texture, Shadow::Color color, FlagPos pos, float alphaMult) {
            if (!rect.valid || !texture || !Canvas) return;

            // 基础大小乘以 g_Config::fESPFontSize 缩放比例
            float texSize = 32.0f * 1.f /*g_Config::fESPFontSize*/;
            Shadow::Vec2 drawPos = { 0.f, 0.f };

            if (pos == FlagPos::Right) {
                drawPos = { (float)(rect.bottomRight.x + 5.0f), (float)(rect.topLeft.y + rightY) };
                rightY += texSize + 1.0f;
            }
            else if (pos == FlagPos::Left) {
                drawPos = { (float)(rect.topLeft.x - 6.0f - texSize), (float)(rect.topLeft.y + leftY) };
                leftY += texSize + 1.0f;
            }
            else if (pos == FlagPos::Top) {
                float centerX = (rect.topLeft.x + rect.bottomRight.x) * 0.5f;
                drawPos = { (float)(centerX - texSize * 0.5f), (float)(rect.topLeft.y - topY - texSize - 3.0f) };
                topY += texSize + 1.0f;
            }
            else if (pos == FlagPos::Bottom) {
                float centerX = (rect.topLeft.x + rect.bottomRight.x) * 0.5f;
                drawPos = { (float)(centerX - texSize * 0.5f), (float)(rect.bottomRight.y + bottomY + 3.0f) };
                bottomY += texSize + 1.0f;
            }

            // 防止像素绘制在非整数坐标上（浮点错位），确保drawPos必定是整数
            drawPos.x = std::round(drawPos.x);
            drawPos.y = std::round(drawPos.y);

            color.a *= alphaMult;
            Shadow::GetBackgroundDrawList()->AddTexture(Shadow::Vec2(drawPos.x, drawPos.y), Shadow::Vec2(texSize, texSize), color, texture);
        }
    };

    inline BoxRect GetBox(SDK::UCanvas* Canvas, SDK::AActor* Actor, float width_scale) {
        BoxRect rect;
        rect.valid = false;

        if (!Actor || Actor->bHidden || !Canvas) {
            return rect;
        }

        SDK::FVector origin, extent;
        Actor->GetActorBounds(true, &origin, &extent, false);

        SDK::FVector worldTop = { origin.X, origin.Y, origin.Z + extent.Z };
        SDK::FVector worldBottom = { origin.X, origin.Y, origin.Z - extent.Z };

        SDK::FVector screenTopVec = Canvas->K2_Project(worldTop);
        SDK::FVector screenBottomVec = Canvas->K2_Project(worldBottom);

        if (screenTopVec.Z > 0.0f && screenBottomVec.Z > 0.0f) {
            SDK::FVector2D screenTop = { screenTopVec.X, screenTopVec.Y };
            SDK::FVector2D screenBottom = { screenBottomVec.X, screenBottomVec.Y };

            float height = std::abs(screenBottom.Y - screenTop.Y);
            float width = height * width_scale;
            rect.topLeft = { (float)(screenTop.X - width * 0.5f), (float)screenTop.Y };
            rect.bottomRight = { (float)(screenTop.X + width * 0.5f), (float)screenBottom.Y };
            rect.valid = true;
        }

        return rect;
    }

	inline void Run(SDK::UCanvas* Canvas) {
        SDK::APlayerController* LocalPC = MEM::GetLocalPC();
        if (!LocalPC) return;

        SDK::APawn* LocalPawn = LocalPC->Pawn;
        if (!LocalPawn) return;

        static std::vector<SDK::AActor*> Actors;
        Actors.clear();

        MEM::GetAllActorsOfClass(SDK::UWorld::GetWorld(), SDK::AActor::StaticClass(), Actors);
        for (SDK::AActor* Actor : Actors) {
            if (!Actor) continue;

            float distance = LocalPawn->GetDistanceTo(Actor) / 100.f;
            if (distance > 100.f) continue;

			ESP::BoxRect rect = ESP::GetBox(Canvas, Actor, 0.5f);
			ESP::FlagManager f;

            f.AddFlag(Canvas, rect, std::format("{}", Actor->GetName()), { 1.f, 1.f, 1.f, 1.f }, ESP::FlagPos::Right, 1.f);
			f.AddFlag(Canvas, rect, std::format("{:.1f}m", distance), { 1.f, 1.f, 1.f, 1.f }, ESP::FlagPos::Right, 1.f);
        }
	}
}