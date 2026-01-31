#pragma once
#include <vector>
#include <string>

struct ImVec2;
using ImU32 = unsigned int;

namespace g_ESP {
    SDK::APlayerController* GetLocalPC();

    std::string ToLower(std::string s);

    bool IsEntityMatch(std::string displayName, std::string filter);

    struct BoxRect {
        ImVec2 topLeft;
        ImVec2 bottomRight;
        bool valid = false;
    };

    enum class FlagPos {
        Left,
        Right,
        Top,
        Bottom
    };

    enum class BarPos {
        Left,
        Right,
        Top,
        Bottom
    };

    enum class BarOrientation {
        Vertical,
        Horizontal
    };

    class BarManager {
    private:
        float leftOffset = 0.0f;
        float rightOffset = 0.0f;
        float topOffset = 0.0f;
        float bottomOffset = 0.0f;

    public:
        void Reset() {
            leftOffset = 0.0f;
            rightOffset = 0.0f;
            topOffset = 0.0f;
            bottomOffset = 0.0f;
        }

        float GetLeftOffset() const { return leftOffset; }
        float GetRightOffset() const { return rightOffset; }
        float GetTopOffset() const { return topOffset; }
        float GetBottomOffset() const { return bottomOffset; }

        void AddBar(BoxRect rect, float currentValue, float maxValue, ImU32 color, BarPos pos, BarOrientation orientation, float a);
    };

    class FlagManager {
    private:
        float leftY = 0.0f;
        float rightY = 0.0f;
        float topY = 0.0f;
        float bottomY = 0.0f;

    public:
        void Reset() {
            leftY = 0.0f;
            rightY = 0.0f;
            topY = 0.0f;
            bottomY = 0.0f;
        }

        void AddFlag(BoxRect rect, const std::string& text, ImU32 color, FlagPos pos, float alphaMult, const BarManager* barMgr);
    };

    BoxRect DrawBox(SDK::AActor* entity, float r, float g, float b, float a, float width_scale, bool bTestOnly = false);
    void DrawHealthBar(BoxRect rect, float healthPercent, float maxHealth, float a);
    void DrawName(SDK::AActor* entity, BoxRect rect, float r, float g, float b, float a);

    struct OOFFlag {
        std::string text;
        ImU32 color;
    };

    void DrawOutOfFOV(const SDK::FVector& targetLoc, SDK::APlayerController* LocalPC, const std::vector<OOFFlag>& flags, float alphaMult = 1.0f);

    enum class RelationType {
        Enemy,
        Team,
    };

    RelationType GetRelation(SDK::APrimalCharacter* TargetChar, SDK::APrimalCharacter* LocalChar);
}