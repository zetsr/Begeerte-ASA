#pragma once

#include "CoreMinimal.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "CanvasItem.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "HAL/PlatformFileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "UnrealClient.h"

// 启用 Shadow 的 UE 原生适配分支
#ifndef SHADOW_USE_UE_NATIVE
#define SHADOW_USE_UE_NATIVE
#endif

#include "Shadow_Texture.h"
#include "Shadow.h"
#include "Shadow_Demo.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <deque>

namespace Shadow {
    // 常量配置
    constexpr float NATIVE_MATH_PI = 3.14159265358979323846f;
    constexpr float NATIVE_RAD_TO_DEG = 180.0f / NATIVE_MATH_PI;
    constexpr float NATIVE_TRIANGLE_LINE_EPSILON = 0.5f;
    constexpr float NATIVE_TRIANGLE_INTERPOLATE_EPSILON = 0.001f;
    constexpr int32 NATIVE_MIN_PRINTABLE_CHAR = 32;
    constexpr int32 NATIVE_MAX_PRINTABLE_CHAR = 126;

    // 后端全局状态与缓存池
    inline UCanvas* s_NativeCurrentCanvas = nullptr;
    inline double s_NativeRealTimeSeconds = 0.0;
    inline double s_NativeDeltaTime = 0.0;

    inline TWeakObjectPtr<UWorld> s_NativeCachedWorld = nullptr;
    inline std::unordered_map<const void*, TWeakObjectPtr<UTexture2D>> s_NativeBufferTextureCache;
    inline std::unordered_map<std::wstring, TWeakObjectPtr<UTexture2D>> s_NativeFileTextureCache;

    inline std::unordered_set<std::wstring> s_NativePermanentPathPool;
    inline std::unordered_map<std::wstring, TWeakObjectPtr<UFont>> s_NativeFileFontCache;
    inline std::unordered_map<const void*, TWeakObjectPtr<UFont>> s_NativeBufferFontCache;
    inline std::deque<FTypefaceEntry> s_NativePermanentTypefaceEntries;
    inline std::unordered_map<UFont*, int32_t> s_NativeFontOriginalSizes;

    constexpr float NATIVE_DEFAULT_GAMMA = 1.0f;
    constexpr float NATIVE_MIN_GAMMA_DIFF = 0.0001f;

    // 基础辅助函数
    inline UWorld* Native_GetReadyWorld() {
        if (!s_NativeCurrentCanvas) {
            return nullptr;
        }

        UWorld* CurrentWorld = s_NativeCurrentCanvas->GetWorld();
        if (!CurrentWorld) {
            if (GEngine) {
                CurrentWorld = GEngine->GetCurrentPlayWorld();
            }
        }

        if (CurrentWorld && CurrentWorld != s_NativeCachedWorld.Get()) {
            s_NativeCachedWorld = CurrentWorld;
            s_NativeBufferTextureCache.clear();
            s_NativeFileTextureCache.clear();
        }

        return CurrentWorld;
    }

    inline const wchar_t* Native_GetPermanentPathPtr(const std::wstring& Path) {
        auto Pair = s_NativePermanentPathPool.insert(Path);
        return Pair.first->c_str();
    }

    inline UFont* Native_CreatePermanentRuntimeFont() {
        UClass* FontClass = UFont::StaticClass();
        if (!FontClass) {
            return nullptr;
        }

        UObject* Outer = GetTransientPackage();
        if (!Outer) {
            return nullptr;
        }

        UFont* NewFont = NewObject<UFont>(Outer, FontClass, NAME_None, RF_Transient | RF_Public);
        if (NewFont) {
            NewFont->AddToRoot();
        }
        return NewFont;
    }

    struct ScopedNativeFontScale {
        UFont* Font = nullptr;
        int32_t OriginalSize = 0;
        bool NoSDF = false;
        float CalculatedScale = 1.0f;

        ScopedNativeFontScale(UFont* InFont, float InFontScale = 1.0f, bool InNoSDF = false) {
            if (!InFont) return;
            Font = InFont;
            NoSDF = InNoSDF;

            auto it = s_NativeFontOriginalSizes.find(Font);
            if (it == s_NativeFontOriginalSizes.end()) {
                OriginalSize = Font->LegacyFontSize;
                s_NativeFontOriginalSizes[Font] = OriginalSize;
            }
            else {
                OriginalSize = it->second;
            }

            const auto& Style = Shadow::GetStyle();
            int32_t BaseSize = OriginalSize > 0 ? OriginalSize : static_cast<int32_t>(Style.FallbackFontSize);
            float FinalScale = InFontScale * Style.FontScaleDpi;

            if (NoSDF) {
                if (Font->LegacyFontSize != static_cast<int32_t>(Style.NoSDFFontBaseSize)) {
                    Font->LegacyFontSize = static_cast<int32_t>(Style.NoSDFFontBaseSize);
                }
                float DesiredSize = std::clamp(static_cast<float>(BaseSize) * FinalScale, Style.FontSizeMin, Style.FontSizeMax);
                CalculatedScale = DesiredSize / Style.NoSDFFontBaseSize;
            }
            else {
                int32_t TargetSize = std::clamp(static_cast<int32_t>(BaseSize * FinalScale), static_cast<int32_t>(Style.FontSizeMin), static_cast<int32_t>(Style.FontSizeMax));
                Font->LegacyFontSize = TargetSize;
                CalculatedScale = 1.0f;
            }
        }

        ~ScopedNativeFontScale() {
            if (Font && !NoSDF) {
                Font->LegacyFontSize = OriginalSize;
            }
        }
    };

    // 资源加载回调实现
    inline ShadowTexture Native_LoadTextureFromBuffer(const unsigned char* BufferData, size_t BufferSize) {
        if (!BufferData || BufferSize == 0) {
            return ShadowTexture{};
        }

        UWorld* World = Native_GetReadyWorld();
        if (!World) {
            return ShadowTexture{};
        }

        auto It = s_NativeBufferTextureCache.find(BufferData);
        if (It != s_NativeBufferTextureCache.end() && It->second.IsValid()) {
            return ShadowTexture{ It->second.Get() };
        }

        TArray<uint8> RawBuffer;
        RawBuffer.Append(BufferData, static_cast<int32>(BufferSize));

        UTexture2D* LoadedTexture = UKismetRenderingLibrary::ImportBufferAsTexture2D(World, RawBuffer);
        if (LoadedTexture) {
            LoadedTexture->AddToRoot();
            LoadedTexture->NeverStream = true;
            LoadedTexture->LODGroup = TEXTUREGROUP_UI;
            LoadedTexture->UpdateResource();

            s_NativeBufferTextureCache[BufferData] = LoadedTexture;
        }

        return ShadowTexture{ LoadedTexture };
    }

    inline ShadowTexture Native_LoadTextureFromFile(const wchar_t* FilePath) {
        if (!FilePath) {
            return ShadowTexture{};
        }

        UWorld* World = Native_GetReadyWorld();
        if (!World) {
            return ShadowTexture{};
        }

        auto It = s_NativeFileTextureCache.find(FilePath);
        if (It != s_NativeFileTextureCache.end() && It->second.IsValid()) {
            return ShadowTexture{ It->second.Get() };
        }

        FString PathStr(FilePath);
        UTexture2D* LoadedTexture = UKismetRenderingLibrary::ImportFileAsTexture2D(World, PathStr);
        if (LoadedTexture) {
            LoadedTexture->AddToRoot();
            LoadedTexture->NeverStream = true;
            LoadedTexture->LODGroup = TEXTUREGROUP_UI;
            LoadedTexture->UpdateResource();

            s_NativeFileTextureCache[FilePath] = LoadedTexture;
        }

        return ShadowTexture{ LoadedTexture };
    }

    inline ShadowFont Native_LoadFontFromFile(const wchar_t* FilePath, int32_t FontSize = DEFAULT_FONT_SIZE) {
        if (!FilePath) {
            return ShadowFont{};
        }

        auto It = s_NativeFileFontCache.find(FilePath);
        if (It != s_NativeFileFontCache.end() && It->second.IsValid()) {
            return ShadowFont{ It->second.Get() };
        }

        UFont* NewFont = Native_CreatePermanentRuntimeFont();
        if (!NewFont) {
            return ShadowFont{};
        }

        NewFont->FontCacheType = EFontCacheType::Runtime;
        if (FontSize >= MIN_VALID_FONT_SIZE) {
            NewFont->LegacyFontSize = FontSize;
        }

        const wchar_t* SafePathPtr = Native_GetPermanentPathPtr(FilePath);

        FTypefaceEntry& Entry = s_NativePermanentTypefaceEntries.emplace_back();
        Entry.Name = NAME_None;
        Entry.Font = FFontData(FString(SafePathPtr), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);

        NewFont->CompositeFont.DefaultTypeface.Fonts.Add(Entry);

        s_NativeFileFontCache[FilePath] = NewFont;
        return ShadowFont{ NewFont };
    }

    inline ShadowFont Native_LoadFontFromBuffer(const unsigned char* BufferData, size_t BufferSize, int32_t FontSize = DEFAULT_FONT_SIZE) {
        if (!BufferData || BufferSize == 0) {
            return ShadowFont{};
        }

        auto It = s_NativeBufferFontCache.find(BufferData);
        if (It != s_NativeBufferFontCache.end() && It->second.IsValid()) {
            return ShadowFont{ It->second.Get() };
        }

        FString TempDir = FPaths::ProjectSavedDir() / TEXT("TempFonts");
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (!PlatformFile.DirectoryExists(*TempDir)) {
            PlatformFile.CreateDirectoryTree(*TempDir);
        }

        FString TempFilePath = TempDir / FString::Printf(TEXT("native_font_%p.ttf"), BufferData);

        if (!PlatformFile.FileExists(*TempFilePath)) {
            TArray<uint8> FontData;
            FontData.Append(BufferData, static_cast<int32>(BufferSize));
            FFileHelper::SaveArrayToFile(FontData, *TempFilePath);
        }

        ShadowFont LoadedFont = Native_LoadFontFromFile(*TempFilePath, FontSize);
        if (LoadedFont) {
            s_NativeBufferFontCache[BufferData] = static_cast<UFont*>(LoadedFont.NativePtr);
        }

        return LoadedFont;
    }

    inline Vec2 Native_MeasureText(ShadowFont shadowFont, float fontScale, bool noSDF, std::string_view text) {
        UFont* Font = static_cast<UFont*>(shadowFont.NativePtr);
        if (!s_NativeCurrentCanvas || !Font || text.empty()) {
            return { 0.f, 0.f };
        }

        ScopedNativeFontScale FontGuard(Font, fontScale, noSDF);
        FVector2D Scale{ FontGuard.CalculatedScale, FontGuard.CalculatedScale };

        FUTF8ToTCHAR Conv(text.data(), static_cast<int32>(text.size()));
        FString Str(Conv.Length(), Conv.Get());
        FVector2D OutSize = s_NativeCurrentCanvas->K2_TextSize(Font, Str, Scale);
        return { static_cast<float>(OutSize.X), static_cast<float>(OutSize.Y) };
    }

    inline UTexture2D* Native_GetCircleTexture(float radius) {
        if (radius <= 0.0f) return nullptr;
        float diameter = radius * 2.0f;
        if (diameter <= 16.0f) {
            return static_cast<UTexture2D*>(Native_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_16, sizeof(Shadow_Texture::CircleFilled_16)).NativePtr);
        }
        else if (diameter <= 32.0f) {
            return static_cast<UTexture2D*>(Native_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_32, sizeof(Shadow_Texture::CircleFilled_32)).NativePtr);
        }
        else if (diameter <= 64.0f) {
            return static_cast<UTexture2D*>(Native_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_64, sizeof(Shadow_Texture::CircleFilled_64)).NativePtr);
        }
        else if (diameter <= 128.0f) {
            return static_cast<UTexture2D*>(Native_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_128, sizeof(Shadow_Texture::CircleFilled_128)).NativePtr);
        }
        else if (diameter <= 256.0f) {
            return static_cast<UTexture2D*>(Native_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_256, sizeof(Shadow_Texture::CircleFilled_256)).NativePtr);
        }
        else if (diameter <= 512.0f) {
            return static_cast<UTexture2D*>(Native_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_512, sizeof(Shadow_Texture::CircleFilled_512)).NativePtr);
        }
        else {
            return static_cast<UTexture2D*>(Native_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_1024, sizeof(Shadow_Texture::CircleFilled_1024)).NativePtr);
        }
    }

    // 渲染图元底层实现
    inline void Native_DrawLine(Vec2 start, Vec2 end, Color color, float thickness, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, UTexture* texture = nullptr) {
        if (!s_NativeCurrentCanvas) return;
        UTexture* Tex = texture ? texture : s_NativeCurrentCanvas->DefaultTexture;
        if (!Tex) return;

        if (clipEnabled) {
            float dx = end.x - start.x;
            float dy = end.y - start.y;
            float t0 = 0.0f, t1 = 1.0f;

            auto ClipTest = [](float p, float q, float& InT0, float& InT1) -> bool {
                if (p == 0.0f) {
                    return q >= 0.0f;
                }
                float r = q / p;
                if (p < 0.0f) {
                    if (r > InT1) return false;
                    if (r > InT0) InT0 = r;
                }
                else {
                    if (r < InT0) return false;
                    if (r < InT1) InT1 = r;
                }
                return true;
                };

            if (!ClipTest(-dx, start.x - clipMin.x, t0, t1)) return;
            if (!ClipTest(dx, clipMax.x - start.x, t0, t1)) return;
            if (!ClipTest(-dy, start.y - clipMin.y, t0, t1)) return;
            if (!ClipTest(dy, clipMax.y - start.y, t0, t1)) return;

            if (t1 < 1.0f) {
                end.x = start.x + t1 * dx;
                end.y = start.y + t1 * dy;
            }
            if (t0 > 0.0f) {
                start.x += t0 * dx;
                start.y += t0 * dy;
            }
        }

        float dx = end.x - start.x;
        float dy = end.y - start.y;
        float Length = std::sqrt(dx * dx + dy * dy);
        if (Length <= 0.0f) return;

        float Angle = std::atan2(dy, dx) * NATIVE_RAD_TO_DEG;

        FVector2D UEPos{ static_cast<float>(start.x), static_cast<float>(start.y) };
        FVector2D UESize{ static_cast<float>(Length), static_cast<float>(thickness) };
        FLinearColor UEColor{ color.r, color.g, color.b, color.a };

        s_NativeCurrentCanvas->K2_DrawTexture(
            Tex,
            UEPos, UESize,
            FVector2D{ 0.0f, 0.0f }, FVector2D{ 1.0f, 1.0f },
            UEColor, BLEND_Translucent,
            Angle, FVector2D{ 0.0f, 0.5f }
        );
    }

    inline void Native_DrawRect(Vec2 pos, Vec2 size, Color color, float thickness, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, float rounding, ShadowDrawFlags flags, UTexture* texture = nullptr) {
        if (!s_NativeCurrentCanvas || size.x <= 0.f || size.y <= 0.f) return;

        if (clipEnabled) {
            if (pos.x + size.x < clipMin.x || pos.x > clipMax.x ||
                pos.y + size.y < clipMin.y || pos.y > clipMax.y) {
                return;
            }
        }

        bool roundTL = (flags & ShadowDrawFlags_RoundCornersTopLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
        bool roundTR = (flags & ShadowDrawFlags_RoundCornersTopRight) && !(flags & ShadowDrawFlags_RoundCornersNone);
        bool roundBL = (flags & ShadowDrawFlags_RoundCornersBottomLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
        bool roundBR = (flags & ShadowDrawFlags_RoundCornersBottomRight) && !(flags & ShadowDrawFlags_RoundCornersNone);

        if (rounding <= 0.0f || (!roundTL && !roundTR && !roundBL && !roundBR)) {
            float halfThick = thickness * 0.5f;
            Native_DrawLine({ pos.x - halfThick, pos.y }, { pos.x + size.x + halfThick, pos.y }, color, thickness, clipEnabled, clipMin, clipMax, texture);
            Native_DrawLine({ pos.x - halfThick, pos.y + size.y }, { pos.x + size.x + halfThick, pos.y + size.y }, color, thickness, clipEnabled, clipMin, clipMax, texture);
            Native_DrawLine({ pos.x, pos.y + halfThick }, { pos.x, pos.y + size.y - halfThick }, color, thickness, clipEnabled, clipMin, clipMax, texture);
            Native_DrawLine({ pos.x + size.x, pos.y + halfThick }, { pos.x + size.x, pos.y + size.y - halfThick }, color, thickness, clipEnabled, clipMin, clipMax, texture);
            return;
        }

        float r = std::min(rounding, std::min(size.x, size.y) * 0.5f);
        float r_tl = roundTL ? r : 0.0f;
        float r_tr = roundTR ? r : 0.0f;
        float r_bl = roundBL ? r : 0.0f;
        float r_br = roundBR ? r : 0.0f;

        Native_DrawLine({ pos.x + r_tl, pos.y }, { pos.x + size.x - r_tr, pos.y }, color, thickness, clipEnabled, clipMin, clipMax, texture);
        Native_DrawLine({ pos.x + r_bl, pos.y + size.y }, { pos.x + size.x - r_br, pos.y + size.y }, color, thickness, clipEnabled, clipMin, clipMax, texture);
        Native_DrawLine({ pos.x, pos.y + r_tl }, { pos.x, pos.y + size.y - r_bl }, color, thickness, clipEnabled, clipMin, clipMax, texture);
        Native_DrawLine({ pos.x + size.x, pos.y + r_tr }, { pos.x + size.x, pos.y + size.y - r_br }, color, thickness, clipEnabled, clipMin, clipMax, texture);

        int segments = RECT_ROUNDING_SEGMENTS;
        auto DrawCorner = [&](Vec2 center, float radius, float startAngle, float endAngle) {
            if (radius <= 0.f) return;
            for (int i = 0; i < segments; ++i) {
                float t0 = (float)i / segments;
                float t1 = (float)(i + 1) / segments;
                float a0 = startAngle + t0 * (endAngle - startAngle);
                float a1 = startAngle + t1 * (endAngle - startAngle);
                Native_DrawLine(
                    { center.x + radius * std::cos(a0), center.y - radius * std::sin(a0) },
                    { center.x + radius * std::cos(a1), center.y - radius * std::sin(a1) },
                    color, thickness, clipEnabled, clipMin, clipMax, texture
                );
            }
            };

        if (roundTL) DrawCorner({ pos.x + r, pos.y + r }, r, NATIVE_MATH_PI / 2.0f, NATIVE_MATH_PI);
        if (roundTR) DrawCorner({ pos.x + size.x - r, pos.y + r }, r, 0.0f, NATIVE_MATH_PI / 2.0f);
        if (roundBL) DrawCorner({ pos.x + r, pos.y + size.y - r }, r, NATIVE_MATH_PI, 3.0f * NATIVE_MATH_PI / 2.0f);
        if (roundBR) DrawCorner({ pos.x + size.x - r, pos.y + size.y - r }, r, 3.0f * NATIVE_MATH_PI / 2.0f, 2.0f * NATIVE_MATH_PI);
    }

    inline void Native_DrawRectFilled(Vec2 pos, Vec2 size, Color color, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, float rounding, ShadowDrawFlags flags, UTexture* texture = nullptr) {
        if (!s_NativeCurrentCanvas) return;

        bool roundTL = (flags & ShadowDrawFlags_RoundCornersTopLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
        bool roundTR = (flags & ShadowDrawFlags_RoundCornersTopRight) && !(flags & ShadowDrawFlags_RoundCornersNone);
        bool roundBL = (flags & ShadowDrawFlags_RoundCornersBottomLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
        bool roundBR = (flags & ShadowDrawFlags_RoundCornersBottomRight) && !(flags & ShadowDrawFlags_RoundCornersNone);

        if (rounding > 0.0f && texture == nullptr && (roundTL || roundTR || roundBL || roundBR)) {
            float r = std::min(rounding, std::min(size.x, size.y) * 0.5f);
            UTexture2D* circleTex = Native_GetCircleTexture(r);
            if (circleTex && IsValid(circleTex)) {
                float w = size.x;
                float h = size.y;

                UTexture* defTex = s_NativeCurrentCanvas->DefaultTexture;
                if (!defTex) return;

                auto DrawPiece = [&](float x, float y, float piece_w, float piece_h, float u0, float v0, float u1, float v1, UTexture* tex) {
                    if (!tex || piece_w <= 0.f || piece_h <= 0.f) return;
                    Vec2 drawPos = { pos.x + x, pos.y + y };
                    Vec2 drawSize = { piece_w, piece_h };
                    Vec2 drawUV0 = { u0, v0 };
                    Vec2 drawUV1 = { u1, v1 };

                    if (clipEnabled) {
                        if (drawPos.x + drawSize.x <= clipMin.x || drawPos.x >= clipMax.x ||
                            drawPos.y + drawSize.y <= clipMin.y || drawPos.y >= clipMax.y) return;

                        if (drawPos.x < clipMin.x) {
                            float cut = clipMin.x - drawPos.x;
                            float cutUV = cut / drawSize.x * (drawUV1.x - drawUV0.x);
                            drawUV0.x += cutUV;
                            drawSize.x -= cut;
                            drawPos.x = clipMin.x;
                        }
                        if (drawPos.y < clipMin.y) {
                            float cut = clipMin.y - drawPos.y;
                            float cutUV = cut / drawSize.y * (drawUV1.y - drawUV0.y);
                            drawUV0.y += cutUV;
                            drawSize.y -= cut;
                            drawPos.y = clipMin.y;
                        }
                        if (drawPos.x + drawSize.x > clipMax.x) {
                            float cut = drawPos.x + drawSize.x - clipMax.x;
                            float cutUV = cut / drawSize.x * (drawUV1.x - drawUV0.x);
                            drawUV1.x -= cutUV;
                            drawSize.x = clipMax.x - drawPos.x;
                        }
                        if (drawPos.y + drawSize.y > clipMax.y) {
                            float cut = drawPos.y + drawSize.y - clipMax.y;
                            float cutUV = cut / drawSize.y * (drawUV1.y - drawUV0.y);
                            drawUV1.y -= cutUV;
                            drawSize.y = clipMax.y - drawPos.y;
                        }
                        if (drawSize.x <= 0.f || drawSize.y <= 0.f) return;
                    }

                    FLinearColor UEColor{ color.r, color.g, color.b, color.a };
                    FVector2D UEPos{ static_cast<float>(drawPos.x), static_cast<float>(drawPos.y) };
                    FVector2D UESize{ static_cast<float>(drawSize.x), static_cast<float>(drawSize.y) };
                    FVector2D UEUV0{ static_cast<float>(drawUV0.x), static_cast<float>(drawUV0.y) };
                    FVector2D UEUVSize{ static_cast<float>(drawUV1.x - drawUV0.x), static_cast<float>(drawUV1.y - drawUV0.y) };

                    s_NativeCurrentCanvas->K2_DrawTexture(
                        tex, UEPos, UESize,
                        UEUV0, UEUVSize,
                        UEColor, BLEND_Translucent, 0.0f, FVector2D{ 0.0f, 0.0f }
                    );
                    };

                // 1. 左上角
                if (roundTL) DrawPiece(0, 0, r, r, 0.0f, 0.0f, 0.5f, 0.5f, circleTex);
                else DrawPiece(0, 0, r, r, 0.0f, 0.0f, 1.0f, 1.0f, defTex);

                // 2. 右上角
                if (roundTR) DrawPiece(w - r, 0, r, r, 0.5f, 0.0f, 1.0f, 0.5f, circleTex);
                else DrawPiece(w - r, 0, r, r, 0.0f, 0.0f, 1.0f, 1.0f, defTex);

                // 3. 左下角
                if (roundBL) DrawPiece(0, h - r, r, r, 0.0f, 0.5f, 0.5f, 1.0f, circleTex);
                else DrawPiece(0, h - r, r, r, 0.0f, 0.0f, 1.0f, 1.0f, defTex);

                // 4. 右下角
                if (roundBR) DrawPiece(w - r, h - r, r, r, 0.5f, 0.5f, 1.0f, 1.0f, circleTex);
                else DrawPiece(w - r, h - r, r, r, 0.0f, 0.0f, 1.0f, 1.0f, defTex);

                // 5. 上边缘
                if (w - 2 * r > 0) DrawPiece(r, 0, w - 2 * r, r, 0.0f, 0.0f, 1.0f, 1.0f, defTex);

                // 6. 下边缘
                if (w - 2 * r > 0) DrawPiece(r, h - r, w - 2 * r, r, 0.0f, 0.0f, 1.0f, 1.0f, defTex);

                // 7. 左边缘
                if (h - 2 * r > 0) DrawPiece(0, r, r, h - 2 * r, 0.0f, 0.0f, 1.0f, 1.0f, defTex);

                // 8. 右边缘
                if (h - 2 * r > 0) DrawPiece(w - r, r, r, h - 2 * r, 0.0f, 0.0f, 1.0f, 1.0f, defTex);

                // 9. 中心区域
                if (w - 2 * r > 0 && h - 2 * r > 0) DrawPiece(r, r, w - 2 * r, h - 2 * r, 0.0f, 0.0f, 1.0f, 1.0f, defTex);

                return;
            }
        }

        UTexture* Tex = texture ? texture : s_NativeCurrentCanvas->DefaultTexture;
        if (!Tex) return;

        if (clipEnabled) {
            if (pos.x + size.x <= clipMin.x || pos.x >= clipMax.x || pos.y + size.y <= clipMin.y || pos.y >= clipMax.y) return;
            if (pos.x < clipMin.x) { size.x -= (clipMin.x - pos.x); pos.x = clipMin.x; }
            if (pos.y < clipMin.y) { size.y -= (clipMin.y - pos.y); pos.y = clipMin.y; }
            if (pos.x + size.x > clipMax.x) size.x = clipMax.x - pos.x;
            if (pos.y + size.y > clipMax.y) size.y = clipMax.y - pos.y;
            if (size.x <= 0.f || size.y <= 0.f) return;
        }

        FLinearColor UEColor{ color.r, color.g, color.b, color.a };
        FVector2D UEPos{ static_cast<float>(pos.x), static_cast<float>(pos.y) };
        FVector2D UESize{ static_cast<float>(size.x), static_cast<float>(size.y) };

        s_NativeCurrentCanvas->K2_DrawTexture(
            Tex, UEPos, UESize,
            FVector2D{ 0.0f, 0.0f }, FVector2D{ 1.0f, 1.0f },
            UEColor, BLEND_Translucent, 0.0f, FVector2D{ 0.0f, 0.0f }
        );
    }

    inline void Native_DrawText(const std::string& text, Vec2 pos, Color color, UFont* font, float fontScale, Color textShadowColor, Color textOutlineColor, bool textOutline, bool noSDF = false) {
        if (!s_NativeCurrentCanvas || !font) return;

        ScopedNativeFontScale FontGuard(font, fontScale, noSDF);

        FVector2D Scale{ FontGuard.CalculatedScale, FontGuard.CalculatedScale };
        FLinearColor UEColor{ color.r, color.g, color.b, color.a };
        FVector2D UEPos{ static_cast<float>(pos.x), static_cast<float>(pos.y) };

        FLinearColor ShadowColor{ textShadowColor.r, textShadowColor.g, textShadowColor.b, textShadowColor.a };
        FLinearColor OutlineColor{ textOutlineColor.r, textOutlineColor.g, textOutlineColor.b, textOutlineColor.a };
        FVector2D ShadowOff{ Shadow::GetStyle().TextShadowOffsetX, Shadow::GetStyle().TextShadowOffsetY };

        FUTF8ToTCHAR Conv(text.data(), static_cast<int32>(text.size()));
        FString Str(Conv.Length(), Conv.Get());
        s_NativeCurrentCanvas->K2_DrawText(font, Str, UEPos, Scale, UEColor, 0.0f, ShadowColor, ShadowOff, false, false, textOutline, OutlineColor);
    }

    inline void Native_DrawTriangleFilled(Vec2 p1, Vec2 p2, Vec2 p3, Color color, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, UTexture* texture = nullptr) {
        Vec2 v[3] = { p1, p2, p3 };
        if (v[0].y > v[1].y) std::swap(v[0], v[1]);
        if (v[0].y > v[2].y) std::swap(v[0], v[2]);
        if (v[1].y > v[2].y) std::swap(v[1], v[2]);
        if (std::abs(v[0].y - v[2].y) < NATIVE_TRIANGLE_LINE_EPSILON) return;

        auto interpolateX = [](float y, const Vec2& a, const Vec2& b) {
            if (std::abs(a.y - b.y) < NATIVE_TRIANGLE_INTERPOLATE_EPSILON) return (a.x + b.x) * 0.5f;
            return a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y);
            };

        int startY = static_cast<int>(std::round(v[0].y));
        int endY = static_cast<int>(std::round(v[2].y));

        for (int y = startY; y < endY; ++y) {
            float fy = static_cast<float>(y) + 0.5f;
            float xa = interpolateX(fy, v[0], v[2]);
            float xb = (fy < v[1].y) ? interpolateX(fy, v[0], v[1]) : interpolateX(fy, v[1], v[2]);
            if (xa > xb) std::swap(xa, xb);

            float x_min = std::ceil(xa - 0.5f);
            float x_max = std::floor(xb - 0.5f);
            float w = x_max - x_min + 1.0f;

            if (w > 0.0f) {
                Native_DrawRectFilled({ x_min, static_cast<float>(y) }, { w, 1.0f }, color, clipEnabled, clipMin, clipMax, 0.0f, ShadowDrawFlags_RoundCornersNone, texture);
            }
        }
    }

    inline void Native_DrawTriangle(Vec2 p1, Vec2 p2, Vec2 p3, Color color, float thickness, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, UTexture* texture = nullptr) {
        Native_DrawLine(p1, p2, color, thickness, clipEnabled, clipMin, clipMax, texture);
        Native_DrawLine(p2, p3, color, thickness, clipEnabled, clipMin, clipMax, texture);
        Native_DrawLine(p3, p1, color, thickness, clipEnabled, clipMin, clipMax, texture);
    }

    // 鼠标坐标精准捕获与同步
    inline void Native_UpdateMousePos(FViewport* Viewport) {
        if (!Viewport) {
            return;
        }

        // 1. 优先获取 Viewport 内部渲染客户区坐标 (与 UCanvas 渲染分辨率 1:1 像素对齐)
        FIntPoint LocalMousePos;
        Viewport->GetMousePos(LocalMousePos, true);
        if (LocalMousePos.X >= 0 && LocalMousePos.Y >= 0 &&
            LocalMousePos.X <= Viewport->GetSizeXY().X &&
            LocalMousePos.Y <= Viewport->GetSizeXY().Y) {
            Shadow::g_Ctx.MousePos.x = static_cast<float>(LocalMousePos.X);
            Shadow::g_Ctx.MousePos.y = static_cast<float>(LocalMousePos.Y);
            return;
        }

        // 2. 备用映射：将 Slate 桌面屏幕绝对坐标转换为 Viewport 视口像素坐标
        if (FSlateApplication::IsInitialized()) {
            FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
            FIntPoint DesktopPoint(FMath::RoundToInt(CursorPos.X), FMath::RoundToInt(CursorPos.Y));
            FVector2D ViewportLocalPos = Viewport->VirtualDesktopPixelToViewport(DesktopPoint);
            Shadow::g_Ctx.MousePos.x = static_cast<float>(ViewportLocalPos.X);
            Shadow::g_Ctx.MousePos.y = static_cast<float>(ViewportLocalPos.Y);
        }
    }

    // 稳健键码映射函数 (保证英文字母/数字/功能键精确转换)
    inline int32 Native_GetVirtualKey(const FKey& Key) {
        // 0. 显式映射鼠标按键，确保与 Windows VK 码绝对一致
        if (Key == EKeys::LeftMouseButton)   return VK_LBUTTON;
        if (Key == EKeys::RightMouseButton)  return VK_RBUTTON;
        if (Key == EKeys::MiddleMouseButton) return VK_MBUTTON;
        if (Key == EKeys::ThumbMouseButton)  return VK_XBUTTON1;
        if (Key == EKeys::ThumbMouseButton2) return VK_XBUTTON2;

        // 1. 优先从 FInputKeyManager 获取官方映射
        const uint32* KeyCode = nullptr;
        const uint32* CharCode = nullptr;
        FInputKeyManager::Get().GetCodesFromKey(Key, KeyCode, CharCode);
        if (KeyCode && *KeyCode != 0) {
            return static_cast<int32>(*KeyCode);
        }
        if (CharCode && *CharCode != 0) {
            uint32 Code = *CharCode;
            if (Code >= 'a' && Code <= 'z') {
                Code = Code - 'a' + 'A';
            }
            return static_cast<int32>(Code);
        }

        // 2. 单字符键 (A~Z, 0~9) 兜底提取并转为标准虚拟键码
        FString NameStr = Key.GetFName().ToString();
        if (NameStr.Len() == 1) {
            TCHAR Ch = NameStr[0];
            if (Ch >= 'a' && Ch <= 'z') {
                return static_cast<int32>(Ch - 'a' + 'A');
            }
            if ((Ch >= 'A' && Ch <= 'Z') || (Ch >= '0' && Ch <= '9')) {
                return static_cast<int32>(Ch);
            }
        }

        // 3. 常见控制键兜底
        if (Key == EKeys::LeftControl || Key == EKeys::RightControl) return VK_CONTROL;
        if (Key == EKeys::LeftShift || Key == EKeys::RightShift)     return VK_SHIFT;
        if (Key == EKeys::LeftAlt || Key == EKeys::RightAlt)         return VK_MENU;
        if (Key == EKeys::BackSpace) return VK_BACK;
        if (Key == EKeys::Tab)       return VK_TAB;
        if (Key == EKeys::Enter)     return VK_RETURN;
        if (Key == EKeys::Escape)    return VK_ESCAPE;
        if (Key == EKeys::SpaceBar)  return VK_SPACE;
        if (Key == EKeys::Delete)    return VK_DELETE;
        if (Key == EKeys::Home)      return VK_HOME;
        if (Key == EKeys::End)       return VK_END;
        if (Key == EKeys::Left)      return VK_LEFT;
        if (Key == EKeys::Right)     return VK_RIGHT;
        if (Key == EKeys::Up)        return VK_UP;
        if (Key == EKeys::Down)      return VK_DOWN;

        return 0;
    }

    // 后端生命周期绑定
    inline void Native_RegisterBackendCallbacks() {
        Shadow::g_Ctx.TextSizeFn = &Native_MeasureText;
        Shadow::g_Ctx.LoadTextureFromBufferCallback = &Native_LoadTextureFromBuffer;
        Shadow::g_Ctx.LoadTextureFromFileCallback = &Native_LoadTextureFromFile;
        Shadow::g_Ctx.LoadFontFromFileCallback = &Native_LoadFontFromFile;
        Shadow::g_Ctx.LoadFontFromBufferCallback = &Native_LoadFontFromBuffer;
    }

    inline void Native_NewFrame(UCanvas* Canvas, FViewport* Viewport = nullptr) {
        s_NativeCurrentCanvas = Canvas;

        // 使用 FApp 单调高精度时钟，杜绝受游戏 World 暂停或时间膨胀影响导致 UI 连发与光标闪烁停滞
        s_NativeRealTimeSeconds = FApp::GetCurrentTime();
        s_NativeDeltaTime = FApp::GetDeltaTime();

        if (!Shadow::g_Ctx.DefaultFont) {
            if (GEngine) {
                Shadow::g_Ctx.DefaultFont = ShadowFont{ GEngine->GetLargeFont() };
            }
        }

        Native_RegisterBackendCallbacks();

        // 每一帧在渲染前无条件精准更新光标位置
        Native_UpdateMousePos(Viewport);

        // 每一帧持续同步修饰键（Ctrl、Shift、Alt），杜绝快捷键判断失灵
        if (FSlateApplication::IsInitialized()) {
            const FModifierKeysState& ModifierKeys = FSlateApplication::Get().GetModifierKeys();
            Shadow::g_Ctx.KeyStates[VK_CONTROL] = ModifierKeys.IsControlDown();
            Shadow::g_Ctx.KeyStates[VK_SHIFT] = ModifierKeys.IsShiftDown();
            Shadow::g_Ctx.KeyStates[VK_MENU] = ModifierKeys.IsAltDown();
        }

        Shadow::g_Ctx.RealTimeSeconds = s_NativeRealTimeSeconds;
        Shadow::g_Ctx.DeltaTime = s_NativeDeltaTime;

        ShadowIO& IO = Shadow::GetIO();
        IO.DisplaySize = {
            Canvas ? static_cast<float>(Canvas->SizeX) : 0.0f,
            Canvas ? static_cast<float>(Canvas->SizeY) : 0.0f
        };
        IO.DeltaTime = static_cast<float>(s_NativeDeltaTime);
        IO.BackendRenderer = BackendRenderer::Native;
    }

    inline Color Native_ApplyGamma(Color col, float gamma) {
        if (std::abs(gamma - NATIVE_DEFAULT_GAMMA) < NATIVE_MIN_GAMMA_DIFF || gamma <= 0.0f) {
            return col;
        }
        float invGamma = 1.0f / gamma;
        return Color{
            std::pow(std::max(0.0f, col.r), invGamma),
            std::pow(std::max(0.0f, col.g), invGamma),
            std::pow(std::max(0.0f, col.b), invGamma),
            col.a
        };
    }

    inline void Native_RenderDrawData(const ShadowDrawData* drawData) {
        if (!drawData || !drawData->Valid || !s_NativeCurrentCanvas) {
            return;
        }

        for (const ShadowDrawList* cmdList : drawData->CmdLists) {
            if (!cmdList) continue;
            for (const auto& cmd : cmdList->CmdBuffer) {
                Color drawCol = Native_ApplyGamma(cmd.color, cmd.gamma);

                switch (cmd.type) {
                case ShadowDrawCmdType::Line:
                    Native_DrawLine(cmd.pos, cmd.size, drawCol, cmd.thickness, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, static_cast<UTexture*>(cmd.texture.NativePtr));
                    break;
                case ShadowDrawCmdType::Rect:
                    Native_DrawRect(cmd.pos, cmd.size, drawCol, cmd.thickness, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, cmd.rounding, cmd.flags, static_cast<UTexture*>(cmd.texture.NativePtr));
                    break;
                case ShadowDrawCmdType::RectFilled:
                case ShadowDrawCmdType::Texture:
                    Native_DrawRectFilled(cmd.pos, cmd.size, drawCol, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, cmd.rounding, cmd.flags, static_cast<UTexture*>(cmd.texture.NativePtr));
                    break;
                case ShadowDrawCmdType::CircleFilled: {
                    float radius = cmd.size.x;
                    if (radius > 0.0f) {
                        UTexture2D* circleTex = Native_GetCircleTexture(radius);
                        if (circleTex) {
                            float diameter = radius * 2.0f;
                            Vec2 p = { cmd.pos.x - radius, cmd.pos.y - radius };
                            Vec2 sz = { diameter, diameter };
                            Native_DrawRectFilled(p, sz, drawCol, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, 0.0f, ShadowDrawFlags_RoundCornersNone, circleTex);
                        }
                    }
                    break;
                }
                case ShadowDrawCmdType::Text: {
                    Color textShadowCol = Native_ApplyGamma(cmd.textShadowColor, cmd.gamma);
                    Color textOutlineCol = Native_ApplyGamma(cmd.textOutlineColor, cmd.gamma);
                    Native_DrawText(cmd.text, cmd.pos, drawCol, static_cast<UFont*>(cmd.font.NativePtr), cmd.fontScale, textShadowCol, textOutlineCol, cmd.textOutline, cmd.noSDF);
                    break;
                }
                case ShadowDrawCmdType::TriangleFilled:
                    Native_DrawTriangleFilled(cmd.p1, cmd.p2, cmd.p3, drawCol, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, static_cast<UTexture*>(cmd.texture.NativePtr));
                    break;
                case ShadowDrawCmdType::Triangle:
                    Native_DrawTriangle(cmd.p1, cmd.p2, cmd.p3, drawCol, cmd.thickness, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, static_cast<UTexture*>(cmd.texture.NativePtr));
                    break;
                }
            }
        }
    }

    // 原生统一输入处理实现
    inline bool Native_InputKey(const FInputKeyEventArgs& EventArgs, bool bMenuOpen) {
        int32 WinVK = Native_GetVirtualKey(EventArgs.Key);

        // 菜单关闭时，响应已注册的全局热键状态更新并放行给游戏
        if (!bMenuOpen) {
            if (WinVK != 0 && WinVK < 256 && IsHotkeyRegistered(WinVK)) {
                if (EventArgs.Event == IE_Pressed || EventArgs.Event == IE_Repeat) {
                    HandleKeyDown(WinVK, EventArgs.Event == IE_Repeat);
                }
                else if (EventArgs.Event == IE_Released) {
                    HandleKeyUp(WinVK);
                }
            }
            return false;
        }

        // 同步修饰键
        if (FSlateApplication::IsInitialized()) {
            const FModifierKeysState& ModifierKeys = FSlateApplication::Get().GetModifierKeys();
            Shadow::g_Ctx.KeyStates[VK_CONTROL] = ModifierKeys.IsControlDown();
            Shadow::g_Ctx.KeyStates[VK_SHIFT] = ModifierKeys.IsShiftDown();
            Shadow::g_Ctx.KeyStates[VK_MENU] = ModifierKeys.IsAltDown();
        }

        bool bIsMouseKey = (EventArgs.Key == EKeys::LeftMouseButton ||
            EventArgs.Key == EKeys::RightMouseButton ||
            EventArgs.Key == EKeys::MiddleMouseButton ||
            EventArgs.Key == EKeys::ThumbMouseButton ||
            EventArgs.Key == EKeys::ThumbMouseButton2);

        // 鼠标按键处理
        if (bIsMouseKey) {
            if (EventArgs.Event == IE_Pressed || EventArgs.Event == IE_DoubleClick) {
                Native_UpdateMousePos(EventArgs.Viewport);

                // 先由 HandleKeyDown 处理；若当前处于分配热键状态，绑定成功后返回 true，直接吞掉本次点击
                bool bKeyAssigned = (WinVK != 0 && WinVK < 256) ? HandleKeyDown(WinVK) : false;
                if (bKeyAssigned) {
                    return true;
                }

                if (EventArgs.Key == EKeys::LeftMouseButton) {
                    bool wasDown = Shadow::g_Ctx.MouseDown;
                    Shadow::g_Ctx.MouseDown = true;
                    if (!wasDown) {
                        Shadow::g_Ctx.MouseClicked = true;
                    }
                }
                else if (EventArgs.Key == EKeys::RightMouseButton) {
                    bool wasDown = Shadow::g_Ctx.RightMouseDown;
                    Shadow::g_Ctx.RightMouseDown = true;
                    if (!wasDown) {
                        Shadow::g_Ctx.RightMouseClicked = true;
                    }
                }
                else if (EventArgs.Key == EKeys::MiddleMouseButton) {
                    bool wasDown = Shadow::g_Ctx.MiddleMouseDown;
                    Shadow::g_Ctx.MiddleMouseDown = true;
                    if (!wasDown) {
                        Shadow::g_Ctx.MiddleMouseClicked = true;
                    }
                }
            }
            else if (EventArgs.Event == IE_Released) {
                if (WinVK != 0 && WinVK < 256) {
                    HandleKeyUp(WinVK);
                }

                if (EventArgs.Key == EKeys::LeftMouseButton) {
                    Shadow::g_Ctx.MouseDown = false;
                    Shadow::g_Ctx.IsDragging = false;
                    Shadow::g_Ctx.IsResizing = false;
                }
                else if (EventArgs.Key == EKeys::RightMouseButton) {
                    Shadow::g_Ctx.RightMouseDown = false;
                }
                else if (EventArgs.Key == EKeys::MiddleMouseButton) {
                    Shadow::g_Ctx.MiddleMouseDown = false;
                }
            }

            return true;
        }

        // 键盘按键与连击事件处理
        if (WinVK != 0 && WinVK < 256) {
            if (EventArgs.Event == IE_Pressed || EventArgs.Event == IE_Repeat) {
                bool isRepeat = (EventArgs.Event == IE_Repeat);
                bool bKeyAssigned = HandleKeyDown(WinVK, isRepeat);
                if (WinVK == VK_LCONTROL || WinVK == VK_RCONTROL) HandleKeyDown(VK_CONTROL, isRepeat);
                if (WinVK == VK_LSHIFT || WinVK == VK_RSHIFT) HandleKeyDown(VK_SHIFT, isRepeat);
                if (WinVK == VK_LMENU || WinVK == VK_RMENU) HandleKeyDown(VK_MENU, isRepeat);

                if (bKeyAssigned) {
                    return true;
                }

                if (IsKeyAllowed(WinVK)) {
                    return false;
                }
                return true;
            }
            else if (EventArgs.Event == IE_Released) {
                HandleKeyUp(WinVK);
                if (WinVK == VK_LCONTROL || WinVK == VK_RCONTROL) {
                    if (FSlateApplication::IsInitialized() && !FSlateApplication::Get().GetModifierKeys().IsControlDown()) {
                        HandleKeyUp(VK_CONTROL);
                    }
                }
                if (WinVK == VK_LSHIFT || WinVK == VK_RSHIFT) {
                    if (FSlateApplication::IsInitialized() && !FSlateApplication::Get().GetModifierKeys().IsShiftDown()) {
                        HandleKeyUp(VK_SHIFT);
                    }
                }
                if (WinVK == VK_LMENU || WinVK == VK_RMENU) {
                    if (FSlateApplication::IsInitialized() && !FSlateApplication::Get().GetModifierKeys().IsAltDown()) {
                        HandleKeyUp(VK_MENU);
                    }
                }

                if (IsKeyAllowed(WinVK)) {
                    return false;
                }
                return true;
            }
        }

        return true;
    }

    inline bool Native_InputAxis(FViewport* InViewport, FInputDeviceId InputDevice, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad, bool bMenuOpen) {
        if (!bMenuOpen) {
            return false;
        }

        // 鼠标滚轮响应
        if (Key == EKeys::MouseWheelAxis) {
            Shadow::g_Ctx.MouseWheel += Delta;
            return true;
        }

        return true;
    }

    inline bool Native_InputChar(FViewport* InViewport, int32 ControllerId, TCHAR Character, bool bMenuOpen) {
        if (!bMenuOpen) {
            return false;
        }

        // 传递可打印字符
        if (Character >= NATIVE_MIN_PRINTABLE_CHAR && Character <= NATIVE_MAX_PRINTABLE_CHAR) {
            Shadow::g_Ctx.InputChars.push_back(static_cast<char>(Character));
        }

        return true;
    }
} // namespace Shadow