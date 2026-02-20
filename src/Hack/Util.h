#pragma once
#include "../Minimal-D3D12-Hook-ImGui/Main/mdx12_api.h"
#include "SDK_Headers.hpp"
#include <cmath>
#include <algorithm>
#include <string>

namespace g_Util {
    inline std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return std::tolower(c);
            });
        return s;
    }

    inline ImU32 GetU32Color(float color[4]) {
        return ImGui::ColorConvertFloat4ToU32(*(ImVec4*)color);
    }

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

    inline bool IsEntityMatch(std::string displayName, std::string filter) {
        if (filter.empty()) return true;

        std::string nameLower = g_Util::ToLower(displayName);
        std::string filterLower = g_Util::ToLower(filter);

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

    inline bool IsStructureMatch(const std::string& structureName, const std::string& filter) {
        if (filter.empty()) return true;

        std::string lowerName = structureName;
        std::string lowerFilter = filter;

        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

        return lowerName.find(lowerFilter) != std::string::npos;
    }

    inline SDK::APlayerController* GetLocalPC() {
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
}