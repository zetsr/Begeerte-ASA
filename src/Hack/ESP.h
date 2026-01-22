#pragma once
#include <vector>
#include <string>

namespace g_ESP {
    SDK::APlayerController* GetLocalPC();

    struct BoxRect {
        ImVec2 topLeft;
        ImVec2 bottomRight;
        bool valid = false;
    };

    enum class FlagPos {
        Left,
        Right
    };

    class FlagManager {
    private:
        float leftY = 0.0f;
        float rightY = 0.0f;

    public:
        void Reset() {
            leftY = 0.0f;
            rightY = 0.0f;
        }

        void AddFlag(BoxRect rect, const std::string& text, ImU32 color, FlagPos pos);
    };

    BoxRect DrawBox(SDK::AActor* entity, float r, float g, float b, float a, float width_scale, bool bTestOnly = false);
    void DrawHealthBar(BoxRect rect, float healthPercent, float maxHealth, float a);
    void DrawName(SDK::AActor* entity, BoxRect rect, float r, float g, float b, float a);

    struct OOFFlag {
        std::string text;
        ImU32 color;
    };

    void DrawOutOfFOV(SDK::AActor* entity, SDK::APlayerController* LocalPC, const std::vector<OOFFlag>& flags);

    enum class RelationType {
        Enemy,
        Team,
    };

    RelationType GetPlayerRelation(SDK::APlayerState* targetPS, SDK::APlayerState* localPS);
}