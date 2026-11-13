#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

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

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

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
    constexpr uint32_t SRV_DESCRIPTOR_HEAP_CAPACITY = 512;
    constexpr uint32_t MAX_FRAME_CONTEXTS = 4;
    constexpr float DX12_DEFAULT_GAMMA = 1.0f;
    constexpr float DX12_MIN_GAMMA_DIFF = 0.0001f;

    // DX12 顶点格式
    struct DX12Vertex {
        Vec2 pos;
        Vec2 uv;
        Color col;
    };

    // 正交投影常量缓冲结构 (16字节对齐)
    struct alignas(16) DX12ConstantBuffer {
        float mvp[4][4];
    };

    // 绘制批次描述项
    struct DX12DrawBatch {
        uint32_t startIndex = 0;
        uint32_t indexCount = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuSrvHandle{};
        D3D12_RECT scissorRect{};
        float gamma = DX12_DEFAULT_GAMMA;
    };

    // 字形度量缓存项
    struct DX12Glyph {
        float width;       // 烘焙位图宽度
        float height;      // 烘焙位图高度
        float xOff;        // X 轴基线偏移
        float yOff;        // Y 轴基线偏移
        float xAdvance;    // 字符水平推进量
        Vec2 uv0;          // 图集左上角 UV
        Vec2 uv1;          // 图集右下角 UV
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

    // 字体资源封装结构体 (支持多尺寸动态无损矢量光栅化)
    struct DX12FontResource {
        stbtt_fontinfo FontInfo{};
        std::vector<unsigned char> FontBuffer;
        int BaseFontSize = DEFAULT_FALLBACK_FONT_SIZE;
        int Ascent = 0;
        int Descent = 0;
        int LineGap = 0;

        int AtlasWidth = ATLAS_DEFAULT_WIDTH;
        int AtlasHeight = ATLAS_DEFAULT_HEIGHT;
        std::vector<unsigned char> AtlasRgbaPixels;
        int CurrentShelfX = ATLAS_PADDING;
        int CurrentShelfY = ATLAS_PADDING;
        int CurrentShelfH = 0;
        bool AtlasDirty = false;

        ID3D12Resource* AtlasTexture = nullptr;
        ID3D12Resource* UploadBuffer = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE CpuSrvHandle{};
        D3D12_GPU_DESCRIPTOR_HANDLE GpuSrvHandle{};
        uint32_t DescriptorIndex = 0;
        std::unordered_map<GlyphKey, DX12Glyph, GlyphKeyHasher> Glyphs;

        ~DX12FontResource() {
            if (UploadBuffer) { UploadBuffer->Release(); UploadBuffer = nullptr; }
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

    // 自定义纹理封装结构体
    struct DX12TextureObject {
        ID3D12Resource* TextureResource = nullptr;
        ID3D12Resource* UploadResource = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE CpuSrvHandle{};
        D3D12_GPU_DESCRIPTOR_HANDLE GpuSrvHandle{};
        uint32_t DescriptorIndex = 0;
        bool NeedsUpload = false;
        UINT Width = 0;
        UINT Height = 0;
        UINT RowPitch = 0;

        ~DX12TextureObject() {
            if (UploadResource) { UploadResource->Release(); UploadResource = nullptr; }
            if (TextureResource) { TextureResource->Release(); TextureResource = nullptr; }
        }
    };

    // 帧缓冲上下文结构
    struct DX12FrameContext {
        ID3D12CommandAllocator* CommandAllocator = nullptr;
        ID3D12Resource* VertexBuffer = nullptr;
        ID3D12Resource* IndexBuffer = nullptr;
        size_t VertexBufferSize = INITIAL_VERTEX_BUFFER_SIZE;
        size_t IndexBufferSize = INITIAL_INDEX_BUFFER_SIZE;
        uint8_t* VertexMappedData = nullptr;
        uint8_t* IndexMappedData = nullptr;
    };

    // 后端全局状态
    inline HWND g_DX12_Hwnd = nullptr;
    inline ID3D12Device* g_DX12_Device = nullptr;
    inline ID3D12CommandQueue* g_DX12_CommandQueue = nullptr;
    inline DXGI_FORMAT g_DX12_RtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    inline uint32_t g_DX12_NumFramesInFlight = 2;
    inline DX12FrameContext g_DX12_Frames[MAX_FRAME_CONTEXTS]{};

    inline ID3D12RootSignature* g_DX12_RootSignature = nullptr;
    inline ID3D12PipelineState* g_DX12_PipelineState = nullptr;

    inline ID3D12DescriptorHeap* g_DX12_SrvDescriptorHeap = nullptr;
    inline uint32_t g_DX12_SrvDescriptorSize = 0;
    inline uint32_t g_DX12_SrvDescriptorAllocated = 0;

    // 独立可信的 1x1 纯白纹理及其上传缓冲 (完全异步伴随主帧，零阻塞)
    inline ID3D12Resource* g_DX12_WhiteTexture = nullptr;
    inline ID3D12Resource* g_DX12_WhiteUploadBuffer = nullptr;
    inline D3D12_CPU_DESCRIPTOR_HANDLE g_DX12_WhiteSrvCpuHandle{};
    inline D3D12_GPU_DESCRIPTOR_HANDLE g_DX12_WhiteSrvGpuHandle{};
    inline bool g_DX12_WhiteTextureUploaded = false;

    inline std::vector<std::unique_ptr<DX12FontResource>> g_DX12_FontPool;
    inline std::unordered_map<FontFileCacheKey, ShadowFont, FontFileCacheHasher> g_DX12_FileFontCache;
    inline std::unordered_map<FontBufferCacheKey, ShadowFont, FontBufferCacheHasher> g_DX12_BufferFontCache;

    inline std::vector<std::unique_ptr<DX12TextureObject>> g_DX12_TexturePool;
    inline std::unordered_map<std::wstring, ShadowTexture> g_DX12_FileTextureCache;
    inline std::unordered_map<const void*, ShadowTexture> g_DX12_BufferTextureCache;

    inline double g_DX12_Time = 0.0;
    inline LARGE_INTEGER g_DX12_PerfFrequency = { 0 };
    inline LARGE_INTEGER g_DX12_PerfCounter = { 0 };

    // 描述符分配函数
    inline bool DX12_AllocateSrvDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle, uint32_t* outIndex) {
        if (!g_DX12_SrvDescriptorHeap || g_DX12_SrvDescriptorAllocated >= SRV_DESCRIPTOR_HEAP_CAPACITY) {
            return false;
        }

        uint32_t index = g_DX12_SrvDescriptorAllocated++;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = g_DX12_SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        cpuHandle.ptr += static_cast<SIZE_T>(index) * g_DX12_SrvDescriptorSize;

        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = g_DX12_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        gpuHandle.ptr += static_cast<UINT64>(index) * g_DX12_SrvDescriptorSize;

        if (outCpuHandle) *outCpuHandle = cpuHandle;
        if (outGpuHandle) *outGpuHandle = gpuHandle;
        if (outIndex) *outIndex = index;
        return true;
    }

    // UTF-8 解码为 UTF-32
    inline uint32_t DX12_DecodeUTF8(const char*& text, const char* end) {
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

    // 按目标像素高度光栅化字形
    inline const DX12Glyph* DX12_GetOrBakeGlyph(DX12FontResource* font, uint32_t codepoint, int pixelHeight) {
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
            std::fill(font->AtlasRgbaPixels.begin(), font->AtlasRgbaPixels.end(), static_cast<unsigned char>(0));
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
                    size_t dstIdx = static_cast<size_t>(((atlasY + r) * font->AtlasWidth + (atlasX + c)) * RGBA_BYTES_PER_PIXEL);
                    font->AtlasRgbaPixels[dstIdx + 0] = COLOR_CHANNEL_MAX;
                    font->AtlasRgbaPixels[dstIdx + 1] = COLOR_CHANNEL_MAX;
                    font->AtlasRgbaPixels[dstIdx + 2] = COLOR_CHANNEL_MAX;
                    font->AtlasRgbaPixels[dstIdx + 3] = alpha;
                }
            }
        }

        font->CurrentShelfX += glyphWidth + ATLAS_PADDING;
        if (glyphHeight > font->CurrentShelfH) {
            font->CurrentShelfH = glyphHeight;
        }

        DX12Glyph glyph{};
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

    // 提交 RGBA 图集到 GPU 显存
    inline void DX12_UpdateAtlasTexture(DX12FontResource* font, ID3D12GraphicsCommandList* cmdList) {
        if (!font || !font->AtlasDirty || !g_DX12_Device || !cmdList) return;

        UINT rowPitch = (font->AtlasWidth * RGBA_BYTES_PER_PIXEL + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
        UINT uploadBufferSize = rowPitch * font->AtlasHeight;

        if (!font->AtlasTexture) {
            D3D12_HEAP_PROPERTIES defaultHeapProps{};
            defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

            D3D12_RESOURCE_DESC texDesc{};
            texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            texDesc.Width = font->AtlasWidth;
            texDesc.Height = font->AtlasHeight;
            texDesc.DepthOrArraySize = 1;
            texDesc.MipLevels = 1;
            texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texDesc.SampleDesc.Count = 1;
            texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

            if (FAILED(g_DX12_Device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&font->AtlasTexture)))) {
                return;
            }

            D3D12_HEAP_PROPERTIES uploadHeapProps{};
            uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC bufDesc{};
            bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            bufDesc.Width = uploadBufferSize;
            bufDesc.Height = 1;
            bufDesc.DepthOrArraySize = 1;
            bufDesc.MipLevels = 1;
            bufDesc.Format = DXGI_FORMAT_UNKNOWN;
            bufDesc.SampleDesc.Count = 1;
            bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            if (FAILED(g_DX12_Device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&font->UploadBuffer)))) {
                return;
            }

            if (!DX12_AllocateSrvDescriptor(&font->CpuSrvHandle, &font->GpuSrvHandle, &font->DescriptorIndex)) {
                return;
            }

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels = 1;

            g_DX12_Device->CreateShaderResourceView(font->AtlasTexture, &srvDesc, font->CpuSrvHandle);
        }
        else {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = font->AtlasTexture;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            cmdList->ResourceBarrier(1, &barrier);
        }

        void* mappedData = nullptr;
        D3D12_RANGE readRange{ 0, 0 };
        if (SUCCEEDED(font->UploadBuffer->Map(0, &readRange, &mappedData))) {
            for (int y = 0; y < font->AtlasHeight; ++y) {
                memcpy(static_cast<uint8_t*>(mappedData) + y * rowPitch,
                    font->AtlasRgbaPixels.data() + y * font->AtlasWidth * RGBA_BYTES_PER_PIXEL,
                    font->AtlasWidth * RGBA_BYTES_PER_PIXEL);
            }
            font->UploadBuffer->Unmap(0, nullptr);
        }

        D3D12_TEXTURE_COPY_LOCATION srcLocation{};
        srcLocation.pResource = font->UploadBuffer;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint.Offset = 0;
        srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srcLocation.PlacedFootprint.Footprint.Width = font->AtlasWidth;
        srcLocation.PlacedFootprint.Footprint.Height = font->AtlasHeight;
        srcLocation.PlacedFootprint.Footprint.Depth = 1;
        srcLocation.PlacedFootprint.Footprint.RowPitch = rowPitch;

        D3D12_TEXTURE_COPY_LOCATION dstLocation{};
        dstLocation.pResource = font->AtlasTexture;
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.SubresourceIndex = 0;

        cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = font->AtlasTexture;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmdList->ResourceBarrier(1, &barrier);

        font->AtlasDirty = false;
    }

    // 准确文本尺寸测量
    inline Vec2 DX12_MeasureText(ShadowFont shadowFont, float fontScale, bool noSDF, std::string_view text) {
        if (!shadowFont.NativePtr || text.empty()) {
            return { 0.f, 0.f };
        }

        auto* font = static_cast<DX12FontResource*>(shadowFont.NativePtr);
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
            uint32_t cp = DX12_DecodeUTF8(p, end);
            if (cp == 0) break;
            const DX12Glyph* g = DX12_GetOrBakeGlyph(font, cp, targetPixelHeight);
            if (g) {
                width += g->xAdvance;
            }
        }

        return { width, fontLineHeight };
    }

    // 从内存装载纹理
    inline ShadowTexture DX12_LoadTextureFromBuffer(const unsigned char* BufferData, size_t BufferSize) {
        if (!BufferData || BufferSize == 0 || !g_DX12_Device) return ShadowTexture{};

        auto it = g_DX12_BufferTextureCache.find(BufferData);
        if (it != g_DX12_BufferTextureCache.end()) return it->second;

        int width = 0, height = 0, channels = 0;
        unsigned char* image = nullptr;

        if (BufferSize == 4) {
            width = 1;
            height = 1;
            channels = 4;
            image = static_cast<unsigned char*>(malloc(4));
            if (image) memcpy(image, BufferData, 4);
        }
        else {
            image = stbi_load_from_memory(BufferData, static_cast<int>(BufferSize), &width, &height, &channels, 4);
        }

        if (!image) return ShadowTexture{};

        UINT rowPitch = (width * RGBA_BYTES_PER_PIXEL + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
        UINT uploadBufferSize = rowPitch * height;

        auto texObj = std::make_unique<DX12TextureObject>();
        texObj->Width = width;
        texObj->Height = height;
        texObj->RowPitch = rowPitch;

        D3D12_HEAP_PROPERTIES defaultHeapProps{};
        defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC texDesc{};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        if (FAILED(g_DX12_Device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texObj->TextureResource)))) {
            free(image);
            return ShadowTexture{};
        }

        D3D12_HEAP_PROPERTIES uploadHeapProps{};
        uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC bufDesc{};
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Width = uploadBufferSize;
        bufDesc.Height = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels = 1;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        if (FAILED(g_DX12_Device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&texObj->UploadResource)))) {
            free(image);
            return ShadowTexture{};
        }

        void* mappedData = nullptr;
        D3D12_RANGE readRange{ 0, 0 };
        if (SUCCEEDED(texObj->UploadResource->Map(0, &readRange, &mappedData))) {
            for (int y = 0; y < height; ++y) {
                memcpy(static_cast<uint8_t*>(mappedData) + y * rowPitch, image + y * width * RGBA_BYTES_PER_PIXEL, width * RGBA_BYTES_PER_PIXEL);
            }
            texObj->UploadResource->Unmap(0, nullptr);
        }
        free(image);

        if (!DX12_AllocateSrvDescriptor(&texObj->CpuSrvHandle, &texObj->GpuSrvHandle, &texObj->DescriptorIndex)) {
            return ShadowTexture{};
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        g_DX12_Device->CreateShaderResourceView(texObj->TextureResource, &srvDesc, texObj->CpuSrvHandle);

        texObj->NeedsUpload = true;

        ShadowTexture tex{ texObj.get() };
        g_DX12_BufferTextureCache[BufferData] = tex;
        g_DX12_TexturePool.push_back(std::move(texObj));
        return tex;
    }

    // 从文件装载纹理
    inline ShadowTexture DX12_LoadTextureFromFile(const wchar_t* FilePath) {
        if (!FilePath || !g_DX12_Device) return ShadowTexture{};

        auto it = g_DX12_FileTextureCache.find(FilePath);
        if (it != g_DX12_FileTextureCache.end()) return it->second;

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

        ShadowTexture tex = DX12_LoadTextureFromBuffer(buffer.data(), buffer.size());
        if (tex) {
            g_DX12_FileTextureCache[FilePath] = tex;
        }
        return tex;
    }

    // 从内存装载字体
    inline ShadowFont DX12_LoadFontFromBuffer(const unsigned char* BufferData, size_t BufferSize, int32_t FontSize = DEFAULT_FONT_SIZE) {
        if (!BufferData || BufferSize == 0) return ShadowFont{};

        const auto& style = Shadow::GetStyle();
        int targetFontSize = (FontSize >= MIN_VALID_FONT_SIZE) ? FontSize : static_cast<int>(style.FallbackFontSize);
        if (targetFontSize <= 0) targetFontSize = DEFAULT_FALLBACK_FONT_SIZE;

        FontBufferCacheKey key{ BufferData, targetFontSize };
        auto it = g_DX12_BufferFontCache.find(key);
        if (it != g_DX12_BufferFontCache.end()) return it->second;

        auto fontRes = std::make_unique<DX12FontResource>();
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

        fontRes->AtlasRgbaPixels.assign(static_cast<size_t>(fontRes->AtlasWidth * fontRes->AtlasHeight * RGBA_BYTES_PER_PIXEL), 0);

        for (uint32_t c = 32; c <= 126; ++c) {
            DX12_GetOrBakeGlyph(fontRes.get(), c, targetFontSize);
        }

        ShadowFont shadowFont{ fontRes.get() };
        g_DX12_BufferFontCache[key] = shadowFont;
        g_DX12_FontPool.push_back(std::move(fontRes));
        return shadowFont;
    }

    // 从文件装载字体
    inline ShadowFont DX12_LoadFontFromFile(const wchar_t* FilePath, int32_t FontSize = DEFAULT_FONT_SIZE) {
        if (!FilePath) return ShadowFont{};

        const auto& style = Shadow::GetStyle();
        int targetFontSize = (FontSize >= MIN_VALID_FONT_SIZE) ? FontSize : static_cast<int>(style.FallbackFontSize);
        if (targetFontSize <= 0) targetFontSize = DEFAULT_FALLBACK_FONT_SIZE;

        FontFileCacheKey key{ FilePath, targetFontSize };
        auto it = g_DX12_FileFontCache.find(key);
        if (it != g_DX12_FileFontCache.end()) return it->second;

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

        auto fontRes = std::make_unique<DX12FontResource>();
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

        fontRes->AtlasRgbaPixels.assign(static_cast<size_t>(fontRes->AtlasWidth * fontRes->AtlasHeight * RGBA_BYTES_PER_PIXEL), 0);

        for (uint32_t c = 32; c <= 126; ++c) {
            DX12_GetOrBakeGlyph(fontRes.get(), c, targetFontSize);
        }

        ShadowFont shadowFont{ fontRes.get() };
        g_DX12_FileFontCache[key] = shadowFont;
        g_DX12_FontPool.push_back(std::move(fontRes));
        return shadowFont;
    }

    inline void DX12_RegisterBackendCallbacks() {
        Shadow::g_Ctx.TextSizeFn = &DX12_MeasureText;
        Shadow::g_Ctx.LoadTextureFromBufferCallback = &DX12_LoadTextureFromBuffer;
        Shadow::g_Ctx.LoadTextureFromFileCallback = &DX12_LoadTextureFromFile;
        Shadow::g_Ctx.LoadFontFromFileCallback = &DX12_LoadFontFromFile;
        Shadow::g_Ctx.LoadFontFromBufferCallback = &DX12_LoadFontFromBuffer;
        Shadow::g_Ctx.BackendSupportsTextPixelClipping = true;
    }

    // 创建管线与设备对象 (完全移除 CPU 阻塞等待，改为随主帧平滑上传)
    inline bool DX12_CreateDeviceObjects() {
        if (!g_DX12_Device) return false;

        // 1. 创建 SRV 描述符堆
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = SRV_DESCRIPTOR_HEAP_CAPACITY;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        if (FAILED(g_DX12_Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&g_DX12_SrvDescriptorHeap)))) {
            return false;
        }
        g_DX12_SrvDescriptorSize = g_DX12_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        g_DX12_SrvDescriptorAllocated = 0;

        // 2. 创建 1x1 纯白纹理与 Upload 缓冲区 (准备随首帧渲染命令流上传)
        D3D12_HEAP_PROPERTIES defaultHeapProps{};
        defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC whiteTexDesc{};
        whiteTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        whiteTexDesc.Width = 1;
        whiteTexDesc.Height = 1;
        whiteTexDesc.DepthOrArraySize = 1;
        whiteTexDesc.MipLevels = 1;
        whiteTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        whiteTexDesc.SampleDesc.Count = 1;
        whiteTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        if (FAILED(g_DX12_Device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &whiteTexDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&g_DX12_WhiteTexture)))) {
            return false;
        }

        D3D12_HEAP_PROPERTIES uploadHeapProps{};
        uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC whiteBufDesc{};
        whiteBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        whiteBufDesc.Width = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
        whiteBufDesc.Height = 1;
        whiteBufDesc.DepthOrArraySize = 1;
        whiteBufDesc.MipLevels = 1;
        whiteBufDesc.Format = DXGI_FORMAT_UNKNOWN;
        whiteBufDesc.SampleDesc.Count = 1;
        whiteBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        if (FAILED(g_DX12_Device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &whiteBufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_DX12_WhiteUploadBuffer)))) {
            return false;
        }

        void* pMappedWhite = nullptr;
        D3D12_RANGE readRange{ 0, 0 };
        if (SUCCEEDED(g_DX12_WhiteUploadBuffer->Map(0, &readRange, &pMappedWhite))) {
            uint32_t whitePixel = 0xFFFFFFFF;
            memcpy(pMappedWhite, &whitePixel, sizeof(uint32_t));
            g_DX12_WhiteUploadBuffer->Unmap(0, nullptr);
        }

        // Slot 0 永久绑定纯白贴图
        DX12_AllocateSrvDescriptor(&g_DX12_WhiteSrvCpuHandle, &g_DX12_WhiteSrvGpuHandle, nullptr);
        D3D12_SHADER_RESOURCE_VIEW_DESC whiteSrvDesc{};
        whiteSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        whiteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        whiteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        whiteSrvDesc.Texture2D.MipLevels = 1;
        g_DX12_Device->CreateShaderResourceView(g_DX12_WhiteTexture, &whiteSrvDesc, g_DX12_WhiteSrvCpuHandle);
        g_DX12_WhiteTextureUploaded = false;

        // 3. 创建 Root Signature (3 个参数：矩阵根常量、SRV描述符表、Gamma根常量)
        D3D12_DESCRIPTOR_RANGE descRange{};
        descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        descRange.NumDescriptors = 1;
        descRange.BaseShaderRegister = 0;
        descRange.RegisterSpace = 0;
        descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER rootParams[3]{};
        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParams[0].Constants.ShaderRegister = 0;
        rootParams[0].Constants.RegisterSpace = 0;
        rootParams[0].Constants.Num32BitValues = 16;
        rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[1].DescriptorTable.pDescriptorRanges = &descRange;
        rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParams[2].Constants.ShaderRegister = 0;
        rootParams[2].Constants.RegisterSpace = 0;
        rootParams[2].Constants.Num32BitValues = 1;
        rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 0;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = 0.0f;
        samplerDesc.ShaderRegister = 0;
        samplerDesc.RegisterSpace = 0;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
        rootSigDesc.NumParameters = 3;
        rootSigDesc.pParameters = rootParams;
        rootSigDesc.NumStaticSamplers = 1;
        rootSigDesc.pStaticSamplers = &samplerDesc;
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* signatureBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;
        if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob))) {
            if (errorBlob) errorBlob->Release();
            return false;
        }

        if (FAILED(g_DX12_Device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&g_DX12_RootSignature)))) {
            signatureBlob->Release();
            return false;
        }
        signatureBlob->Release();

        // 4. 编译 Shader
        const char* vertexShaderHLSL =
            "cbuffer ProjectionBuffer : register(b0) {\n"
            "    float4x4 ProjectionMatrix;\n"
            "};\n"
            "struct VS_INPUT {\n"
            "    float2 pos : POSITION;\n"
            "    float2 uv  : TEXCOORD0;\n"
            "    float4 col : COLOR0;\n"
            "};\n"
            "struct PS_INPUT {\n"
            "    float4 pos : SV_POSITION;\n"
            "    float2 uv  : TEXCOORD0;\n"
            "    float4 col : COLOR0;\n"
            "};\n"
            "PS_INPUT main(VS_INPUT input) {\n"
            "    PS_INPUT output;\n"
            "    output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\n"
            "    output.col = input.col;\n"
            "    output.uv  = input.uv;\n"
            "    return output;\n"
            "}\n";

        const char* pixelShaderHLSL =
            "cbuffer GammaBuffer : register(b0) {\n"
            "    float InvGamma;\n"
            "};\n"
            "struct PS_INPUT {\n"
            "    float4 pos : SV_POSITION;\n"
            "    float2 uv  : TEXCOORD0;\n"
            "    float4 col : COLOR0;\n"
            "};\n"
            "SamplerState Sampler : register(s0);\n"
            "Texture2D Texture : register(t0);\n"
            "float4 main(PS_INPUT input) : SV_Target {\n"
            "    float4 texColor = Texture.Sample(Sampler, input.uv);\n"
            "    float4 outColor = input.col * texColor;\n"
            "    outColor.rgb = pow(max(outColor.rgb, 0.0f), InvGamma);\n"
            "    return outColor;\n"
            "}\n";

        ID3DBlob* vsBlob = nullptr;
        if (FAILED(D3DCompile(vertexShaderHLSL, strlen(vertexShaderHLSL), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, nullptr))) {
            return false;
        }

        ID3DBlob* psBlob = nullptr;
        if (FAILED(D3DCompile(pixelShaderHLSL, strlen(pixelShaderHLSL), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, nullptr))) {
            vsBlob->Release();
            return false;
        }

        D3D12_INPUT_ELEMENT_DESC inputElements[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(DX12Vertex, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(DX12Vertex, uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DX12Vertex, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        // 5. 创建 PSO
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = g_DX12_RootSignature;
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        psoDesc.InputLayout = { inputElements, 3 };
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = g_DX12_RtvFormat;
        psoDesc.SampleDesc.Count = 1;

        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;

        psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        HRESULT hr = g_DX12_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_DX12_PipelineState));
        vsBlob->Release();
        psBlob->Release();
        if (FAILED(hr)) return false;

        return true;
    }

    inline void DX12_InvalidateDeviceObjects() {
        if (g_DX12_WhiteUploadBuffer) { g_DX12_WhiteUploadBuffer->Release(); g_DX12_WhiteUploadBuffer = nullptr; }
        if (g_DX12_WhiteTexture) { g_DX12_WhiteTexture->Release(); g_DX12_WhiteTexture = nullptr; }
        g_DX12_WhiteTextureUploaded = false;

        if (g_DX12_PipelineState) { g_DX12_PipelineState->Release(); g_DX12_PipelineState = nullptr; }
        if (g_DX12_RootSignature) { g_DX12_RootSignature->Release(); g_DX12_RootSignature = nullptr; }
        if (g_DX12_SrvDescriptorHeap) { g_DX12_SrvDescriptorHeap->Release(); g_DX12_SrvDescriptorHeap = nullptr; }

        for (uint32_t i = 0; i < g_DX12_NumFramesInFlight; ++i) {
            auto& fc = g_DX12_Frames[i];
            if (fc.VertexBuffer) {
                fc.VertexBuffer->Unmap(0, nullptr);
                fc.VertexBuffer->Release();
                fc.VertexBuffer = nullptr;
            }
            if (fc.IndexBuffer) {
                fc.IndexBuffer->Unmap(0, nullptr);
                fc.IndexBuffer->Release();
                fc.IndexBuffer = nullptr;
            }
            if (fc.CommandAllocator) {
                fc.CommandAllocator->Release();
                fc.CommandAllocator = nullptr;
            }
            fc.VertexMappedData = nullptr;
            fc.IndexMappedData = nullptr;
        }

        for (auto& f : g_DX12_FontPool) {
            if (f->UploadBuffer) { f->UploadBuffer->Release(); f->UploadBuffer = nullptr; }
            if (f->AtlasTexture) { f->AtlasTexture->Release(); f->AtlasTexture = nullptr; }
            f->AtlasDirty = true;
        }

        g_DX12_TexturePool.clear();
        g_DX12_FileTextureCache.clear();
        g_DX12_BufferTextureCache.clear();
    }

    inline bool DX12_Init(HWND hWnd, ID3D12Device* device, ID3D12CommandQueue* commandQueue, uint32_t numFramesInFlight, DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM) {
        g_DX12_Hwnd = hWnd;
        g_DX12_Device = device;
        g_DX12_CommandQueue = commandQueue;
        g_DX12_NumFramesInFlight = std::clamp(numFramesInFlight, 1u, MAX_FRAME_CONTEXTS);
        g_DX12_RtvFormat = rtvFormat;

        QueryPerformanceFrequency(&g_DX12_PerfFrequency);
        QueryPerformanceCounter(&g_DX12_PerfCounter);

        DX12_RegisterBackendCallbacks();

        if (!DX12_CreateDeviceObjects()) {
            return false;
        }

        for (uint32_t i = 0; i < g_DX12_NumFramesInFlight; ++i) {
            if (FAILED(g_DX12_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_DX12_Frames[i].CommandAllocator)))) {
                return false;
            }
        }

        return true;
    }

    inline void DX12_Shutdown() {
        DX12_InvalidateDeviceObjects();
        g_DX12_FontPool.clear();
        g_DX12_FileFontCache.clear();
        g_DX12_BufferFontCache.clear();
        g_DX12_Device = nullptr;
        g_DX12_CommandQueue = nullptr;
        g_DX12_Hwnd = nullptr;
    }

    inline void DX12_NewFrame() {
        if (!g_DX12_PipelineState) {
            DX12_CreateDeviceObjects();
        }

        DX12_RegisterBackendCallbacks();

        LARGE_INTEGER currentCounter;
        QueryPerformanceCounter(&currentCounter);
        double dt = static_cast<double>(currentCounter.QuadPart - g_DX12_PerfCounter.QuadPart) / static_cast<double>(g_DX12_PerfFrequency.QuadPart);
        g_DX12_PerfCounter = currentCounter;
        g_DX12_Time += dt;

        RECT rect{ 0, 0, 0, 0 };
        if (g_DX12_Hwnd) {
            GetClientRect(g_DX12_Hwnd, &rect);
        }

        ShadowIO& io = Shadow::GetIO();
        io.DisplaySize = { static_cast<float>(rect.right - rect.left), static_cast<float>(rect.bottom - rect.top) };
        io.DeltaTime = static_cast<float>(dt);
        io.BackendRenderer = BackendRenderer::DX12;

        Shadow::g_Ctx.RealTimeSeconds = g_DX12_Time;
        Shadow::g_Ctx.DeltaTime = dt;

        if (!Shadow::g_Ctx.DefaultFont) {
            Shadow::g_Ctx.DefaultFont = DX12_LoadFontFromFile(L"C:\\Windows\\Fonts\\msyh.ttc", DEFAULT_FALLBACK_FONT_SIZE);
            if (!Shadow::g_Ctx.DefaultFont) {
                Shadow::g_Ctx.DefaultFont = DX12_LoadFontFromFile(L"C:\\Windows\\Fonts\\simhei.ttf", DEFAULT_FALLBACK_FONT_SIZE);
            }
            if (!Shadow::g_Ctx.DefaultFont) {
                Shadow::g_Ctx.DefaultFont = DX12_LoadFontFromFile(L"C:\\Windows\\Fonts\\arial.ttf", DEFAULT_FALLBACK_FONT_SIZE);
            }
        }
    }

    // 几何生成与批次合并构建器
    struct DX12MeshBuilder {
        std::vector<DX12Vertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<DX12DrawBatch> Batches;

        void Clear() {
            Vertices.clear();
            Indices.clear();
            Batches.clear();
        }

        void EnsureBatch(D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv, const D3D12_RECT& scissor, float gamma) {
            if (!Batches.empty()) {
                auto& last = Batches.back();
                if (last.gpuSrvHandle.ptr == gpuSrv.ptr &&
                    last.scissorRect.left == scissor.left &&
                    last.scissorRect.top == scissor.top &&
                    last.scissorRect.right == scissor.right &&
                    last.scissorRect.bottom == scissor.bottom &&
                    std::abs(last.gamma - gamma) < DX12_MIN_GAMMA_DIFF) {
                    return;
                }
            }
            DX12DrawBatch newBatch{};
            newBatch.startIndex = static_cast<uint32_t>(Indices.size());
            newBatch.indexCount = 0;
            newBatch.gpuSrvHandle = gpuSrv;
            newBatch.scissorRect = scissor;
            newBatch.gamma = gamma;
            Batches.push_back(newBatch);
        }

        void AddVertex(Vec2 pos, Vec2 uv, Color col) {
            Vertices.push_back({ pos, uv, col });
        }

        void AddTriangleIndices(uint32_t i0, uint32_t i1, uint32_t i2) {
            Indices.push_back(i0);
            Indices.push_back(i1);
            Indices.push_back(i2);
            if (!Batches.empty()) {
                Batches.back().indexCount += 3;
            }
        }

        void BuildThickLine(Vec2 p0, Vec2 p1, Color col, float thickness) {
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

        // 闭合折线均匀描边 (法线均值与 Miter 尖角限幅)
        void BuildPolylineStroke(const std::vector<Vec2>& points, Color col, float thickness) {
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

        void BuildRect(Vec2 pos, Vec2 size, Color col, float thickness, float rounding, ShadowDrawFlags flags) {
            if (size.x <= 0.f || size.y <= 0.f || thickness <= 0.f) return;

            bool roundTL = (flags & ShadowDrawFlags_RoundCornersTopLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
            bool roundTR = (flags & ShadowDrawFlags_RoundCornersTopRight) && !(flags & ShadowDrawFlags_RoundCornersNone);
            bool roundBL = (flags & ShadowDrawFlags_RoundCornersBottomLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
            bool roundBR = (flags & ShadowDrawFlags_RoundCornersBottomRight) && !(flags & ShadowDrawFlags_RoundCornersNone);

            float r = std::min(rounding, std::min(size.x, size.y) * 0.5f);
            float r_tl = roundTL ? r : 0.0f;
            float r_tr = roundTR ? r : 0.0f;
            float r_br = roundBR ? r : 0.0f;
            float r_bl = roundBL ? r : 0.0f;

            std::vector<Vec2> points;
            points.reserve((CORNER_ROUNDING_SEGMENTS + 1) * 4);

            auto AppendCorner = [&](Vec2 center, float radius, float startAngle, float endAngle, Vec2 sharpCorner) {
                if (radius <= 0.0f) {
                    points.push_back(sharpCorner);
                    return;
                }
                int segs = CORNER_ROUNDING_SEGMENTS;
                for (int i = 0; i <= segs; ++i) {
                    float t = static_cast<float>(i) / static_cast<float>(segs);
                    float a = startAngle + t * (endAngle - startAngle);
                    points.push_back({ center.x + radius * std::cos(a), center.y - radius * std::sin(a) });
                }
                };

            // 顺时针构建严格外围环形点集 (TL -> TR -> BR -> BL)
            AppendCorner({ pos.x + r_tl, pos.y + r_tl }, r_tl, MATH_PI, MATH_PI * 0.5f, pos);
            AppendCorner({ pos.x + size.x - r_tr, pos.y + r_tr }, r_tr, MATH_PI * 0.5f, 0.0f, { pos.x + size.x, pos.y });
            AppendCorner({ pos.x + size.x - r_br, pos.y + size.y - r_br }, r_br, 0.0f, -MATH_PI * 0.5f, { pos.x + size.x, pos.y + size.y });
            AppendCorner({ pos.x + r_bl, pos.y + size.y - r_bl }, r_bl, -MATH_PI * 0.5f, -MATH_PI, { pos.x, pos.y + size.y });

            BuildPolylineStroke(points, col, thickness);
        }

        void BuildRectFilled(Vec2 pos, Vec2 size, Color col, float rounding, ShadowDrawFlags flags) {
            if (size.x <= 0.f || size.y <= 0.f) return;

            bool roundTL = (flags & ShadowDrawFlags_RoundCornersTopLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
            bool roundTR = (flags & ShadowDrawFlags_RoundCornersTopRight) && !(flags & ShadowDrawFlags_RoundCornersNone);
            bool roundBL = (flags & ShadowDrawFlags_RoundCornersBottomLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
            bool roundBR = (flags & ShadowDrawFlags_RoundCornersBottomRight) && !(flags & ShadowDrawFlags_RoundCornersNone);

            float r = std::min(rounding, std::min(size.x, size.y) * 0.5f);
            float r_tl = roundTL ? r : 0.0f;
            float r_tr = roundTR ? r : 0.0f;
            float r_br = roundBR ? r : 0.0f;
            float r_bl = roundBL ? r : 0.0f;

            if (r <= 0.0f || (!roundTL && !roundTR && !roundBL && !roundBR)) {
                uint32_t idx = static_cast<uint32_t>(Vertices.size());
                AddVertex(pos, { 0.f, 0.f }, col);
                AddVertex({ pos.x + size.x, pos.y }, { 1.f, 0.f }, col);
                AddVertex({ pos.x + size.x, pos.y + size.y }, { 1.f, 1.f }, col);
                AddVertex({ pos.x, pos.y + size.y }, { 0.f, 1.f }, col);

                AddTriangleIndices(idx, idx + 1, idx + 2);
                AddTriangleIndices(idx, idx + 2, idx + 3);
                return;
            }

            std::vector<Vec2> points;
            points.reserve((CORNER_ROUNDING_SEGMENTS + 1) * 4);

            auto AppendCorner = [&](Vec2 center, float radius, float startAngle, float endAngle, Vec2 sharpCorner) {
                if (radius <= 0.0f) {
                    points.push_back(sharpCorner);
                    return;
                }
                int segs = CORNER_ROUNDING_SEGMENTS;
                for (int i = 0; i <= segs; ++i) {
                    float t = static_cast<float>(i) / static_cast<float>(segs);
                    float a = startAngle + t * (endAngle - startAngle);
                    points.push_back({ center.x + radius * std::cos(a), center.y - radius * std::sin(a) });
                }
                };

            // 顺时针收集多边形外沿顶点
            AppendCorner({ pos.x + r_tl, pos.y + r_tl }, r_tl, MATH_PI, MATH_PI * 0.5f, pos);
            AppendCorner({ pos.x + size.x - r_tr, pos.y + r_tr }, r_tr, MATH_PI * 0.5f, 0.0f, { pos.x + size.x, pos.y });
            AppendCorner({ pos.x + size.x - r_br, pos.y + size.y - r_br }, r_br, 0.0f, -MATH_PI * 0.5f, { pos.x + size.x, pos.y + size.y });
            AppendCorner({ pos.x + r_bl, pos.y + size.y - r_bl }, r_bl, -MATH_PI * 0.5f, -MATH_PI, { pos.x, pos.y + size.y });

            size_t count = points.size();
            if (count < 3) return;

            uint32_t baseIdx = static_cast<uint32_t>(Vertices.size());
            for (size_t i = 0; i < count; ++i) {
                AddVertex(points[i], { 0.f, 0.f }, col);
            }

            // 凸多边形单点扇形索引缝合，零多余顶点，零中心点重叠
            for (size_t i = 2; i < count; ++i) {
                AddTriangleIndices(
                    baseIdx,
                    static_cast<uint32_t>(baseIdx + i - 1),
                    static_cast<uint32_t>(baseIdx + i)
                );
            }
        }

        void BuildCircleFilled(Vec2 center, float radius, Color col) {
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

    // 渲染后端核心执行逻辑 (frameIndex 严格与外部受 Fence 同步的 BackBuffer 槽位 1:1 绑定)
    inline void DX12_RenderDrawData(const ShadowDrawData* drawData, ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex) {
        if (!drawData || !drawData->Valid || !cmdList || !g_DX12_Device) return;
        if (drawData->DisplaySize.x <= 0.f || drawData->DisplaySize.y <= 0.f) return;

        uint32_t currentFrame = frameIndex % g_DX12_NumFramesInFlight;

        // 首帧在主命令流中执行 1x1 白纹理上传并切换屏障
        if (!g_DX12_WhiteTextureUploaded && g_DX12_WhiteTexture && g_DX12_WhiteUploadBuffer) {
            D3D12_TEXTURE_COPY_LOCATION srcLoc{};
            srcLoc.pResource = g_DX12_WhiteUploadBuffer;
            srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLoc.PlacedFootprint.Offset = 0;
            srcLoc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srcLoc.PlacedFootprint.Footprint.Width = 1;
            srcLoc.PlacedFootprint.Footprint.Height = 1;
            srcLoc.PlacedFootprint.Footprint.Depth = 1;
            srcLoc.PlacedFootprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

            D3D12_TEXTURE_COPY_LOCATION dstLoc{};
            dstLoc.pResource = g_DX12_WhiteTexture;
            dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.SubresourceIndex = 0;

            cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = g_DX12_WhiteTexture;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            cmdList->ResourceBarrier(1, &barrier);

            g_DX12_WhiteTextureUploaded = true;
        }

        // 伴随主命令列表安全提交外部纹理上传
        for (auto& tex : g_DX12_TexturePool) {
            if (tex->NeedsUpload && tex->TextureResource && tex->UploadResource) {
                D3D12_TEXTURE_COPY_LOCATION srcLoc{};
                srcLoc.pResource = tex->UploadResource;
                srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                srcLoc.PlacedFootprint.Offset = 0;
                srcLoc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                srcLoc.PlacedFootprint.Footprint.Width = tex->Width;
                srcLoc.PlacedFootprint.Footprint.Height = tex->Height;
                srcLoc.PlacedFootprint.Footprint.Depth = 1;
                srcLoc.PlacedFootprint.Footprint.RowPitch = tex->RowPitch;

                D3D12_TEXTURE_COPY_LOCATION dstLoc{};
                dstLoc.pResource = tex->TextureResource;
                dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dstLoc.SubresourceIndex = 0;

                cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

                D3D12_RESOURCE_BARRIER barrier{};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = tex->TextureResource;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                cmdList->ResourceBarrier(1, &barrier);

                tex->NeedsUpload = false;
            }
        }

        const auto& globalStyle = Shadow::GetStyle();

        // 第一阶段：预扫描并按需动态矢量光栅化字符
        for (const ShadowDrawList* cmdListPtr : drawData->CmdLists) {
            if (!cmdListPtr) continue;
            for (const auto& cmd : cmdListPtr->CmdBuffer) {
                if (cmd.type == ShadowDrawCmdType::Text && cmd.font.NativePtr && !cmd.text.empty()) {
                    auto* font = static_cast<DX12FontResource*>(cmd.font.NativePtr);
                    int targetPixelHeight = std::max(MIN_TARGET_PIXEL_HEIGHT, static_cast<int>(std::round(font->BaseFontSize * cmd.fontScale * globalStyle.FontScaleDpi)));

                    const char* p = cmd.text.data();
                    const char* end = p + cmd.text.size();
                    while (p < end) {
                        uint32_t cp = DX12_DecodeUTF8(p, end);
                        if (cp == 0) break;
                        DX12_GetOrBakeGlyph(font, cp, targetPixelHeight);
                    }
                }
            }
        }

        // 第二阶段：提交脏图集到 GPU 显存
        for (auto& font : g_DX12_FontPool) {
            if (font->AtlasDirty || !font->AtlasTexture) {
                DX12_UpdateAtlasTexture(font.get(), cmdList);
            }
        }

        // 第三阶段：收集图元并进行批次合并
        static DX12MeshBuilder s_GlobalMeshBuilder;
        s_GlobalMeshBuilder.Clear();

        for (const ShadowDrawList* cmdListPtr : drawData->CmdLists) {
            if (!cmdListPtr) continue;
            for (const auto& cmd : cmdListPtr->CmdBuffer) {
                D3D12_RECT scissor{ 0, 0, static_cast<LONG>(drawData->DisplaySize.x), static_cast<LONG>(drawData->DisplaySize.y) };
                if (cmd.clippingEnabled) {
                    scissor.left = std::max(0L, static_cast<LONG>(cmd.clipMin.x));
                    scissor.top = std::max(0L, static_cast<LONG>(cmd.clipMin.y));
                    scissor.right = std::min(static_cast<LONG>(drawData->DisplaySize.x), static_cast<LONG>(cmd.clipMax.x));
                    scissor.bottom = std::min(static_cast<LONG>(drawData->DisplaySize.y), static_cast<LONG>(cmd.clipMax.y));
                    if (scissor.right <= scissor.left || scissor.bottom <= scissor.top) continue;
                }

                D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv = g_DX12_WhiteSrvGpuHandle;
                if (cmd.texture.NativePtr) {
                    auto* pTexObj = static_cast<DX12TextureObject*>(cmd.texture.NativePtr);
                    gpuSrv = pTexObj->GpuSrvHandle;
                }

                switch (cmd.type) {
                case ShadowDrawCmdType::Line:
                    s_GlobalMeshBuilder.EnsureBatch(gpuSrv, scissor, cmd.gamma);
                    s_GlobalMeshBuilder.BuildThickLine(cmd.pos, cmd.size, cmd.color, cmd.thickness);
                    break;
                case ShadowDrawCmdType::Rect:
                    s_GlobalMeshBuilder.EnsureBatch(gpuSrv, scissor, cmd.gamma);
                    s_GlobalMeshBuilder.BuildRect(cmd.pos, cmd.size, cmd.color, cmd.thickness, cmd.rounding, cmd.flags);
                    break;
                case ShadowDrawCmdType::RectFilled:
                    s_GlobalMeshBuilder.EnsureBatch(gpuSrv, scissor, cmd.gamma);
                    s_GlobalMeshBuilder.BuildRectFilled(cmd.pos, cmd.size, cmd.color, cmd.rounding, cmd.flags);
                    break;
                case ShadowDrawCmdType::Texture: {
                    s_GlobalMeshBuilder.EnsureBatch(gpuSrv, scissor, cmd.gamma);
                    uint32_t idx = static_cast<uint32_t>(s_GlobalMeshBuilder.Vertices.size());
                    s_GlobalMeshBuilder.AddVertex(cmd.pos, { TEXTURE_DEFAULT_UV_MIN, TEXTURE_DEFAULT_UV_MIN }, cmd.color);
                    s_GlobalMeshBuilder.AddVertex({ cmd.pos.x + cmd.size.x, cmd.pos.y }, { TEXTURE_DEFAULT_UV_MAX, TEXTURE_DEFAULT_UV_MIN }, cmd.color);
                    s_GlobalMeshBuilder.AddVertex({ cmd.pos.x + cmd.size.x, cmd.pos.y + cmd.size.y }, { TEXTURE_DEFAULT_UV_MAX, TEXTURE_DEFAULT_UV_MAX }, cmd.color);
                    s_GlobalMeshBuilder.AddVertex({ cmd.pos.x, cmd.pos.y + cmd.size.y }, { TEXTURE_DEFAULT_UV_MIN, TEXTURE_DEFAULT_UV_MAX }, cmd.color);
                    s_GlobalMeshBuilder.AddTriangleIndices(idx, idx + 1, idx + 2);
                    s_GlobalMeshBuilder.AddTriangleIndices(idx, idx + 2, idx + 3);
                    break;
                }
                case ShadowDrawCmdType::CircleFilled:
                    s_GlobalMeshBuilder.EnsureBatch(gpuSrv, scissor, cmd.gamma);
                    s_GlobalMeshBuilder.BuildCircleFilled(cmd.pos, cmd.size.x, cmd.color);
                    break;
                case ShadowDrawCmdType::Triangle:
                    s_GlobalMeshBuilder.EnsureBatch(gpuSrv, scissor, cmd.gamma);
                    s_GlobalMeshBuilder.BuildThickLine(cmd.p1, cmd.p2, cmd.color, cmd.thickness);
                    s_GlobalMeshBuilder.BuildThickLine(cmd.p2, cmd.p3, cmd.color, cmd.thickness);
                    s_GlobalMeshBuilder.BuildThickLine(cmd.p3, cmd.p1, cmd.color, cmd.thickness);
                    break;
                case ShadowDrawCmdType::TriangleFilled: {
                    s_GlobalMeshBuilder.EnsureBatch(gpuSrv, scissor, cmd.gamma);
                    uint32_t idx = static_cast<uint32_t>(s_GlobalMeshBuilder.Vertices.size());
                    s_GlobalMeshBuilder.AddVertex(cmd.p1, { 0.f, 0.f }, cmd.color);
                    s_GlobalMeshBuilder.AddVertex(cmd.p2, { 0.f, 0.f }, cmd.color);
                    s_GlobalMeshBuilder.AddVertex(cmd.p3, { 0.f, 0.f }, cmd.color);
                    s_GlobalMeshBuilder.AddTriangleIndices(idx, idx + 1, idx + 2);
                    break;
                }
                case ShadowDrawCmdType::Text: {
                    if (!cmd.font.NativePtr || cmd.text.empty()) break;
                    auto* font = static_cast<DX12FontResource*>(cmd.font.NativePtr);
                    int targetPixelHeight = std::max(MIN_TARGET_PIXEL_HEIGHT, static_cast<int>(std::round(font->BaseFontSize * cmd.fontScale * globalStyle.FontScaleDpi)));

                    s_GlobalMeshBuilder.EnsureBatch(font->GpuSrvHandle, scissor, cmd.gamma);

                    auto DrawString = [&](Vec2 pos, Color col) {
                        const char* p = cmd.text.data();
                        const char* end = p + cmd.text.size();
                        float curX = pos.x;
                        float curY = pos.y;

                        while (p < end) {
                            uint32_t cp = DX12_DecodeUTF8(p, end);
                            if (cp == 0) break;

                            const DX12Glyph* g = DX12_GetOrBakeGlyph(font, cp, targetPixelHeight);
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
                        DrawString({ cmd.pos.x + globalStyle.TextShadowOffsetX, cmd.pos.y + globalStyle.TextShadowOffsetY }, cmd.textShadowColor);
                    }
                    if (cmd.textOutline && cmd.textOutlineColor.a > 0.001f) {
                        DrawString({ cmd.pos.x - 1.f, cmd.pos.y }, cmd.textOutlineColor);
                        DrawString({ cmd.pos.x + 1.f, cmd.pos.y }, cmd.textOutlineColor);
                        DrawString({ cmd.pos.x, cmd.pos.y - 1.f }, cmd.textOutlineColor);
                        DrawString({ cmd.pos.x, cmd.pos.y + 1.f }, cmd.textOutlineColor);
                    }
                    DrawString(cmd.pos, cmd.color);
                    break;
                }
                }
            }
        }

        if (s_GlobalMeshBuilder.Vertices.empty() || s_GlobalMeshBuilder.Indices.empty()) {
            return;
        }

        // 第四阶段：整帧单次上传顶点与索引数据
        auto& fc = g_DX12_Frames[currentFrame];

        size_t requiredVbSize = s_GlobalMeshBuilder.Vertices.size() * sizeof(DX12Vertex);
        if (!fc.VertexBuffer || fc.VertexBufferSize < s_GlobalMeshBuilder.Vertices.size()) {
            if (fc.VertexBuffer) {
                fc.VertexBuffer->Unmap(0, nullptr);
                fc.VertexBuffer->Release();
                fc.VertexBuffer = nullptr;
            }
            fc.VertexBufferSize = s_GlobalMeshBuilder.Vertices.size() + INITIAL_VERTEX_BUFFER_SIZE;

            D3D12_HEAP_PROPERTIES uploadHeapProps{};
            uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = fc.VertexBufferSize * sizeof(DX12Vertex);
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            g_DX12_Device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&fc.VertexBuffer));
            D3D12_RANGE readRange{ 0, 0 };
            fc.VertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&fc.VertexMappedData));
        }

        size_t requiredIbSize = s_GlobalMeshBuilder.Indices.size() * sizeof(uint32_t);
        if (!fc.IndexBuffer || fc.IndexBufferSize < s_GlobalMeshBuilder.Indices.size()) {
            if (fc.IndexBuffer) {
                fc.IndexBuffer->Unmap(0, nullptr);
                fc.IndexBuffer->Release();
                fc.IndexBuffer = nullptr;
            }
            fc.IndexBufferSize = s_GlobalMeshBuilder.Indices.size() + INITIAL_INDEX_BUFFER_SIZE;

            D3D12_HEAP_PROPERTIES uploadHeapProps{};
            uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = fc.IndexBufferSize * sizeof(uint32_t);
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            g_DX12_Device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&fc.IndexBuffer));
            D3D12_RANGE readRange{ 0, 0 };
            fc.IndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&fc.IndexMappedData));
        }

        if (fc.VertexMappedData) {
            memcpy(fc.VertexMappedData, s_GlobalMeshBuilder.Vertices.data(), requiredVbSize);
        }
        if (fc.IndexMappedData) {
            memcpy(fc.IndexMappedData, s_GlobalMeshBuilder.Indices.data(), requiredIbSize);
        }

        // 第五阶段：设置管线状态并批量 DrawIndexedInstanced
        D3D12_VIEWPORT vp{};
        vp.Width = drawData->DisplaySize.x;
        vp.Height = drawData->DisplaySize.y;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        cmdList->RSSetViewports(1, &vp);

        D3D12_VERTEX_BUFFER_VIEW vbView{};
        vbView.BufferLocation = fc.VertexBuffer->GetGPUVirtualAddress();
        vbView.SizeInBytes = static_cast<UINT>(requiredVbSize);
        vbView.StrideInBytes = sizeof(DX12Vertex);
        cmdList->IASetVertexBuffers(0, 1, &vbView);

        D3D12_INDEX_BUFFER_VIEW ibView{};
        ibView.BufferLocation = fc.IndexBuffer->GetGPUVirtualAddress();
        ibView.SizeInBytes = static_cast<UINT>(requiredIbSize);
        ibView.Format = DXGI_FORMAT_R32_UINT;
        cmdList->IASetIndexBuffer(&ibView);

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->SetPipelineState(g_DX12_PipelineState);
        cmdList->SetGraphicsRootSignature(g_DX12_RootSignature);

        ID3D12DescriptorHeap* heaps[] = { g_DX12_SrvDescriptorHeap };
        cmdList->SetDescriptorHeaps(1, heaps);

        float L = 0.0f;
        float R = drawData->DisplaySize.x;
        float T = 0.0f;
        float B = drawData->DisplaySize.y;
        float mvp[4][4] = {
            { 2.0f / (R - L),    0.0f,              0.0f, 0.0f },
            { 0.0f,              2.0f / (T - B),    0.0f, 0.0f },
            { 0.0f,              0.0f,              0.5f, 0.0f },
            { (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f },
        };
        cmdList->SetGraphicsRoot32BitConstants(0, 16, mvp, 0);

        float lastBatchGamma = -1.0f;
        for (const auto& batch : s_GlobalMeshBuilder.Batches) {
            if (batch.indexCount == 0) continue;
            if (std::abs(batch.gamma - lastBatchGamma) >= DX12_MIN_GAMMA_DIFF) {
                lastBatchGamma = batch.gamma;
                float invGamma = (batch.gamma > DX12_MIN_GAMMA_DIFF) ? (1.0f / batch.gamma) : (1.0f / DX12_DEFAULT_GAMMA);
                cmdList->SetGraphicsRoot32BitConstants(2, 1, &invGamma, 0);
            }

            cmdList->RSSetScissorRects(1, &batch.scissorRect);
            cmdList->SetGraphicsRootDescriptorTable(1, batch.gpuSrvHandle);
            cmdList->DrawIndexedInstanced(batch.indexCount, 1, batch.startIndex, 0, 0);
        }
    }
} // namespace Shadow