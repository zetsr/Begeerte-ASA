#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
#include <cstdint>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../misc/stb/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../misc/stb/stb_truetype.h"

#include "Shadow.h"

#pragma comment(lib, "d3d9.lib")

namespace Shadow {
    // 渲染管线与资源配置常量
    constexpr size_t INITIAL_VERTEX_BUFFER_SIZE = 8192;
    constexpr size_t INITIAL_INDEX_BUFFER_SIZE = 16384;
    constexpr int ATLAS_DEFAULT_WIDTH = 2048;
    constexpr int ATLAS_DEFAULT_HEIGHT = 2048;
    constexpr int ATLAS_PADDING = 1;
    constexpr float MATH_PI = 3.14159265358979323846f;
    constexpr int CIRCLE_NUM_SEGMENTS = 32;
    constexpr int CORNER_ROUNDING_SEGMENTS = 8;
    constexpr float THICKNESS_HALF_RATIO = 0.5f;
    constexpr float TEXTURE_DEFAULT_UV_MIN = 0.0f;
    constexpr float TEXTURE_DEFAULT_UV_MAX = 1.0f;
    constexpr int RGBA_BYTES_PER_PIXEL = 4;
    constexpr unsigned char COLOR_CHANNEL_MAX = 255;
    constexpr int MIN_TARGET_PIXEL_HEIGHT = 1;
    constexpr int DEFAULT_FALLBACK_FONT_SIZE = 14;
    constexpr float MITER_MIN_DOT_THRESHOLD = 0.1f;
    constexpr float MITER_MAX_LIMIT = 2.0f;
    constexpr float DX9_HALF_PIXEL_OFFSET = -0.5f;
    constexpr float DX9_DEFAULT_GAMMA = 1.0f;
    constexpr float DX9_MIN_GAMMA_DIFF = 0.0001f;

    // DX9 屏幕空间顶点格式 (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)
    struct DX9Vertex {
        float x, y, z, rhw;
        D3DCOLOR col;
        float u, v;
    };
    constexpr DWORD D3DFVF_DX9VERTEX = (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

    // 合并批次描述项
    struct DX9DrawBatch {
        uint32_t startIndex = 0;
        uint32_t indexCount = 0;
        IDirect3DTexture9* pTexture = nullptr;
        RECT scissorRect{};
        float gamma = DX9_DEFAULT_GAMMA;
    };

    // 字形度量缓存项
    struct DX9Glyph {
        float width;
        float height;
        float xOff;
        float yOff;
        float xAdvance;
        Vec2 uv0;
        Vec2 uv1;
    };

    // 字符与字号双维度复合键
    struct GlyphKey {
        uint32_t Codepoint;
        int PixelHeight;

        bool operator==(const GlyphKey& other) const noexcept {
            return Codepoint == other.Codepoint && PixelHeight == other.PixelHeight;
        }
    };

    struct GlyphKeyHasher {
        size_t operator()(const GlyphKey& k) const noexcept {
            return std::hash<uint32_t>()(k.Codepoint) ^ (std::hash<int>()(k.PixelHeight) << 16);
        }
    };

    // 字体资源封装结构体 (支持动态多尺寸按需矢量光栅化)
    struct DX9FontResource {
        stbtt_fontinfo FontInfo{};
        std::vector<unsigned char> FontBuffer;
        int BaseFontSize = DEFAULT_FALLBACK_FONT_SIZE;
        int Ascent = 0;
        int Descent = 0;
        int LineGap = 0;

        int AtlasWidth = ATLAS_DEFAULT_WIDTH;
        int AtlasHeight = ATLAS_DEFAULT_HEIGHT;
        std::vector<uint32_t> AtlasPixels;
        int CurrentShelfX = ATLAS_PADDING;
        int CurrentShelfY = ATLAS_PADDING;
        int CurrentShelfH = 0;
        bool AtlasDirty = false;

        IDirect3DTexture9* AtlasTexture = nullptr;
        std::unordered_map<GlyphKey, DX9Glyph, GlyphKeyHasher> Glyphs;

        ~DX9FontResource() {
            if (AtlasTexture) { AtlasTexture->Release(); AtlasTexture = nullptr; }
        }
    };

    // 字体复合缓存键
    struct FontBufferCacheKey {
        const void* BufferPtr;
        int32_t FontSize;
        bool operator==(const FontBufferCacheKey& o) const {
            return BufferPtr == o.BufferPtr && FontSize == o.FontSize;
        }
    };

    struct FontBufferCacheHasher {
        size_t operator()(const FontBufferCacheKey& k) const noexcept {
            return std::hash<const void*>()(k.BufferPtr) ^ (std::hash<int>()(k.FontSize) << 1);
        }
    };

    struct FontFileCacheKey {
        std::wstring FilePath;
        int32_t FontSize;
        bool operator==(const FontFileCacheKey& o) const {
            return FilePath == o.FilePath && FontSize == o.FontSize;
        }
    };

    struct FontFileCacheHasher {
        size_t operator()(const FontFileCacheKey& k) const noexcept {
            return std::hash<std::wstring>()(k.FilePath) ^ (std::hash<int>()(k.FontSize) << 1);
        }
    };

    // 后端全局状态
    inline HWND g_DX9_Hwnd = nullptr;
    inline IDirect3DDevice9* g_DX9_Device = nullptr;
    inline IDirect3DPixelShader9* g_DX9_PixelShader = nullptr;

    inline IDirect3DVertexBuffer9* g_DX9_VB = nullptr;
    inline IDirect3DIndexBuffer9* g_DX9_IB = nullptr;
    inline size_t g_DX9_VertexBufferSize = INITIAL_VERTEX_BUFFER_SIZE;
    inline size_t g_DX9_IndexBufferSize = INITIAL_INDEX_BUFFER_SIZE;

    inline IDirect3DTexture9* g_DX9_WhiteTexture = nullptr;

    inline std::vector<std::unique_ptr<DX9FontResource>> g_DX9_FontPool;
    inline std::unordered_map<FontFileCacheKey, ShadowFont, FontFileCacheHasher> g_DX9_FileFontCache;
    inline std::unordered_map<FontBufferCacheKey, ShadowFont, FontBufferCacheHasher> g_DX9_BufferFontCache;

    inline std::vector<IDirect3DTexture9*> g_DX9_TexturePool;
    inline std::unordered_map<std::wstring, ShadowTexture> g_DX9_FileTextureCache;
    inline std::unordered_map<const void*, ShadowTexture> g_DX9_BufferTextureCache;

    inline double g_DX9_Time = 0.0;
    inline LARGE_INTEGER g_DX9_PerfFrequency = { 0 };
    inline LARGE_INTEGER g_DX9_PerfCounter = { 0 };

    // 颜色转换：Color (浮点 RGBA) 转换为 D3DCOLOR (0xAARRGGBB)
    inline D3DCOLOR DX9_ToD3DColor(Color col) {
        auto Clamp255 = [](float f) -> DWORD {
            int n = static_cast<int>(f * 255.0f);
            return static_cast<DWORD>(std::clamp(n, 0, 255));
            };
        return (Clamp255(col.a) << 24) | (Clamp255(col.r) << 16) | (Clamp255(col.g) << 8) | Clamp255(col.b);
    }

    // UTF-8 解码为 UTF-32 Codepoint
    inline uint32_t DX9_DecodeUTF8(const char*& text, const char* end) {
        if (text >= end) return 0;
        unsigned char c = static_cast<unsigned char>(*text++);
        if (c < 0x80) return c;
        if ((c & 0xE0) == 0xC0) {
            if (text >= end) return 0;
            return ((c & 0x1F) << 6) | (static_cast<unsigned char>(*text++) & 0x3F);
        }
        if ((c & 0xF0) == 0xE0) {
            if (text + 1 >= end) return 0;
            uint32_t cp = ((c & 0x0F) << 12);
            cp |= (static_cast<unsigned char>(*text++) & 0x3F) << 6;
            cp |= (static_cast<unsigned char>(*text++) & 0x3F);
            return cp;
        }
        if ((c & 0xF8) == 0xF0) {
            if (text + 2 >= end) return 0;
            uint32_t cp = ((c & 0x07) << 18);
            cp |= (static_cast<unsigned char>(*text++) & 0x3F) << 12;
            cp |= (static_cast<unsigned char>(*text++) & 0x3F) << 6;
            cp |= (static_cast<unsigned char>(*text++) & 0x3F);
            return cp;
        }
        return c;
    }

    // 根据物理像素高度按需光栅化矢量字形
    inline const DX9Glyph* DX9_GetOrBakeGlyph(DX9FontResource* font, uint32_t codepoint, int pixelHeight) {
        if (!font) return nullptr;
        if (pixelHeight < MIN_TARGET_PIXEL_HEIGHT) {
            pixelHeight = MIN_TARGET_PIXEL_HEIGHT;
        }

        GlyphKey key{ codepoint, pixelHeight };
        auto it = font->Glyphs.find(key);
        if (it != font->Glyphs.end()) {
            return &it->second;
        }

        float scale = stbtt_ScaleForPixelHeight(&font->FontInfo, static_cast<float>(pixelHeight));

        int glyphIndex = stbtt_FindGlyphIndex(&font->FontInfo, static_cast<int>(codepoint));
        if (glyphIndex == 0 && codepoint != ' ') {
            glyphIndex = stbtt_FindGlyphIndex(&font->FontInfo, '?');
        }

        int advanceWidth = 0, leftSideBearing = 0;
        stbtt_GetGlyphHMetrics(&font->FontInfo, glyphIndex, &advanceWidth, &leftSideBearing);

        int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
        stbtt_GetGlyphBitmapBox(&font->FontInfo, glyphIndex, scale, scale, &ix0, &iy0, &ix1, &iy1);

        int glyphWidth = ix1 - ix0;
        int glyphHeight = iy1 - iy0;

        if (font->CurrentShelfX + glyphWidth + ATLAS_PADDING >= font->AtlasWidth) {
            font->CurrentShelfY += font->CurrentShelfH + ATLAS_PADDING;
            font->CurrentShelfX = ATLAS_PADDING;
            font->CurrentShelfH = 0;
        }

        if (font->CurrentShelfY + glyphHeight + ATLAS_PADDING >= font->AtlasHeight) {
            font->Glyphs.clear();
            std::fill(font->AtlasPixels.begin(), font->AtlasPixels.end(), 0x00FFFFFF);
            font->CurrentShelfX = ATLAS_PADDING;
            font->CurrentShelfY = ATLAS_PADDING;
            font->CurrentShelfH = 0;
            font->AtlasDirty = true;
        }

        int atlasX = font->CurrentShelfX;
        int atlasY = font->CurrentShelfY;

        if (glyphWidth > 0 && glyphHeight > 0) {
            std::vector<unsigned char> tempAlpha(glyphWidth * glyphHeight, 0);
            stbtt_MakeGlyphBitmap(&font->FontInfo, tempAlpha.data(), glyphWidth, glyphHeight, glyphWidth, scale, scale, glyphIndex);

            for (int r = 0; r < glyphHeight; ++r) {
                for (int c = 0; c < glyphWidth; ++c) {
                    unsigned char alpha = tempAlpha[r * glyphWidth + c];
                    size_t dstIdx = static_cast<size_t>((atlasY + r) * font->AtlasWidth + (atlasX + c));
                    // 存储为 0xAARRGGBB (ARGB)
                    font->AtlasPixels[dstIdx] = (static_cast<uint32_t>(alpha) << 24) | 0x00FFFFFF;
                }
            }
        }

        font->CurrentShelfX += glyphWidth + ATLAS_PADDING;
        if (glyphHeight > font->CurrentShelfH) {
            font->CurrentShelfH = glyphHeight;
        }

        DX9Glyph glyph{};
        glyph.width = static_cast<float>(glyphWidth);
        glyph.height = static_cast<float>(glyphHeight);
        glyph.xAdvance = static_cast<float>(advanceWidth) * scale;
        glyph.xOff = static_cast<float>(ix0);
        glyph.yOff = static_cast<float>(iy0) + static_cast<float>(font->Ascent) * scale;
        glyph.uv0 = { static_cast<float>(atlasX) / static_cast<float>(font->AtlasWidth), static_cast<float>(atlasY) / static_cast<float>(font->AtlasHeight) };
        glyph.uv1 = { static_cast<float>(atlasX + glyphWidth) / static_cast<float>(font->AtlasWidth), static_cast<float>(atlasY + glyphHeight) / static_cast<float>(font->AtlasHeight) };

        font->Glyphs[key] = glyph;
        font->AtlasDirty = true;
        return &font->Glyphs[key];
    }

    // 将图集提交上传至 DX9 纹理 (使用 D3DPOOL_MANAGED 托管池，Reset 期间无需销毁)
    inline void DX9_UpdateAtlasTexture(DX9FontResource* font) {
        if (!font || !font->AtlasDirty || !g_DX9_Device) return;

        if (!font->AtlasTexture) {
            if (FAILED(g_DX9_Device->CreateTexture(
                font->AtlasWidth, font->AtlasHeight, 1, 0,
                D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &font->AtlasTexture, nullptr))) {
                return;
            }
        }

        D3DLOCKED_RECT lockedRect;
        if (SUCCEEDED(font->AtlasTexture->LockRect(0, &lockedRect, nullptr, 0))) {
            for (int y = 0; y < font->AtlasHeight; ++y) {
                memcpy(
                    static_cast<unsigned char*>(lockedRect.pBits) + y * lockedRect.Pitch,
                    font->AtlasPixels.data() + y * font->AtlasWidth,
                    font->AtlasWidth * sizeof(uint32_t)
                );
            }
            font->AtlasTexture->UnlockRect(0);
        }

        font->AtlasDirty = false;
    }

    // 准确文本尺寸度量
    inline Vec2 DX9_MeasureText(ShadowFont shadowFont, float fontScale, bool noSDF, std::string_view text) {
        if (!shadowFont.NativePtr || text.empty()) {
            return { 0.f, 0.f };
        }

        auto* font = static_cast<DX9FontResource*>(shadowFont.NativePtr);
        const auto& style = Shadow::GetStyle();
        int targetPixelHeight = std::max(MIN_TARGET_PIXEL_HEIGHT, static_cast<int>(std::round(font->BaseFontSize * fontScale * style.FontScaleDpi)));

        float scale = stbtt_ScaleForPixelHeight(&font->FontInfo, static_cast<float>(targetPixelHeight));
        float fontLineHeight = static_cast<float>(font->Ascent - font->Descent) * scale;
        if (fontLineHeight <= 0.0f) {
            fontLineHeight = static_cast<float>(targetPixelHeight);
        }

        float width = 0.0f;
        const char* p = text.data();
        const char* end = p + text.size();

        while (p < end) {
            uint32_t cp = DX9_DecodeUTF8(p, end);
            if (cp == 0) break;
            const DX9Glyph* g = DX9_GetOrBakeGlyph(font, cp, targetPixelHeight);
            if (g) {
                width += g->xAdvance;
            }
        }

        return { width, fontLineHeight };
    }

    inline ShadowTexture DX9_LoadTextureFromBuffer(const unsigned char* BufferData, size_t BufferSize) {
        if (!BufferData || BufferSize == 0 || !g_DX9_Device) return ShadowTexture{};

        auto it = g_DX9_BufferTextureCache.find(BufferData);
        if (it != g_DX9_BufferTextureCache.end()) return it->second;

        int width = 0, height = 0, channels = 0;
        unsigned char* image = stbi_load_from_memory(BufferData, static_cast<int>(BufferSize), &width, &height, &channels, 4);
        if (!image) return ShadowTexture{};

        IDirect3DTexture9* pTexture = nullptr;
        HRESULT hr = g_DX9_Device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pTexture, nullptr);
        if (FAILED(hr)) {
            stbi_image_free(image);
            return ShadowTexture{};
        }

        D3DLOCKED_RECT lockedRect;
        if (SUCCEEDED(pTexture->LockRect(0, &lockedRect, nullptr, 0))) {
            for (int y = 0; y < height; ++y) {
                uint32_t* pDst = reinterpret_cast<uint32_t*>(static_cast<unsigned char*>(lockedRect.pBits) + y * lockedRect.Pitch);
                const unsigned char* pSrc = image + y * width * 4;
                for (int x = 0; x < width; ++x) {
                    unsigned char r = pSrc[x * 4 + 0];
                    unsigned char g = pSrc[x * 4 + 1];
                    unsigned char b = pSrc[x * 4 + 2];
                    unsigned char a = pSrc[x * 4 + 3];
                    // RGBA 转化为 D3DFMT_A8R8G8B8 (ARGB)
                    pDst[x] = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
                }
            }
            pTexture->UnlockRect(0);
        }

        stbi_image_free(image);

        g_DX9_TexturePool.push_back(pTexture);
        ShadowTexture tex{ pTexture };
        g_DX9_BufferTextureCache[BufferData] = tex;
        return tex;
    }

    inline ShadowTexture DX9_LoadTextureFromFile(const wchar_t* FilePath) {
        if (!FilePath || !g_DX9_Device) return ShadowTexture{};

        auto it = g_DX9_FileTextureCache.find(FilePath);
        if (it != g_DX9_FileTextureCache.end()) return it->second;

        FILE* f = nullptr;
        _wfopen_s(&f, FilePath, L"rb");
        if (!f) return ShadowTexture{};

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (size <= 0) {
            fclose(f);
            return ShadowTexture{};
        }

        std::vector<unsigned char> buffer(size);
        fread(buffer.data(), 1, size, f);
        fclose(f);

        ShadowTexture tex = DX9_LoadTextureFromBuffer(buffer.data(), buffer.size());
        if (tex) {
            g_DX9_FileTextureCache[FilePath] = tex;
        }
        return tex;
    }

    inline ShadowFont DX9_LoadFontFromBuffer(const unsigned char* BufferData, size_t BufferSize, int32_t FontSize = DEFAULT_FONT_SIZE) {
        if (!BufferData || BufferSize == 0) return ShadowFont{};

        const auto& style = Shadow::GetStyle();
        int targetFontSize = (FontSize >= MIN_VALID_FONT_SIZE) ? FontSize : static_cast<int>(style.FallbackFontSize);
        if (targetFontSize <= 0) targetFontSize = DEFAULT_FALLBACK_FONT_SIZE;

        FontBufferCacheKey key{ BufferData, targetFontSize };
        auto it = g_DX9_BufferFontCache.find(key);
        if (it != g_DX9_BufferFontCache.end()) return it->second;

        auto fontRes = std::make_unique<DX9FontResource>();
        fontRes->FontBuffer.assign(BufferData, BufferData + BufferSize);

        int fontOffset = stbtt_GetFontOffsetForIndex(fontRes->FontBuffer.data(), 0);
        if (fontOffset < 0) {
            fontOffset = 0;
        }

        if (!stbtt_InitFont(&fontRes->FontInfo, fontRes->FontBuffer.data(), fontOffset)) {
            return ShadowFont{};
        }

        fontRes->BaseFontSize = targetFontSize;
        stbtt_GetFontVMetrics(&fontRes->FontInfo, &fontRes->Ascent, &fontRes->Descent, &fontRes->LineGap);

        fontRes->AtlasPixels.resize(static_cast<size_t>(fontRes->AtlasWidth * fontRes->AtlasHeight), 0x00FFFFFF);

        for (uint32_t c = 32; c <= 126; ++c) {
            DX9_GetOrBakeGlyph(fontRes.get(), c, targetFontSize);
        }

        ShadowFont shadowFont{ fontRes.get() };
        g_DX9_BufferFontCache[key] = shadowFont;
        g_DX9_FontPool.push_back(std::move(fontRes));
        return shadowFont;
    }

    inline ShadowFont DX9_LoadFontFromFile(const wchar_t* FilePath, int32_t FontSize = DEFAULT_FONT_SIZE) {
        if (!FilePath) return ShadowFont{};

        const auto& style = Shadow::GetStyle();
        int targetFontSize = (FontSize >= MIN_VALID_FONT_SIZE) ? FontSize : static_cast<int>(style.FallbackFontSize);
        if (targetFontSize <= 0) targetFontSize = DEFAULT_FALLBACK_FONT_SIZE;

        FontFileCacheKey key{ FilePath, targetFontSize };
        auto it = g_DX9_FileFontCache.find(key);
        if (it != g_DX9_FileFontCache.end()) return it->second;

        FILE* f = nullptr;
        _wfopen_s(&f, FilePath, L"rb");
        if (!f) return ShadowFont{};

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (size <= 0) {
            fclose(f);
            return ShadowFont{};
        }

        std::vector<unsigned char> buffer(size);
        fread(buffer.data(), 1, size, f);
        fclose(f);

        auto fontRes = std::make_unique<DX9FontResource>();
        fontRes->FontBuffer = std::move(buffer);

        int fontOffset = stbtt_GetFontOffsetForIndex(fontRes->FontBuffer.data(), 0);
        if (fontOffset < 0) {
            fontOffset = 0;
        }

        if (!stbtt_InitFont(&fontRes->FontInfo, fontRes->FontBuffer.data(), fontOffset)) {
            return ShadowFont{};
        }

        fontRes->BaseFontSize = targetFontSize;
        stbtt_GetFontVMetrics(&fontRes->FontInfo, &fontRes->Ascent, &fontRes->Descent, &fontRes->LineGap);

        fontRes->AtlasPixels.resize(static_cast<size_t>(fontRes->AtlasWidth * fontRes->AtlasHeight), 0x00FFFFFF);

        for (uint32_t c = 32; c <= 126; ++c) {
            DX9_GetOrBakeGlyph(fontRes.get(), c, targetFontSize);
        }

        ShadowFont shadowFont{ fontRes.get() };
        g_DX9_FileFontCache[key] = shadowFont;
        g_DX9_FontPool.push_back(std::move(fontRes));
        return shadowFont;
    }

    inline void DX9_RegisterBackendCallbacks() {
        Shadow::g_Ctx.TextSizeFn = &DX9_MeasureText;
        Shadow::g_Ctx.LoadTextureFromBufferCallback = &DX9_LoadTextureFromBuffer;
        Shadow::g_Ctx.LoadTextureFromFileCallback = &DX9_LoadTextureFromFile;
        Shadow::g_Ctx.LoadFontFromFileCallback = &DX9_LoadFontFromFile;
        Shadow::g_Ctx.LoadFontFromBufferCallback = &DX9_LoadFontFromBuffer;
        Shadow::g_Ctx.BackendSupportsTextPixelClipping = true;
    }

    // 创建动态资源（VB / IB / 1x1 白纹理）
    inline bool DX9_CreateDeviceObjects() {
        if (!g_DX9_Device) return false;

        if (!g_DX9_WhiteTexture) {
            if (FAILED(g_DX9_Device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_DX9_WhiteTexture, nullptr))) {
                return false;
            }
            D3DLOCKED_RECT lr;
            if (SUCCEEDED(g_DX9_WhiteTexture->LockRect(0, &lr, nullptr, 0))) {
                *reinterpret_cast<uint32_t*>(lr.pBits) = 0xFFFFFFFF;
                g_DX9_WhiteTexture->UnlockRect(0);
            }
        }

        if (!g_DX9_PixelShader) {
            const char* pixelShaderHLSL =
                "sampler2D TextureSampler : register(s0);\n"
                "float4 GammaParams : register(c0);\n"
                "struct PS_INPUT {\n"
                "    float4 col : COLOR0;\n"
                "    float2 uv  : TEXCOORD0;\n"
                "};\n"
                "float4 main(PS_INPUT input) : COLOR0 {\n"
                "    float4 texColor = tex2D(TextureSampler, input.uv);\n"
                "    float4 outColor = input.col * texColor;\n"
                "    outColor.rgb = pow(max(outColor.rgb, 0.0f), GammaParams.x);\n"
                "    return outColor;\n"
                "}\n";

            ID3DBlob* psBlob = nullptr;
            if (FAILED(D3DCompile(pixelShaderHLSL, strlen(pixelShaderHLSL), nullptr, nullptr, nullptr, "main", "ps_2_0", 0, 0, &psBlob, nullptr))) {
                return false;
            }
            HRESULT hr = g_DX9_Device->CreatePixelShader(reinterpret_cast<const DWORD*>(psBlob->GetBufferPointer()), &g_DX9_PixelShader);
            psBlob->Release();
            if (FAILED(hr)) return false;
        }

        return true;
    }

    // 释放默认池（D3DPOOL_DEFAULT）资源，供 Reset 之前调用
    inline void DX9_InvalidateDeviceObjects() {
        if (g_DX9_VB) { g_DX9_VB->Release(); g_DX9_VB = nullptr; }
        if (g_DX9_IB) { g_DX9_IB->Release(); g_DX9_IB = nullptr; }
        if (g_DX9_PixelShader) { g_DX9_PixelShader->Release(); g_DX9_PixelShader = nullptr; }
    }

    inline bool DX9_Init(HWND hWnd, IDirect3DDevice9* device) {
        g_DX9_Hwnd = hWnd;
        g_DX9_Device = device;

        QueryPerformanceFrequency(&g_DX9_PerfFrequency);
        QueryPerformanceCounter(&g_DX9_PerfCounter);

        DX9_RegisterBackendCallbacks();
        return DX9_CreateDeviceObjects();
    }

    inline void DX9_Shutdown() {
        DX9_InvalidateDeviceObjects();
        if (g_DX9_WhiteTexture) { g_DX9_WhiteTexture->Release(); g_DX9_WhiteTexture = nullptr; }

        g_DX9_FontPool.clear();
        g_DX9_FileFontCache.clear();
        g_DX9_BufferFontCache.clear();

        for (auto* tex : g_DX9_TexturePool) {
            if (tex) tex->Release();
        }
        g_DX9_TexturePool.clear();
        g_DX9_FileTextureCache.clear();
        g_DX9_BufferTextureCache.clear();

        g_DX9_Device = nullptr;
        g_DX9_Hwnd = nullptr;
    }

    inline void DX9_NewFrame() {
        if (!g_DX9_WhiteTexture) {
            DX9_CreateDeviceObjects();
        }

        DX9_RegisterBackendCallbacks();

        LARGE_INTEGER currentCounter;
        QueryPerformanceCounter(&currentCounter);
        double dt = static_cast<double>(currentCounter.QuadPart - g_DX9_PerfCounter.QuadPart) / static_cast<double>(g_DX9_PerfFrequency.QuadPart);
        g_DX9_PerfCounter = currentCounter;
        g_DX9_Time += dt;

        RECT rect{ 0, 0, 0, 0 };
        if (g_DX9_Hwnd) {
            GetClientRect(g_DX9_Hwnd, &rect);
        }

        ShadowIO& io = Shadow::GetIO();
        io.DisplaySize = { static_cast<float>(rect.right - rect.left), static_cast<float>(rect.bottom - rect.top) };
        io.DeltaTime = static_cast<float>(dt);
        io.BackendRenderer = BackendRenderer::DX9;

        Shadow::g_Ctx.RealTimeSeconds = g_DX9_Time;
        Shadow::g_Ctx.DeltaTime = dt;

        if (!Shadow::g_Ctx.DefaultFont) {
            Shadow::g_Ctx.DefaultFont = DX9_LoadFontFromFile(L"C:\\Windows\\Fonts\\msyh.ttc", DEFAULT_FALLBACK_FONT_SIZE);
            if (!Shadow::g_Ctx.DefaultFont) {
                Shadow::g_Ctx.DefaultFont = DX9_LoadFontFromFile(L"C:\\Windows\\Fonts\\simhei.ttf", DEFAULT_FALLBACK_FONT_SIZE);
            }
            if (!Shadow::g_Ctx.DefaultFont) {
                Shadow::g_Ctx.DefaultFont = DX9_LoadFontFromFile(L"C:\\Windows\\Fonts\\arial.ttf", DEFAULT_FALLBACK_FONT_SIZE);
            }
        }
    }

    // 内部几何生成与批次合并构建器
    struct DX9MeshBuilder {
        std::vector<DX9Vertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<DX9DrawBatch> Batches;

        void Clear() {
            Vertices.clear();
            Indices.clear();
            Batches.clear();
        }

        void EnsureBatch(IDirect3DTexture9* pTexture, const RECT& scissor, float gamma) {
            if (!Batches.empty()) {
                auto& last = Batches.back();
                if (last.pTexture == pTexture &&
                    last.scissorRect.left == scissor.left &&
                    last.scissorRect.top == scissor.top &&
                    last.scissorRect.right == scissor.right &&
                    last.scissorRect.bottom == scissor.bottom &&
                    std::abs(last.gamma - gamma) < DX9_MIN_GAMMA_DIFF) {
                    return;
                }
            }
            DX9DrawBatch newBatch{};
            newBatch.startIndex = static_cast<uint32_t>(Indices.size());
            newBatch.indexCount = 0;
            newBatch.pTexture = pTexture;
            newBatch.scissorRect = scissor;
            newBatch.gamma = gamma;
            Batches.push_back(newBatch);
        }

        void AddVertex(Vec2 pos, Vec2 uv, D3DCOLOR col) {
            // 施加 DX9 -0.5f 像素偏移校准，实现 100% 锐利清晰度
            Vertices.push_back({ pos.x + DX9_HALF_PIXEL_OFFSET, pos.y + DX9_HALF_PIXEL_OFFSET, 0.0f, 1.0f, col, uv.x, uv.y });
        }

        void AddTriangleIndices(uint32_t i0, uint32_t i1, uint32_t i2) {
            Indices.push_back(i0);
            Indices.push_back(i1);
            Indices.push_back(i2);
            if (!Batches.empty()) {
                Batches.back().indexCount += 3;
            }
        }

        void BuildThickLine(Vec2 p0, Vec2 p1, D3DCOLOR col, float thickness) {
            float dx = p1.x - p0.x;
            float dy = p1.y - p0.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len <= 0.0f) return;

            float nx = -dy / len * (thickness * THICKNESS_HALF_RATIO);
            float ny = dx / len * (thickness * THICKNESS_HALF_RATIO);

            uint32_t idx = static_cast<uint32_t>(Vertices.size());
            AddVertex({ p0.x + nx, p0.y + ny }, { 0.f, 0.f }, col);
            AddVertex({ p0.x - nx, p0.y - ny }, { 0.f, 0.f }, col);
            AddVertex({ p1.x - nx, p1.y - ny }, { 0.f, 0.f }, col);
            AddVertex({ p1.x + nx, p1.y + ny }, { 0.f, 0.f }, col);

            AddTriangleIndices(idx, idx + 1, idx + 2);
            AddTriangleIndices(idx, idx + 2, idx + 3);
        }

        void BuildPolylineStroke(const std::vector<Vec2>& points, D3DCOLOR col, float thickness) {
            size_t count = points.size();
            if (count < 3) return;

            std::vector<Vec2> normals(count);
            for (size_t i = 0; i < count; ++i) {
                size_t next = (i + 1 == count) ? 0 : (i + 1);
                float dx = points[next].x - points[i].x;
                float dy = points[next].y - points[i].y;
                float len = std::sqrt(dx * dx + dy * dy);
                if (len > 0.0f) {
                    normals[i] = { dy / len, -dx / len };
                }
                else {
                    normals[i] = { 0.f, 0.f };
                }
            }

            uint32_t baseIdx = static_cast<uint32_t>(Vertices.size());
            float halfThickness = thickness * THICKNESS_HALF_RATIO;

            for (size_t i = 0; i < count; ++i) {
                size_t prev = (i == 0) ? (count - 1) : (i - 1);
                float nx = normals[prev].x + normals[i].x;
                float ny = normals[prev].y + normals[i].y;
                float nlen = std::sqrt(nx * nx + ny * ny);
                if (nlen > 0.0f) {
                    nx /= nlen;
                    ny /= nlen;
                }

                float dot = nx * normals[i].x + ny * normals[i].y;
                float miter = (dot > MITER_MIN_DOT_THRESHOLD) ? (1.0f / dot) : 1.0f;
                if (miter > MITER_MAX_LIMIT) miter = MITER_MAX_LIMIT;

                float offX = nx * halfThickness * miter;
                float offY = ny * halfThickness * miter;

                AddVertex({ points[i].x + offX, points[i].y + offY }, { 0.f, 0.f }, col);
                AddVertex({ points[i].x - offX, points[i].y - offY }, { 0.f, 0.f }, col);
            }

            for (size_t i = 0; i < count; ++i) {
                uint32_t next = static_cast<uint32_t>((i + 1 == count) ? 0 : (i + 1));
                uint32_t o0 = baseIdx + static_cast<uint32_t>(i * 2);
                uint32_t i0 = o0 + 1;
                uint32_t o1 = baseIdx + static_cast<uint32_t>(next * 2);
                uint32_t i1 = o1 + 1;

                AddTriangleIndices(o0, i0, i1);
                AddTriangleIndices(o0, i1, o1);
            }
        }

        void BuildRect(Vec2 pos, Vec2 size, D3DCOLOR col, float thickness, float rounding, ShadowDrawFlags flags) {
            if (size.x <= 0.f || size.y <= 0.f || thickness <= 0.f) return;

            bool roundTL = (flags & ShadowDrawFlags_RoundCornersTopLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
            bool roundTR = (flags & ShadowDrawFlags_RoundCornersTopRight) && !(flags & ShadowDrawFlags_RoundCornersNone);
            bool roundBL = (flags & ShadowDrawFlags_RoundCornersBottomLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
            bool roundBR = (flags & ShadowDrawFlags_RoundCornersBottomRight) && !(flags & ShadowDrawFlags_RoundCornersNone);

            bool noRound = (rounding <= 0.0f) || (!roundTL && !roundTR && !roundBL && !roundBR);

            std::vector<Vec2> points;
            points.reserve(noRound ? 4 : (CORNER_ROUNDING_SEGMENTS + 1) * 4);

            if (noRound) {
                points.push_back(pos);
                points.push_back({ pos.x + size.x, pos.y });
                points.push_back({ pos.x + size.x, pos.y + size.y });
                points.push_back({ pos.x, pos.y + size.y });
            }
            else {
                float r = std::min(rounding, std::min(size.x, size.y) * 0.5f);

                auto AppendArc = [&](Vec2 center, float radius, float startAngle, float endAngle) {
                    int segs = CORNER_ROUNDING_SEGMENTS;
                    for (int i = 0; i <= segs; ++i) {
                        float t = static_cast<float>(i) / static_cast<float>(segs);
                        float a = startAngle + t * (endAngle - startAngle);
                        points.push_back({ center.x + radius * std::cos(a), center.y - radius * std::sin(a) });
                    }
                    };

                if (roundTL) AppendArc({ pos.x + r, pos.y + r }, r, MATH_PI, MATH_PI * 0.5f);
                else points.push_back(pos);

                if (roundTR) AppendArc({ pos.x + size.x - r, pos.y + r }, r, MATH_PI * 0.5f, 0.0f);
                else points.push_back({ pos.x + size.x, pos.y });

                if (roundBR) AppendArc({ pos.x + size.x - r, pos.y + size.y - r }, r, 0.0f, -MATH_PI * 0.5f);
                else points.push_back({ pos.x + size.x, pos.y + size.y });

                if (roundBL) AppendArc({ pos.x + r, pos.y + size.y - r }, r, -MATH_PI * 0.5f, -MATH_PI);
                else points.push_back({ pos.x, pos.y + size.y });
            }

            BuildPolylineStroke(points, col, thickness);
        }

        void BuildRectFilled(Vec2 pos, Vec2 size, D3DCOLOR col, float rounding, ShadowDrawFlags flags) {
            if (size.x <= 0.f || size.y <= 0.f) return;

            bool roundTL = (flags & ShadowDrawFlags_RoundCornersTopLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
            bool roundTR = (flags & ShadowDrawFlags_RoundCornersTopRight) && !(flags & ShadowDrawFlags_RoundCornersNone);
            bool roundBL = (flags & ShadowDrawFlags_RoundCornersBottomLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
            bool roundBR = (flags & ShadowDrawFlags_RoundCornersBottomRight) && !(flags & ShadowDrawFlags_RoundCornersNone);

            if (rounding <= 0.0f || (!roundTL && !roundTR && !roundBL && !roundBR)) {
                uint32_t idx = static_cast<uint32_t>(Vertices.size());
                AddVertex(pos, { 0.f, 0.f }, col);
                AddVertex({ pos.x + size.x, pos.y }, { 1.f, 0.f }, col);
                AddVertex({ pos.x + size.x, pos.y + size.y }, { 1.f, 1.f }, col);
                AddVertex({ pos.x, pos.y + size.y }, { 0.f, 1.f }, col);

                AddTriangleIndices(idx, idx + 1, idx + 2);
                AddTriangleIndices(idx, idx + 2, idx + 3);
                return;
            }

            float r = std::min(rounding, std::min(size.x, size.y) * 0.5f);
            uint32_t centerIdx = static_cast<uint32_t>(Vertices.size());
            AddVertex({ pos.x + size.x * 0.5f, pos.y + size.y * 0.5f }, { 0.5f, 0.5f }, col);

            auto AppendArc = [&](Vec2 center, float radius, float startAngle, float endAngle) {
                int segs = CORNER_ROUNDING_SEGMENTS;
                for (int i = 0; i <= segs; ++i) {
                    float t = static_cast<float>(i) / static_cast<float>(segs);
                    float a = startAngle + t * (endAngle - startAngle);
                    AddVertex({ center.x + radius * std::cos(a), center.y - radius * std::sin(a) }, { 0.f, 0.f }, col);
                }
                };

            if (roundTL) AppendArc({ pos.x + r, pos.y + r }, r, MATH_PI, MATH_PI * 0.5f);
            else AddVertex(pos, { 0.f, 0.f }, col);

            if (roundTR) AppendArc({ pos.x + size.x - r, pos.y + r }, r, MATH_PI * 0.5f, 0.0f);
            else AddVertex({ pos.x + size.x, pos.y }, { 1.f, 0.f }, col);

            if (roundBR) AppendArc({ pos.x + size.x - r, pos.y + size.y - r }, r, 0.0f, -MATH_PI * 0.5f);
            else AddVertex({ pos.x + size.x, pos.y + size.y }, { 1.f, 1.f }, col);

            if (roundBL) AppendArc({ pos.x + r, pos.y + size.y - r }, r, -MATH_PI * 0.5f, -MATH_PI);
            else AddVertex({ pos.x, pos.y + size.y }, { 0.f, 1.f }, col);

            uint32_t totalPerimeter = static_cast<uint32_t>(Vertices.size() - centerIdx - 1);
            for (uint32_t i = 0; i < totalPerimeter; ++i) {
                uint32_t current = centerIdx + 1 + i;
                uint32_t next = (i + 1 < totalPerimeter) ? (current + 1) : (centerIdx + 1);
                AddTriangleIndices(centerIdx, current, next);
            }
        }

        void BuildCircleFilled(Vec2 center, float radius, D3DCOLOR col) {
            if (radius <= 0.0f) return;
            uint32_t centerIdx = static_cast<uint32_t>(Vertices.size());
            AddVertex(center, { 0.5f, 0.5f }, col);

            int segs = CIRCLE_NUM_SEGMENTS;
            for (int i = 0; i <= segs; ++i) {
                float a = (static_cast<float>(i) / static_cast<float>(segs)) * 2.0f * MATH_PI;
                AddVertex({ center.x + radius * std::cos(a), center.y + radius * std::sin(a) }, { 0.5f, 0.5f }, col);
            }

            for (int i = 0; i < segs; ++i) {
                AddTriangleIndices(centerIdx, centerIdx + 1 + i, centerIdx + 2 + i);
            }
        }
    };

    // 渲染后端 DrawData 核心执行逻辑
    inline void DX9_RenderDrawData(const ShadowDrawData* drawData) {
        if (!drawData || !drawData->Valid || !g_DX9_Device) return;
        if (drawData->DisplaySize.x <= 0.f || drawData->DisplaySize.y <= 0.f) return;

        const auto& globalStyle = Shadow::GetStyle();

        // 第一阶段：预扫描文本命令，按目标尺寸按需光栅化
        for (const ShadowDrawList* cmdList : drawData->CmdLists) {
            if (!cmdList) continue;
            for (const auto& cmd : cmdList->CmdBuffer) {
                if (cmd.type == ShadowDrawCmdType::Text && cmd.font.NativePtr && !cmd.text.empty()) {
                    auto* font = static_cast<DX9FontResource*>(cmd.font.NativePtr);
                    int targetPixelHeight = std::max(MIN_TARGET_PIXEL_HEIGHT, static_cast<int>(std::round(font->BaseFontSize * cmd.fontScale * globalStyle.FontScaleDpi)));

                    const char* p = cmd.text.data();
                    const char* end = p + cmd.text.size();
                    while (p < end) {
                        uint32_t cp = DX9_DecodeUTF8(p, end);
                        if (cp == 0) break;
                        DX9_GetOrBakeGlyph(font, cp, targetPixelHeight);
                    }
                }
            }
        }

        // 第二阶段：提交脏图集至 DX9 纹理
        for (auto& font : g_DX9_FontPool) {
            if (font->AtlasDirty || !font->AtlasTexture) {
                DX9_UpdateAtlasTexture(font.get());
            }
        }

        // 第三阶段：收集顶点并进行批次合并
        static DX9MeshBuilder s_GlobalMeshBuilder;
        s_GlobalMeshBuilder.Clear();

        for (const ShadowDrawList* cmdList : drawData->CmdLists) {
            if (!cmdList) continue;
            for (const auto& cmd : cmdList->CmdBuffer) {
                RECT scissor{ 0, 0, static_cast<LONG>(drawData->DisplaySize.x), static_cast<LONG>(drawData->DisplaySize.y) };
                if (cmd.clippingEnabled) {
                    scissor.left = std::max(0L, static_cast<LONG>(cmd.clipMin.x));
                    scissor.top = std::max(0L, static_cast<LONG>(cmd.clipMin.y));
                    scissor.right = std::min(static_cast<LONG>(drawData->DisplaySize.x), static_cast<LONG>(cmd.clipMax.x));
                    scissor.bottom = std::min(static_cast<LONG>(drawData->DisplaySize.y), static_cast<LONG>(cmd.clipMax.y));
                    if (scissor.right <= scissor.left || scissor.bottom <= scissor.top) continue;
                }

                IDirect3DTexture9* pTexture = static_cast<IDirect3DTexture9*>(cmd.texture.NativePtr);
                D3DCOLOR d3dCol = DX9_ToD3DColor(cmd.color);

                switch (cmd.type) {
                case ShadowDrawCmdType::Line:
                    s_GlobalMeshBuilder.EnsureBatch(pTexture, scissor, cmd.gamma);
                    s_GlobalMeshBuilder.BuildThickLine(cmd.pos, cmd.size, d3dCol, cmd.thickness);
                    break;
                case ShadowDrawCmdType::Rect:
                    s_GlobalMeshBuilder.EnsureBatch(pTexture, scissor, cmd.gamma);
                    s_GlobalMeshBuilder.BuildRect(cmd.pos, cmd.size, d3dCol, cmd.thickness, cmd.rounding, cmd.flags);
                    break;
                case ShadowDrawCmdType::RectFilled:
                    s_GlobalMeshBuilder.EnsureBatch(pTexture, scissor, cmd.gamma);
                    s_GlobalMeshBuilder.BuildRectFilled(cmd.pos, cmd.size, d3dCol, cmd.rounding, cmd.flags);
                    break;
                case ShadowDrawCmdType::Texture: {
                    s_GlobalMeshBuilder.EnsureBatch(pTexture, scissor, cmd.gamma);
                    uint32_t idx = static_cast<uint32_t>(s_GlobalMeshBuilder.Vertices.size());
                    s_GlobalMeshBuilder.AddVertex(cmd.pos, { TEXTURE_DEFAULT_UV_MIN, TEXTURE_DEFAULT_UV_MIN }, d3dCol);
                    s_GlobalMeshBuilder.AddVertex({ cmd.pos.x + cmd.size.x, cmd.pos.y }, { TEXTURE_DEFAULT_UV_MAX, TEXTURE_DEFAULT_UV_MIN }, d3dCol);
                    s_GlobalMeshBuilder.AddVertex({ cmd.pos.x + cmd.size.x, cmd.pos.y + cmd.size.y }, { TEXTURE_DEFAULT_UV_MAX, TEXTURE_DEFAULT_UV_MAX }, d3dCol);
                    s_GlobalMeshBuilder.AddVertex({ cmd.pos.x, cmd.pos.y + cmd.size.y }, { TEXTURE_DEFAULT_UV_MIN, TEXTURE_DEFAULT_UV_MAX }, d3dCol);
                    s_GlobalMeshBuilder.AddTriangleIndices(idx, idx + 1, idx + 2);
                    s_GlobalMeshBuilder.AddTriangleIndices(idx, idx + 2, idx + 3);
                    break;
                }
                case ShadowDrawCmdType::CircleFilled:
                    s_GlobalMeshBuilder.EnsureBatch(pTexture, scissor, cmd.gamma);
                    s_GlobalMeshBuilder.BuildCircleFilled(cmd.pos, cmd.size.x, d3dCol);
                    break;
                case ShadowDrawCmdType::Triangle:
                    s_GlobalMeshBuilder.EnsureBatch(pTexture, scissor, cmd.gamma);
                    s_GlobalMeshBuilder.BuildThickLine(cmd.p1, cmd.p2, d3dCol, cmd.thickness);
                    s_GlobalMeshBuilder.BuildThickLine(cmd.p2, cmd.p3, d3dCol, cmd.thickness);
                    s_GlobalMeshBuilder.BuildThickLine(cmd.p3, cmd.p1, d3dCol, cmd.thickness);
                    break;
                case ShadowDrawCmdType::TriangleFilled: {
                    s_GlobalMeshBuilder.EnsureBatch(pTexture, scissor, cmd.gamma);
                    uint32_t idx = static_cast<uint32_t>(s_GlobalMeshBuilder.Vertices.size());
                    s_GlobalMeshBuilder.AddVertex(cmd.p1, { 0.f, 0.f }, d3dCol);
                    s_GlobalMeshBuilder.AddVertex(cmd.p2, { 0.f, 0.f }, d3dCol);
                    s_GlobalMeshBuilder.AddVertex(cmd.p3, { 0.f, 0.f }, d3dCol);
                    s_GlobalMeshBuilder.AddTriangleIndices(idx, idx + 1, idx + 2);
                    break;
                }
                case ShadowDrawCmdType::Text: {
                    if (!cmd.font.NativePtr || cmd.text.empty()) break;
                    auto* font = static_cast<DX9FontResource*>(cmd.font.NativePtr);
                    int targetPixelHeight = std::max(MIN_TARGET_PIXEL_HEIGHT, static_cast<int>(std::round(font->BaseFontSize * cmd.fontScale * globalStyle.FontScaleDpi)));

                    s_GlobalMeshBuilder.EnsureBatch(font->AtlasTexture, scissor, cmd.gamma);

                    auto DrawString = [&](Vec2 pos, D3DCOLOR col) {
                        const char* p = cmd.text.data();
                        const char* end = p + cmd.text.size();
                        float curX = pos.x;
                        float curY = pos.y;

                        while (p < end) {
                            uint32_t cp = DX9_DecodeUTF8(p, end);
                            if (cp == 0) break;

                            const DX9Glyph* g = DX9_GetOrBakeGlyph(font, cp, targetPixelHeight);
                            if (g) {
                                float x0 = std::floor(curX + g->xOff);
                                float y0 = std::floor(curY + g->yOff);
                                float x1 = x0 + g->width;
                                float y1 = y0 + g->height;

                                uint32_t vidx = static_cast<uint32_t>(s_GlobalMeshBuilder.Vertices.size());
                                s_GlobalMeshBuilder.AddVertex({ x0, y0 }, { g->uv0.x, g->uv0.y }, col);
                                s_GlobalMeshBuilder.AddVertex({ x1, y0 }, { g->uv1.x, g->uv0.y }, col);
                                s_GlobalMeshBuilder.AddVertex({ x1, y1 }, { g->uv1.x, g->uv1.y }, col);
                                s_GlobalMeshBuilder.AddVertex({ x0, y1 }, { g->uv0.x, g->uv1.y }, col);

                                s_GlobalMeshBuilder.AddTriangleIndices(vidx, vidx + 1, vidx + 2);
                                s_GlobalMeshBuilder.AddTriangleIndices(vidx, vidx + 2, vidx + 3);

                                curX += g->xAdvance;
                            }
                        }
                        };

                    if (cmd.textShadowColor.a > 0.001f) {
                        DrawString({ cmd.pos.x + globalStyle.TextShadowOffsetX, cmd.pos.y + globalStyle.TextShadowOffsetY }, DX9_ToD3DColor(cmd.textShadowColor));
                    }
                    if (cmd.textOutline && cmd.textOutlineColor.a > 0.001f) {
                        D3DCOLOR outlineCol = DX9_ToD3DColor(cmd.textOutlineColor);
                        DrawString({ cmd.pos.x - 1.f, cmd.pos.y }, outlineCol);
                        DrawString({ cmd.pos.x + 1.f, cmd.pos.y }, outlineCol);
                        DrawString({ cmd.pos.x, cmd.pos.y - 1.f }, outlineCol);
                        DrawString({ cmd.pos.x, cmd.pos.y + 1.f }, outlineCol);
                    }
                    DrawString(cmd.pos, d3dCol);
                    break;
                }
                }
            }
        }

        if (s_GlobalMeshBuilder.Vertices.empty() || s_GlobalMeshBuilder.Indices.empty()) {
            return;
        }

        // 第四阶段：整帧单次上传顶点与索引数据 (Single Lock-Unlock)
        if (!g_DX9_VB || g_DX9_VertexBufferSize < s_GlobalMeshBuilder.Vertices.size()) {
            if (g_DX9_VB) { g_DX9_VB->Release(); g_DX9_VB = nullptr; }
            g_DX9_VertexBufferSize = s_GlobalMeshBuilder.Vertices.size() + INITIAL_VERTEX_BUFFER_SIZE;
            if (FAILED(g_DX9_Device->CreateVertexBuffer(
                static_cast<UINT>(g_DX9_VertexBufferSize * sizeof(DX9Vertex)),
                D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
                D3DFVF_DX9VERTEX,
                D3DPOOL_DEFAULT,
                &g_DX9_VB,
                nullptr))) {
                return;
            }
        }

        if (!g_DX9_IB || g_DX9_IndexBufferSize < s_GlobalMeshBuilder.Indices.size()) {
            if (g_DX9_IB) { g_DX9_IB->Release(); g_DX9_IB = nullptr; }
            g_DX9_IndexBufferSize = s_GlobalMeshBuilder.Indices.size() + INITIAL_INDEX_BUFFER_SIZE;
            if (FAILED(g_DX9_Device->CreateIndexBuffer(
                static_cast<UINT>(g_DX9_IndexBufferSize * sizeof(uint32_t)),
                D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
                D3DFMT_INDEX32,
                D3DPOOL_DEFAULT,
                &g_DX9_IB,
                nullptr))) {
                return;
            }
        }

        void* vData = nullptr;
        if (SUCCEEDED(g_DX9_VB->Lock(0, static_cast<UINT>(s_GlobalMeshBuilder.Vertices.size() * sizeof(DX9Vertex)), &vData, D3DLOCK_DISCARD))) {
            memcpy(vData, s_GlobalMeshBuilder.Vertices.data(), s_GlobalMeshBuilder.Vertices.size() * sizeof(DX9Vertex));
            g_DX9_VB->Unlock();
        }

        void* iData = nullptr;
        if (SUCCEEDED(g_DX9_IB->Lock(0, static_cast<UINT>(s_GlobalMeshBuilder.Indices.size() * sizeof(uint32_t)), &iData, D3DLOCK_DISCARD))) {
            memcpy(iData, s_GlobalMeshBuilder.Indices.data(), s_GlobalMeshBuilder.Indices.size() * sizeof(uint32_t));
            g_DX9_IB->Unlock();
        }

        // 第五阶段：设置渲染状态
        D3DVIEWPORT9 vp{};
        vp.X = 0;
        vp.Y = 0;
        vp.Width = static_cast<DWORD>(drawData->DisplaySize.x);
        vp.Height = static_cast<DWORD>(drawData->DisplaySize.y);
        vp.MinZ = 0.0f;
        vp.MaxZ = 1.0f;
        g_DX9_Device->SetViewport(&vp);

        g_DX9_Device->SetPixelShader(g_DX9_PixelShader);
        g_DX9_Device->SetVertexShader(nullptr);
        g_DX9_Device->SetFVF(D3DFVF_DX9VERTEX);
        g_DX9_Device->SetStreamSource(0, g_DX9_VB, 0, sizeof(DX9Vertex));
        g_DX9_Device->SetIndices(g_DX9_IB);

        g_DX9_Device->SetRenderState(D3DRS_LIGHTING, FALSE);
        g_DX9_Device->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_DX9_Device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        g_DX9_Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        g_DX9_Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        g_DX9_Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        g_DX9_Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        g_DX9_Device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        g_DX9_Device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);

        g_DX9_Device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        g_DX9_Device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        g_DX9_Device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        g_DX9_Device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

        // 第六阶段：依序遍历合并批次进行绘制并应用动态 Gamma
        float lastBatchGamma = -1.0f;
        for (const auto& batch : s_GlobalMeshBuilder.Batches) {
            if (batch.indexCount == 0) continue;
            if (std::abs(batch.gamma - lastBatchGamma) >= DX9_MIN_GAMMA_DIFF) {
                lastBatchGamma = batch.gamma;
                float invGamma = (batch.gamma > DX9_MIN_GAMMA_DIFF) ? (1.0f / batch.gamma) : (1.0f / DX9_DEFAULT_GAMMA);
                float gammaData[4] = { invGamma, batch.gamma, 0.0f, 0.0f };
                g_DX9_Device->SetPixelShaderConstantF(0, gammaData, 1);
            }

            g_DX9_Device->SetScissorRect(&batch.scissorRect);
            IDirect3DTexture9* bindTex = batch.pTexture ? batch.pTexture : g_DX9_WhiteTexture;
            g_DX9_Device->SetTexture(0, bindTex);
            g_DX9_Device->DrawIndexedPrimitive(
                D3DPT_TRIANGLELIST,
                0,
                0,
                static_cast<UINT>(s_GlobalMeshBuilder.Vertices.size()),
                batch.startIndex,
                batch.indexCount / 3
            );
        }
    }
} // namespace Shadow