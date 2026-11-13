/*
    目前无法完全适配 UE5.8+
    在 UE5.8 及以上版本 GC 时，
    *reinterpret_cast<FScriptArrayHelper*>(&NewFont->CompositeFont.DefaultTypeface.Fonts) = { &Entry, 1, 1 };
    会导致游戏崩溃。
    且纹理加载处于不确定状态。
	避免在 UE5.8 及以上版本使用 LoadFont / LoadTexture
*/

#pragma once
#include <windows.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <algorithm>

#include "../../CppSDK/SDK/Basic.hpp"
#include "../../CppSDK/SDK/CoreUObject_classes.hpp"
#include "../../CppSDK/SDK/Engine_classes.hpp"
#include "Shadow_Texture.h"
#include "Shadow.h"

namespace Shadow {
    // 纹理退役池初始预留容量
    constexpr size_t TEXTURE_RETIRED_POOL_RESERVE = 64;

    // UFunction 标志位定义
    constexpr int32_t FUNCTION_FLAG_NATIVE = 0x00000400; // FUNC_Native

    // 通用标志位定义 (UObject Flags & Internal GC Flags)
    constexpr uint32_t OBJECT_FLAG_PUBLIC = 0x00000001; // RF_Public
    constexpr uint32_t OBJECT_FLAG_STANDALONE = 0x00000002; // RF_Standalone
    constexpr uint32_t OBJECT_FLAG_TRANSIENT = 0x00000040; // RF_Transient
    constexpr uint32_t OBJECT_FLAG_TAG_GARBAGE_TEMP = 0x00000100; // RF_TagGarbageTemp
    constexpr uint32_t OBJECT_FLAG_DESTROYED_MASK = 0x40000000 | 0x80000000; // RF_BeginDestroyed | RF_FinishDestroyed

    constexpr int32_t INTERNAL_FLAG_NONE = 0;
    constexpr int32_t INTERNAL_FLAG_GARBAGE = 1 << 27; // 0x08000000
    constexpr int32_t INTERNAL_FLAG_PERSISTENT_GARBAGE = 1 << 28; // 0x10000000
    constexpr int32_t INTERNAL_FLAG_ROOTSET = 1 << 30; // 0x40000000
    constexpr int32_t INTERNAL_FLAG_PENDING_KILL = 1 << 31; // 0x80000000

    constexpr int32_t INTERNAL_FLAGS_GARBAGE_MASK =
        INTERNAL_FLAG_GARBAGE |
        INTERNAL_FLAG_PERSISTENT_GARBAGE |
        INTERNAL_FLAG_PENDING_KILL;

    // 几何数学常量
    constexpr float MATH_PI = 3.14159265358979323846f;
    constexpr float RAD_TO_DEG = 180.0f / MATH_PI;
    constexpr float TRIANGLE_LINE_EPSILON = 0.5f;
    constexpr float TRIANGLE_INTERPOLATE_EPSILON = 0.001f;

    constexpr float DUMPER7_DEFAULT_GAMMA = 1.0f;
    constexpr float DUMPER7_MIN_GAMMA_DIFF = 0.0001f;

    // 通用底层 FUObjectItem 映射与探针
    struct FUObjectItemInternal {
        SDK::UObject* Object;
        int32_t       Flags;
        int32_t       ClusterRootIndex;
        int32_t       SerialNumber;
    };

    static_assert(sizeof(FUObjectItemInternal) == sizeof(SDK::FUObjectItem),
        "FUObjectItem size mismatch! Please verify SDK::FUObjectItem");

    // 虚幻引擎原生底层 FScriptArray 内存布局映射 (16 字节)
    struct FScriptArrayHelper {
        void* Data = nullptr;
        int32_t NumElements = 0;
        int32_t MaxElements = 0;
    };
    static_assert(sizeof(FScriptArrayHelper) == 0x10, "FScriptArrayHelper size mismatch!");

    // 映射 UKismetRenderingLibrary::ImportBufferAsTexture2D 调用的原生 Parms 结构体 (32 字节)
    struct FImportBufferAsTexture2DParams {
        SDK::UObject* WorldContextObject;
        FScriptArrayHelper ImageBuffer;
        SDK::UTexture2D* ReturnValue;
    };
    static_assert(sizeof(FImportBufferAsTexture2DParams) == 0x20, "FImportBufferAsTexture2DParams size mismatch!");

    // 后端状态与对象池
    inline SDK::UCanvas* s_CurrentCanvas = nullptr;
    inline double s_RealTimeSeconds = 0.0;
    inline double s_DeltaTime = 0.0;

    inline SDK::UWorld* s_CachedWorld = nullptr;
    inline std::unordered_map<const void*, SDK::UTexture2D*> s_BufferTextureCache;
    inline std::unordered_map<std::wstring, SDK::UTexture2D*> s_FileTextureCache;
    inline std::vector<SDK::UTexture2D*> s_RetiredTextures;

    inline std::unordered_set<std::wstring> s_PermanentPathPool;
    inline std::unordered_map<std::wstring, SDK::UFont*> s_FileFontCache;
    inline std::unordered_map<const void*, SDK::UFont*> s_BufferFontCache;
    inline std::deque<SDK::FTypefaceEntry> s_PermanentTypefaceEntries;
    inline std::unordered_map<SDK::UFont*, int32_t> s_FontOriginalSizes;

    // 避开 Dumper-7 的 SDK::TArray 内存泄露，直接构造虚幻引擎底层参数布局调用 ProcessEvent
    inline SDK::UTexture2D* ImportBufferAsTexture2D(SDK::UWorld* World, const unsigned char* BufferData, size_t BufferSize) {
        if (!World || !BufferData || BufferSize == 0) {
            return nullptr;
        }

        static SDK::UFunction* Func = nullptr;
        if (!Func) {
            SDK::UClass* Clazz = SDK::UKismetRenderingLibrary::StaticClass();
            if (Clazz) {
                Func = Clazz->GetFunction("KismetRenderingLibrary", "ImportBufferAsTexture2D");
            }
            if (!Func) {
                Func = static_cast<SDK::UFunction*>(SDK::UObject::FindObjectFastImpl("ImportBufferAsTexture2D"));
            }
        }

        if (!Func) {
            return nullptr;
        }

        SDK::UObject* DefaultObj = SDK::UKismetRenderingLibrary::GetDefaultObj();
        if (!DefaultObj) {
            return nullptr;
        }

        FImportBufferAsTexture2DParams Parms{};
        Parms.WorldContextObject = World;
        Parms.ImageBuffer.Data = const_cast<unsigned char*>(BufferData);
        Parms.ImageBuffer.NumElements = static_cast<int32_t>(BufferSize);
        Parms.ImageBuffer.MaxElements = static_cast<int32_t>(BufferSize);
        Parms.ReturnValue = nullptr;

        const auto OriginalFlags = Func->FunctionFlags;
        Func->FunctionFlags |= FUNCTION_FLAG_NATIVE;

        DefaultObj->ProcessEvent(Func, &Parms);

        Func->FunctionFlags = OriginalFlags;

        return Parms.ReturnValue;
    }

    inline FUObjectItemInternal* GetObjectItem(SDK::UObject* Obj) {
        if (!Obj) {
            return nullptr;
        }

        SDK::TUObjectArray* ObjectsArray = SDK::UObject::GObjects.GetTypedPtr();
        if (!ObjectsArray) {
            return nullptr;
        }

        const int32_t Index = Obj->Index;
        if (Index < 0 || Index >= ObjectsArray->NumElements) {
            return nullptr;
        }

        const int32_t ChunkIndex = Index / SDK::TUObjectArray::ElementsPerChunk;
        const int32_t InChunkIdx = Index % SDK::TUObjectArray::ElementsPerChunk;

        if (ObjectsArray->NumChunks > 0 && ChunkIndex >= ObjectsArray->NumChunks) {
            return nullptr;
        }

        SDK::FUObjectItem** DecryptedChunks = ObjectsArray->GetDecrytedObjPtr();
        if (!DecryptedChunks) {
            return nullptr;
        }

        SDK::FUObjectItem* ChunkPtr = DecryptedChunks[ChunkIndex];
        if (!ChunkPtr) {
            return nullptr;
        }

        return reinterpret_cast<FUObjectItemInternal*>(&ChunkPtr[InChunkIdx]);
    }

    // 判断 Texture 是否处于存活可渲染状态
    inline bool IsObjectAliveAndValid(SDK::UObject* Obj) {
        if (!Obj) {
            return false;
        }

        const uint32_t ObjFlags = *reinterpret_cast<uint32_t*>(&Obj->Flags);
        if (ObjFlags & OBJECT_FLAG_DESTROYED_MASK) {
            return false;
        }

        FUObjectItemInternal* Item = GetObjectItem(Obj);
        if (Item) {
            // 如果对象被 GhostObject 隐藏过，Item->Object 会变为 nullptr。
            // 只要它没有被其他新对象占用，我们就认为该对象依然存活。
            if (Item->Object != nullptr && Item->Object != Obj) {
                return false;
            }
            if (Item->Flags & INTERNAL_FLAGS_GARBAGE_MASK) {
                return false;
            }
        }

        return true;
    }

    // 剔除瞬态标志，打上 Standalone 与 RootSet
    inline void InternalSolidifyBase(SDK::UObject* Obj) {
        if (!Obj) {
            return;
        }

        *(uint32_t*)(&Obj->Flags) &= ~(OBJECT_FLAG_TRANSIENT | OBJECT_FLAG_TAG_GARBAGE_TEMP);
        *(uint32_t*)(&Obj->Flags) |= (OBJECT_FLAG_PUBLIC | OBJECT_FLAG_STANDALONE);

        FUObjectItemInternal* Item = GetObjectItem(Obj);
        if (Item && Item->Object == Obj) {
            Item->Flags &= ~INTERNAL_FLAGS_GARBAGE_MASK;
            Item->Flags |= INTERNAL_FLAG_ROOTSET;
        }
    }

    // 将对象从全局池中抹除，使引擎在退出时彻底忽略它，阻止 FMemory::Free 释放 CRT 内存
    inline void GhostObject(SDK::UObject* Obj) {
        if (!Obj) {
            return;
        }

        FUObjectItemInternal* Item = GetObjectItem(Obj);
        if (Item && Item->Object == Obj) {
            Item->Object = nullptr; // 引擎从此失去对该对象的记忆
            Item->Flags = 0;
        }
    }

    // 针对 Texture2D 的专用固化（包含显存流送与 UI 组别锁定）
    inline void SolidifyTexture(SDK::UTexture2D* Texture) {
        if (!Texture) {
            return;
        }

        InternalSolidifyBase(Texture);
        Texture->bTemporarilyDisableStreaming = 1;
        Texture->NeverStream = 1; // 永久禁止流送系统卸载纹理资源
        Texture->LODGroup = SDK::ETextureGroup::TEXTUREGROUP_UI;

        // 将纹理从全局对象数组中隐藏，防止引擎 GC 回收其内存并复用地址导致纹理互换
        GhostObject(Texture);
    }

    // 移除 Standalone 与 RootSet 标志，允许引擎 GC 正常回收对象
    inline void InternalUnsolidifyBase(SDK::UObject* Obj) {
        if (!Obj) {
            return;
        }

        *(uint32_t*)(&Obj->Flags) &= ~(OBJECT_FLAG_PUBLIC | OBJECT_FLAG_STANDALONE);
        *(uint32_t*)(&Obj->Flags) |= OBJECT_FLAG_TRANSIENT;

        FUObjectItemInternal* Item = GetObjectItem(Obj);
        if (Item && Item->Object == Obj) {
            Item->Flags &= ~INTERNAL_FLAG_ROOTSET;
        }
    }

    // 针对 Texture2D 的解除固化（恢复显存流送限制并允许 GC 回收）
    inline void UnsolidifyTexture(SDK::UTexture2D* Texture) {
        if (!Texture) {
            return;
        }

        InternalUnsolidifyBase(Texture);
        Texture->bTemporarilyDisableStreaming = 0;
    }

    // 将旧缓存中的纹理指针移入退役池。
    inline void RetireTexturePointer(SDK::UTexture2D* Texture) {
        if (!Texture) {
            return;
        }

        if (s_RetiredTextures.capacity() < TEXTURE_RETIRED_POOL_RESERVE) {
            s_RetiredTextures.reserve(TEXTURE_RETIRED_POOL_RESERVE);
        }

        s_RetiredTextures.push_back(Texture);
    }

    // 检查并自动维护 World 变动（关卡切换时清空易失的 Texture 显存缓存）
    inline SDK::UWorld* GetReadyWorld() {
        SDK::UWorld* CurrentWorld = SDK::UWorld::GetWorld();
        if (!CurrentWorld || !CurrentWorld->OwningGameInstance || !CurrentWorld->PersistentLevel) {
            return nullptr;
        }

        if (CurrentWorld != s_CachedWorld) {
            s_CachedWorld = CurrentWorld;
            for (auto& Pair : s_BufferTextureCache) {
                RetireTexturePointer(Pair.second);
            }
            s_BufferTextureCache.clear();

            for (auto& Pair : s_FileTextureCache) {
                RetireTexturePointer(Pair.second);
            }
            s_FileTextureCache.clear();
        }

        return CurrentWorld;
    }

    // 获取持久化 Outer 容器
    inline SDK::UObject* GetPersistentOuter() {
        SDK::UObject* TransientPkg = SDK::UObject::FindObjectFastImpl("Transient");
        if (TransientPkg) {
            return TransientPkg;
        }

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (World && World->OwningGameInstance) {
            return World->OwningGameInstance;
        }

        return World;
    }

    // 永久缓存字符串指针，杜绝 FreeType 读取到悬垂路径
    inline const wchar_t* GetPermanentPathPtr(const std::wstring& Path) {
        auto Pair = s_PermanentPathPool.insert(Path);
        return Pair.first->c_str();
    }

    // 创建 Slate 专用的全局持久 Runtime UFont 实例
    inline SDK::UFont* CreatePermanentRuntimeFont() {
        SDK::UClass* FontClass = SDK::UFont::StaticClass();
        if (!FontClass) {
            return nullptr;
        }

        SDK::UObject* Outer = GetPersistentOuter();
        if (!Outer) {
            return nullptr;
        }

        SDK::UObject* NewObj = SDK::UGameplayStatics::SpawnObject(FontClass, Outer);
        return static_cast<SDK::UFont*>(NewObj);
    }

    struct ScopedFontScale {
        SDK::UFont* Font = nullptr;
        int32_t OriginalSize = 0;
        bool NoSDF = false;
        float CalculatedScale = 1.0f;

        ScopedFontScale(SDK::UFont* font, float fontScale = 1.0f, bool noSDF = false) {
            if (!font) return;
            Font = font;
            NoSDF = noSDF;

            // 查找或记录该字体最初的未缩放基础字号
            auto it = s_FontOriginalSizes.find(font);
            if (it == s_FontOriginalSizes.end()) {
                OriginalSize = font->LegacyFontSize;
                s_FontOriginalSizes[font] = OriginalSize;
            }
            else {
                OriginalSize = it->second;
            }

            const auto& style = Shadow::GetStyle();
            int32_t baseSize = OriginalSize > 0 ? OriginalSize : static_cast<int32_t>(style.FallbackFontSize);
            float finalScale = fontScale * style.FontScaleDpi;

            if (NoSDF) {
                if (font->LegacyFontSize != static_cast<int32_t>(style.NoSDFFontBaseSize)) {
                    font->LegacyFontSize = static_cast<int32_t>(style.NoSDFFontBaseSize);
                }
                float desiredSize = std::clamp(static_cast<float>(baseSize) * finalScale, style.FontSizeMin, style.FontSizeMax);
                CalculatedScale = desiredSize / style.NoSDFFontBaseSize;
            }
            else {
                int32_t targetSize = std::clamp(static_cast<int32_t>(baseSize * finalScale), static_cast<int32_t>(style.FontSizeMin), static_cast<int32_t>(style.FontSizeMax));
                font->LegacyFontSize = targetSize;
                CalculatedScale = 1.0f;
            }
        }

        ~ScopedFontScale() {
            if (Font && !NoSDF) {
                Font->LegacyFontSize = OriginalSize;
            }
        }
    };

    // 后端资源加载真实实现函数
    inline ShadowTexture Dumper7_LoadTextureFromBuffer(const unsigned char* BufferData, size_t BufferSize) {
        if (!BufferData || BufferSize == 0) {
            return ShadowTexture{};
        }

        SDK::UWorld* World = GetReadyWorld();
        if (!World) {
            return ShadowTexture{};
        }

        auto It = s_BufferTextureCache.find(BufferData);
        if (It != s_BufferTextureCache.end()) {
            if (IsObjectAliveAndValid(It->second)) {
                return ShadowTexture{ It->second };
            }
            s_BufferTextureCache.erase(It);
        }

        SDK::UTexture2D* LoadedTexture = ImportBufferAsTexture2D(World, BufferData, BufferSize);

        if (LoadedTexture) {
            SolidifyTexture(LoadedTexture);
            s_BufferTextureCache[BufferData] = LoadedTexture;
        }

        return ShadowTexture{ LoadedTexture };
    }

    inline ShadowTexture Dumper7_LoadTextureFromFile(const wchar_t* FilePath) {
        if (!FilePath) {
            return ShadowTexture{};
        }

        SDK::UWorld* World = GetReadyWorld();
        if (!World) {
            return ShadowTexture{};
        }

        auto It = s_FileTextureCache.find(FilePath);
        if (It != s_FileTextureCache.end()) {
            return ShadowTexture{ It->second };
        }

        SDK::FString PathStr(FilePath);
        SDK::UTexture2D* LoadedTexture = SDK::UKismetRenderingLibrary::ImportFileAsTexture2D(World, PathStr);
        if (LoadedTexture) {
            SolidifyTexture(LoadedTexture);
            s_FileTextureCache[FilePath] = LoadedTexture;
        }

        return ShadowTexture{ LoadedTexture };
    }

    inline ShadowFont Dumper7_LoadFontFromFile(const wchar_t* FilePath, int32_t FontSize = DEFAULT_FONT_SIZE) {
        if (!FilePath) {
            return ShadowFont{};
        }

        auto It = s_FileFontCache.find(FilePath);
        if (It != s_FileFontCache.end()) {
            return ShadowFont{ It->second };
        }

        SDK::UFont* NewFont = CreatePermanentRuntimeFont();
        if (!NewFont) {
            return ShadowFont{};
        }

        NewFont->FontCacheType = SDK::EFontCacheType::Runtime;

        if (FontSize >= MIN_VALID_FONT_SIZE) {
            NewFont->LegacyFontSize = FontSize;
        }

        const wchar_t* SafePathPtr = GetPermanentPathPtr(FilePath);

        SDK::FTypefaceEntry& Entry = s_PermanentTypefaceEntries.emplace_back();
        Entry.Name = SDK::FName();
        Entry.Font.FontFilename = SDK::FString(SafePathPtr);
        Entry.Font.Hinting = SDK::EFontHinting::Default;
        Entry.Font.LoadingPolicy = SDK::EFontLoadingPolicy::LazyLoad;
        Entry.Font.SubFaceIndex = 0;
        Entry.Font.FontFaceAsset = nullptr;

        // 无法适配 UE5.8+
        *reinterpret_cast<FScriptArrayHelper*>(&NewFont->CompositeFont.DefaultTypeface.Fonts) = { &Entry, 1, 1 };

        GhostObject(NewFont);
        s_FileFontCache[FilePath] = NewFont;

        return ShadowFont{ NewFont };
    }

    inline ShadowFont Dumper7_LoadFontFromBuffer(const unsigned char* BufferData, size_t BufferSize, int32_t FontSize = DEFAULT_FONT_SIZE) {
        if (!BufferData || BufferSize == 0) {
            return ShadowFont{};
        }

        auto It = s_BufferFontCache.find(BufferData);
        if (It != s_BufferFontCache.end()) {
            return ShadowFont{ It->second };
        }

        std::filesystem::path TempDir = std::filesystem::temp_directory_path();
        std::wstring TempFileName = L"mod_font_" + std::to_wstring(reinterpret_cast<uintptr_t>(BufferData)) + L".ttf";
        std::filesystem::path TempFilePath = TempDir / TempFileName;

        if (!std::filesystem::exists(TempFilePath)) {
            std::ofstream Out(TempFilePath, std::ios::binary);
            if (!Out.is_open()) {
                return ShadowFont{};
            }
            Out.write(reinterpret_cast<const char*>(BufferData), BufferSize);
            Out.close();
        }

        ShadowFont LoadedFont = Dumper7_LoadFontFromFile(TempFilePath.c_str(), FontSize);
        if (LoadedFont) {
            s_BufferFontCache[BufferData] = static_cast<SDK::UFont*>(LoadedFont.NativePtr);
        }

        return LoadedFont;
    }

    // 后端文本尺寸测量实现
    inline Vec2 Dumper7_MeasureText(ShadowFont shadowFont, float fontScale, bool noSDF, std::string_view text) {
        SDK::UFont* font = static_cast<SDK::UFont*>(shadowFont.NativePtr);
        if (!s_CurrentCanvas || !font || text.empty()) {
            return { 0.f, 0.f };
        }

        ScopedFontScale fontGuard(font, fontScale, noSDF);
        SDK::FVector2D scale{ fontGuard.CalculatedScale, fontGuard.CalculatedScale };

        std::wstring wstr = Shadow::ToWString(text);
        SDK::FVector2D size = s_CurrentCanvas->K2_TextSize(font, SDK::FString(wstr.c_str()), scale);
        return { static_cast<float>(size.X), static_cast<float>(size.Y) };
    }

    inline SDK::UTexture2D* GetCircleTexture(float radius) {
        if (radius <= 0.0f) return nullptr;
        float diameter = radius * 2.0f;
        if (diameter <= 16.0f) {
            return static_cast<SDK::UTexture2D*>(Dumper7_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_16, sizeof(Shadow_Texture::CircleFilled_16)).NativePtr);
        }
        else if (diameter <= 32.0f) {
            return static_cast<SDK::UTexture2D*>(Dumper7_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_32, sizeof(Shadow_Texture::CircleFilled_32)).NativePtr);
        }
        else if (diameter <= 64.0f) {
            return static_cast<SDK::UTexture2D*>(Dumper7_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_64, sizeof(Shadow_Texture::CircleFilled_64)).NativePtr);
        }
        else if (diameter <= 128.0f) {
            return static_cast<SDK::UTexture2D*>(Dumper7_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_128, sizeof(Shadow_Texture::CircleFilled_128)).NativePtr);
        }
        else if (diameter <= 256.0f) {
            return static_cast<SDK::UTexture2D*>(Dumper7_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_256, sizeof(Shadow_Texture::CircleFilled_256)).NativePtr);
        }
        else if (diameter <= 512.0f) {
            return static_cast<SDK::UTexture2D*>(Dumper7_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_512, sizeof(Shadow_Texture::CircleFilled_512)).NativePtr);
        }
        else {
            return static_cast<SDK::UTexture2D*>(Dumper7_LoadTextureFromBuffer(Shadow_Texture::CircleFilled_1024, sizeof(Shadow_Texture::CircleFilled_1024)).NativePtr);
        }
    }

    inline void InternalDrawLine(Vec2 start, Vec2 end, Color color, float thickness, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, SDK::UTexture* texture = nullptr) {
        if (!s_CurrentCanvas) return;
        SDK::UTexture* tex = texture ? texture : (s_CurrentCanvas ? s_CurrentCanvas->DefaultTexture : nullptr);
        if (!tex) return;

        if (clipEnabled) {
            float dx = end.x - start.x;
            float dy = end.y - start.y;
            float t0 = 0.0f, t1 = 1.0f;

            auto ClipTest = [](float p, float q, float& t0, float& t1) -> bool {
                if (p == 0.0f) {
                    return q >= 0.0f;
                }
                float r = q / p;
                if (p < 0.0f) {
                    if (r > t1) return false;
                    if (r > t0) t0 = r;
                }
                else {
                    if (r < t0) return false;
                    if (r < t1) t1 = r;
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
        float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.0f) return;

        float angle = std::atan2(dy, dx) * RAD_TO_DEG;

        SDK::FVector2D uePos{ static_cast<float>(start.x), static_cast<float>(start.y) };
        SDK::FVector2D ueSize{ static_cast<float>(length), static_cast<float>(thickness) };
        SDK::FLinearColor ueColor{ color.r, color.g, color.b, color.a };

        s_CurrentCanvas->K2_DrawTexture(
            tex,
            uePos, ueSize,
            SDK::FVector2D{ 0.0f, 0.0f }, SDK::FVector2D{ 1.0f, 1.0f },
            ueColor, SDK::EBlendMode::BLEND_Translucent,
            angle, SDK::FVector2D{ 0.0f, 0.5f }
        );
    }

    inline void InternalDrawRect(Vec2 pos, Vec2 size, Color color, float thickness, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, float rounding, ShadowDrawFlags flags, SDK::UTexture* texture = nullptr) {
        if (!s_CurrentCanvas) return;
        if (size.x <= 0.f || size.y <= 0.f) return;

        if (clipEnabled) {
            if (pos.x + size.x < clipMin.x || pos.x > clipMax.x ||
                pos.y + size.y < clipMin.y || pos.y > clipMax.y)
                return;
        }

        bool roundTL = (flags & ShadowDrawFlags_RoundCornersTopLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
        bool roundTR = (flags & ShadowDrawFlags_RoundCornersTopRight) && !(flags & ShadowDrawFlags_RoundCornersNone);
        bool roundBL = (flags & ShadowDrawFlags_RoundCornersBottomLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
        bool roundBR = (flags & ShadowDrawFlags_RoundCornersBottomRight) && !(flags & ShadowDrawFlags_RoundCornersNone);

        if (rounding <= 0.0f || (!roundTL && !roundTR && !roundBL && !roundBR)) {
            float halfThick = thickness * 0.5f;
            InternalDrawLine({ pos.x - halfThick, pos.y }, { pos.x + size.x + halfThick, pos.y }, color, thickness, clipEnabled, clipMin, clipMax, texture);
            InternalDrawLine({ pos.x - halfThick, pos.y + size.y }, { pos.x + size.x + halfThick, pos.y + size.y }, color, thickness, clipEnabled, clipMin, clipMax, texture);
            InternalDrawLine({ pos.x, pos.y + halfThick }, { pos.x, pos.y + size.y - halfThick }, color, thickness, clipEnabled, clipMin, clipMax, texture);
            InternalDrawLine({ pos.x + size.x, pos.y + halfThick }, { pos.x + size.x, pos.y + size.y - halfThick }, color, thickness, clipEnabled, clipMin, clipMax, texture);
            return;
        }

        float r = std::min(rounding, std::min(size.x, size.y) * 0.5f);
        float r_tl = roundTL ? r : 0.0f;
        float r_tr = roundTR ? r : 0.0f;
        float r_bl = roundBL ? r : 0.0f;
        float r_br = roundBR ? r : 0.0f;

        InternalDrawLine({ pos.x + r_tl, pos.y }, { pos.x + size.x - r_tr, pos.y }, color, thickness, clipEnabled, clipMin, clipMax, texture);
        InternalDrawLine({ pos.x + r_bl, pos.y + size.y }, { pos.x + size.x - r_br, pos.y + size.y }, color, thickness, clipEnabled, clipMin, clipMax, texture);
        InternalDrawLine({ pos.x, pos.y + r_tl }, { pos.x, pos.y + size.y - r_bl }, color, thickness, clipEnabled, clipMin, clipMax, texture);
        InternalDrawLine({ pos.x + size.x, pos.y + r_tr }, { pos.x + size.x, pos.y + size.y - r_br }, color, thickness, clipEnabled, clipMin, clipMax, texture);

        int segments = RECT_ROUNDING_SEGMENTS;

        auto DrawCorner = [&](Vec2 center, float radius, float startAngle, float endAngle) {
            if (radius <= 0.f) return;
            for (int i = 0; i < segments; ++i) {
                float t0 = (float)i / segments;
                float t1 = (float)(i + 1) / segments;
                float a0 = startAngle + t0 * (endAngle - startAngle);
                float a1 = startAngle + t1 * (endAngle - startAngle);
                InternalDrawLine(
                    { center.x + radius * std::cos(a0), center.y - radius * std::sin(a0) },
                    { center.x + radius * std::cos(a1), center.y - radius * std::sin(a1) },
                    color, thickness, clipEnabled, clipMin, clipMax, texture
                );
            }
            };

        if (roundTL) DrawCorner({ pos.x + r, pos.y + r }, r, MATH_PI / 2.0f, MATH_PI);
        if (roundTR) DrawCorner({ pos.x + size.x - r, pos.y + r }, r, 0.0f, MATH_PI / 2.0f);
        if (roundBL) DrawCorner({ pos.x + r, pos.y + size.y - r }, r, MATH_PI, 3.0f * MATH_PI / 2.0f);
        if (roundBR) DrawCorner({ pos.x + size.x - r, pos.y + size.y - r }, r, 3.0f * MATH_PI / 2.0f, 2.0f * MATH_PI);
    }

    inline void InternalDrawRectFilled(Vec2 pos, Vec2 size, Color color, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, float rounding, ShadowDrawFlags flags, SDK::UTexture* texture = nullptr) {
        if (!s_CurrentCanvas) return;

        bool roundTL = (flags & ShadowDrawFlags_RoundCornersTopLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
        bool roundTR = (flags & ShadowDrawFlags_RoundCornersTopRight) && !(flags & ShadowDrawFlags_RoundCornersNone);
        bool roundBL = (flags & ShadowDrawFlags_RoundCornersBottomLeft) && !(flags & ShadowDrawFlags_RoundCornersNone);
        bool roundBR = (flags & ShadowDrawFlags_RoundCornersBottomRight) && !(flags & ShadowDrawFlags_RoundCornersNone);

        if (rounding > 0.0f && texture == nullptr && (roundTL || roundTR || roundBL || roundBR)) {
            float r = std::min(rounding, std::min(size.x, size.y) * 0.5f);
            SDK::UTexture2D* circleTex = GetCircleTexture(r);
            if (circleTex && IsObjectAliveAndValid(circleTex)) {
                float w = size.x;
                float h = size.y;

                SDK::UTexture* defTex = s_CurrentCanvas->DefaultTexture;
                if (!defTex) return;

                auto DrawPiece = [&](float x, float y, float piece_w, float piece_h, float u0, float v0, float u1, float v1, SDK::UTexture* tex) {
                    if (!tex) return;
                    if (piece_w <= 0.f || piece_h <= 0.f) return;
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

                    SDK::FLinearColor ueColor{ color.r, color.g, color.b, color.a };
                    SDK::FVector2D uePos{ static_cast<float>(drawPos.x), static_cast<float>(drawPos.y) };
                    SDK::FVector2D ueSize{ static_cast<float>(drawSize.x), static_cast<float>(drawSize.y) };
                    SDK::FVector2D ueUV0{ static_cast<float>(drawUV0.x), static_cast<float>(drawUV0.y) };
                    SDK::FVector2D ueUVSize{ static_cast<float>(drawUV1.x - drawUV0.x), static_cast<float>(drawUV1.y - drawUV0.y) };

                    s_CurrentCanvas->K2_DrawTexture(
                        tex, uePos, ueSize,
                        ueUV0, ueUVSize,
                        ueColor, SDK::EBlendMode::BLEND_Translucent, 0.0f, SDK::FVector2D{ 0.0f, 0.0f }
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

        SDK::UTexture* tex = texture ? texture : s_CurrentCanvas->DefaultTexture;
        if (!tex) return;

        if (clipEnabled) {
            if (pos.x + size.x <= clipMin.x || pos.x >= clipMax.x || pos.y + size.y <= clipMin.y || pos.y >= clipMax.y) return;
            if (pos.x < clipMin.x) { size.x -= (clipMin.x - pos.x); pos.x = clipMin.x; }
            if (pos.y < clipMin.y) { size.y -= (clipMin.y - pos.y); pos.y = clipMin.y; }
            if (pos.x + size.x > clipMax.x) size.x = clipMax.x - pos.x;
            if (pos.y + size.y > clipMax.y) size.y = clipMax.y - pos.y;
            if (size.x <= 0.f || size.y <= 0.f) return;
        }

        SDK::FLinearColor ueColor{ color.r, color.g, color.b, color.a };
        SDK::FVector2D uePos{ static_cast<float>(pos.x), static_cast<float>(pos.y) };
        SDK::FVector2D ueSize{ static_cast<float>(size.x), static_cast<float>(size.y) };

        s_CurrentCanvas->K2_DrawTexture(
            tex, uePos, ueSize,
            SDK::FVector2D{ 0.0f, 0.0f }, SDK::FVector2D{ 1.0f, 1.0f },
            ueColor, SDK::EBlendMode::BLEND_Translucent, 0.0f, SDK::FVector2D{ 0.0f, 0.0f }
        );
    }

    inline void InternalDrawText(const std::string& text, Vec2 pos, Color color, SDK::UFont* font, float fontScale, Color textShadowColor, Color textOutlineColor, bool textOutline, bool noSDF = false) {
        if (!s_CurrentCanvas || !font) return;

        ScopedFontScale fontGuard(font, fontScale, noSDF);

        SDK::FVector2D scale{ fontGuard.CalculatedScale, fontGuard.CalculatedScale };
        SDK::FLinearColor ueColor{ color.r, color.g, color.b, color.a };
        SDK::FVector2D uePos{ static_cast<float>(pos.x), static_cast<float>(pos.y) };

        SDK::FLinearColor shadow{ textShadowColor.r, textShadowColor.g, textShadowColor.b, textShadowColor.a };
        SDK::FLinearColor outline{ textOutlineColor.r, textOutlineColor.g, textOutlineColor.b, textOutlineColor.a };
        SDK::FVector2D shadowOff{ Shadow::GetStyle().TextShadowOffsetX, Shadow::GetStyle().TextShadowOffsetY };

        std::wstring wstr = Shadow::ToWString(text);
        s_CurrentCanvas->K2_DrawText(font, SDK::FString(wstr.c_str()), uePos, scale, ueColor, 0.0f, shadow, shadowOff, false, false, textOutline, outline);
    }

    inline void InternalDrawTriangleFilled(Vec2 p1, Vec2 p2, Vec2 p3, Color color, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, SDK::UTexture* texture = nullptr) {
        Vec2 v[3] = { p1, p2, p3 };
        if (v[0].y > v[1].y) std::swap(v[0], v[1]);
        if (v[0].y > v[2].y) std::swap(v[0], v[2]);
        if (v[1].y > v[2].y) std::swap(v[1], v[2]);
        if (std::abs(v[0].y - v[2].y) < TRIANGLE_LINE_EPSILON) return;

        auto interpolateX = [](float y, const Vec2& a, const Vec2& b) {
            if (std::abs(a.y - b.y) < TRIANGLE_INTERPOLATE_EPSILON) return (a.x + b.x) * 0.5f;
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
                InternalDrawRectFilled({ x_min, static_cast<float>(y) }, { w, 1.0f }, color, clipEnabled, clipMin, clipMax, 0.0f, ShadowDrawFlags_RoundCornersNone, texture);
            }
        }
    }

    inline void InternalDrawTriangle(Vec2 p1, Vec2 p2, Vec2 p3, Color color, float thickness, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, SDK::UTexture* texture = nullptr) {
        InternalDrawLine(p1, p2, color, thickness, clipEnabled, clipMin, clipMax, texture);
        InternalDrawLine(p2, p3, color, thickness, clipEnabled, clipMin, clipMax, texture);
        InternalDrawLine(p3, p1, color, thickness, clipEnabled, clipMin, clipMax, texture);
    }

    // 后端函数注册表绑定
    inline void Dumper7_RegisterBackendCallbacks() {
        Shadow::g_Ctx.TextSizeFn = &Dumper7_MeasureText;
        Shadow::g_Ctx.LoadTextureFromBufferCallback = &Dumper7_LoadTextureFromBuffer;
        Shadow::g_Ctx.LoadTextureFromFileCallback = &Dumper7_LoadTextureFromFile;
        Shadow::g_Ctx.LoadFontFromFileCallback = &Dumper7_LoadFontFromFile;
        Shadow::g_Ctx.LoadFontFromBufferCallback = &Dumper7_LoadFontFromBuffer;
    }

    // 静态自动注册器：只要当前单元包含该头文件，便在进入 main/DllMain 阶段完成默认绑定
    struct Dumper7_BackendAutoRegister {
        Dumper7_BackendAutoRegister() {
            Dumper7_RegisterBackendCallbacks();
        }
    };
    inline Dumper7_BackendAutoRegister s_Dumper7_BackendAutoRegister;

    inline void Dumper7_NewFrame(SDK::UCanvas* Canvas) {
        s_CurrentCanvas = Canvas;

        if (auto world = SDK::UWorld::GetWorld()) {
            s_RealTimeSeconds = SDK::UGameplayStatics::GetRealTimeSeconds(world);
            s_DeltaTime = SDK::UGameplayStatics::GetWorldDeltaSeconds(world);
        }
        else {
            s_RealTimeSeconds = 0.0;
            s_DeltaTime = 0.0;
        }

        // 若默认字体未初始化，自动获取引擎内置的大字体
        if (!Shadow::g_Ctx.DefaultFont) {
            if (SDK::UEngine::GetEngine()) {
                Shadow::g_Ctx.DefaultFont = ShadowFont{ SDK::UEngine::GetEngine()->LargeFont };
            }
        }

        // 每次进入帧时确保绑定后端接口
        Dumper7_RegisterBackendCallbacks();

        // 同步时间与显示尺寸参数
        Shadow::g_Ctx.RealTimeSeconds = s_RealTimeSeconds;
        Shadow::g_Ctx.DeltaTime = s_DeltaTime;

        ShadowIO& io = Shadow::GetIO();
        io.DisplaySize = {
            Canvas ? static_cast<float>(Canvas->SizeX) : 0.0f,
            Canvas ? static_cast<float>(Canvas->SizeY) : 0.0f
        };
        io.DeltaTime = static_cast<float>(s_DeltaTime);
        io.BackendRenderer = BackendRenderer::Dumper7;
    }

    inline Color Dumper7_ApplyGamma(Color col, float gamma) {
        if (std::abs(gamma - DUMPER7_DEFAULT_GAMMA) < DUMPER7_MIN_GAMMA_DIFF || gamma <= 0.0f) {
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

    inline void Dumper7_RenderDrawData(const ShadowDrawData* drawData) {
        if (!drawData || !drawData->Valid || !s_CurrentCanvas) {
            return;
        }

        for (const ShadowDrawList* cmdList : drawData->CmdLists) {
            if (!cmdList) continue;
            for (const auto& cmd : cmdList->CmdBuffer) {
                Color drawCol = Dumper7_ApplyGamma(cmd.color, cmd.gamma);

                switch (cmd.type) {
                case ShadowDrawCmdType::Line:
                    InternalDrawLine(cmd.pos, cmd.size, drawCol, cmd.thickness, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, static_cast<SDK::UTexture*>(cmd.texture.NativePtr));
                    break;
                case ShadowDrawCmdType::Rect:
                    InternalDrawRect(cmd.pos, cmd.size, drawCol, cmd.thickness, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, cmd.rounding, cmd.flags, static_cast<SDK::UTexture*>(cmd.texture.NativePtr));
                    break;
                case ShadowDrawCmdType::RectFilled:
                case ShadowDrawCmdType::Texture:
                    InternalDrawRectFilled(cmd.pos, cmd.size, drawCol, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, cmd.rounding, cmd.flags, static_cast<SDK::UTexture*>(cmd.texture.NativePtr));
                    break;
                case ShadowDrawCmdType::CircleFilled: {
                    float radius = cmd.size.x;
                    if (radius > 0.0f) {
                        SDK::UTexture2D* circleTex = GetCircleTexture(radius);
                        if (circleTex) {
                            float diameter = radius * 2.0f;
                            Vec2 p = { cmd.pos.x - radius, cmd.pos.y - radius };
                            Vec2 sz = { diameter, diameter };
                            InternalDrawRectFilled(p, sz, drawCol, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, 0.0f, ShadowDrawFlags_RoundCornersNone, circleTex);
                        }
                    }
                    break;
                }
                case ShadowDrawCmdType::Text: {
                    Color textShadowCol = Dumper7_ApplyGamma(cmd.textShadowColor, cmd.gamma);
                    Color textOutlineCol = Dumper7_ApplyGamma(cmd.textOutlineColor, cmd.gamma);
                    InternalDrawText(cmd.text, cmd.pos, drawCol, static_cast<SDK::UFont*>(cmd.font.NativePtr), cmd.fontScale, textShadowCol, textOutlineCol, cmd.textOutline, cmd.noSDF);
                    break;
                }
                case ShadowDrawCmdType::TriangleFilled:
                    InternalDrawTriangleFilled(cmd.p1, cmd.p2, cmd.p3, drawCol, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, static_cast<SDK::UTexture*>(cmd.texture.NativePtr));
                    break;
                case ShadowDrawCmdType::Triangle:
                    InternalDrawTriangle(cmd.p1, cmd.p2, cmd.p3, drawCol, cmd.thickness, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, static_cast<SDK::UTexture*>(cmd.texture.NativePtr));
                    break;
                }
            }
        }
    }
} // namespace Shadow