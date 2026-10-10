/*
Credit:
    https://github.com/ocornut/imgui
    https://github.com/Encryqed/Dumper-7
    https://aistudio.google.com/
*/

#pragma once
#include <windows.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <format>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <cmath>
#include <bit>
#include <charconv>
#include <array>
#include <ranges>
#include <cstdio>
#include <optional>
#include <fstream>
#include <filesystem>
#include <deque>

#include "../../CppSDK/SDK.hpp"
#include "Shadow_Texture.h"

namespace Shadow {
    namespace Detail {
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

        // 避开 Dumper-7 的 SDK::TArray 内存泄露，直接构造虚幻引擎底层参数布局调用 ProcessEvent
        static SDK::UTexture2D* ImportBufferAsTexture2D(SDK::UWorld* World, const unsigned char* BufferData, size_t BufferSize) {
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

        static FUObjectItemInternal* GetObjectItem(SDK::UObject* Obj) {
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
        static bool IsObjectAliveAndValid(SDK::UObject* Obj) {
            if (!Obj) {
                return false;
            }

            const uint32_t ObjFlags = *reinterpret_cast<uint32_t*>(&Obj->Flags);
            if (ObjFlags & OBJECT_FLAG_DESTROYED_MASK) {
                return false;
            }

            FUObjectItemInternal* Item = GetObjectItem(Obj);
            if (Item) {
                if (Item->Object != Obj || (Item->Flags & INTERNAL_FLAGS_GARBAGE_MASK)) {
                    return false;
                }
            }

            return true;
        }

        // 剔除瞬态标志，打上 Standalone 与 RootSet
        static void InternalSolidifyBase(SDK::UObject* Obj) {
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

        // 针对 Texture2D 的专用固化（包含显存流送与 UI 组别锁定）
        static void SolidifyTexture(SDK::UTexture2D* Texture) {
            if (!Texture) {
                return;
            }

            InternalSolidifyBase(Texture);
            Texture->bTemporarilyDisableStreaming = 1;
            Texture->LODGroup = SDK::ETextureGroup::TEXTUREGROUP_UI;
        }

        // 移除 Standalone 与 RootSet 标志，允许引擎 GC 正常回收对象
        static void InternalUnsolidifyBase(SDK::UObject* Obj) {
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
        static void UnsolidifyTexture(SDK::UTexture2D* Texture) {
            if (!Texture) {
                return;
            }

            InternalUnsolidifyBase(Texture);
            Texture->bTemporarilyDisableStreaming = 0;
        }

        // 将对象从全局池中抹除，使引擎在退出时彻底忽略它，阻止 FMemory::Free 释放 CRT 内存
        static void GhostObject(SDK::UObject* Obj) {
            if (!Obj) {
                return;
            }

            FUObjectItemInternal* Item = GetObjectItem(Obj);
            if (Item && Item->Object == Obj) {
                Item->Object = nullptr; // 引擎从此失去对该对象的记忆
                Item->Flags = 0;
            }
        }

        // 上下文、环境检查与持久数据池
        static SDK::UWorld* s_CachedWorld = nullptr;
        static std::unordered_map<const void*, SDK::UTexture2D*> s_BufferTextureCache;
        static std::unordered_map<std::wstring, SDK::UTexture2D*> s_FileTextureCache;

        // 字体是全局且免疫 GC 的，使用独立永生池
        static std::unordered_set<std::wstring> s_PermanentPathPool;
        static std::unordered_map<std::wstring, SDK::UFont*> s_FileFontCache;
        static std::unordered_map<const void*, SDK::UFont*> s_BufferFontCache;
        static std::deque<SDK::FTypefaceEntry> s_PermanentTypefaceEntries;

        // 检查并自动维护 World 变动（关卡切换时清空易失的 Texture 显存缓存）
        static SDK::UWorld* GetReadyWorld() {
            SDK::UWorld* CurrentWorld = SDK::UWorld::GetWorld();
            if (!CurrentWorld || !CurrentWorld->OwningGameInstance || !CurrentWorld->PersistentLevel) {
                return nullptr;
            }

            if (CurrentWorld != s_CachedWorld) {
                s_CachedWorld = CurrentWorld;
                for (auto& Pair : s_BufferTextureCache) {
                    UnsolidifyTexture(Pair.second);
                }
                s_BufferTextureCache.clear();

                for (auto& Pair : s_FileTextureCache) {
                    UnsolidifyTexture(Pair.second);
                }
                s_FileTextureCache.clear();
            }

            return CurrentWorld;
        }

        // 获取持久化 Outer 容器
        static SDK::UObject* GetPersistentOuter() {
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
        static const wchar_t* GetPermanentPathPtr(const std::wstring& Path) {
            auto Pair = s_PermanentPathPool.insert(Path);
            return Pair.first->c_str();
        }

        // 创建 Slate 专用的全局持久 Runtime UFont 实例
        static SDK::UFont* CreatePermanentRuntimeFont() {
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
    } // namespace Detail

    static SDK::UTexture2D* LoadTextureFromBuffer(const unsigned char* BufferData, size_t BufferSize) {
        if (!BufferData || BufferSize == 0) {
            return nullptr;
        }

        SDK::UWorld* World = Detail::GetReadyWorld();
        if (!World) {
            return nullptr;
        }

        auto It = Detail::s_BufferTextureCache.find(BufferData);
        if (It != Detail::s_BufferTextureCache.end()) {
            if (Detail::IsObjectAliveAndValid(It->second)) {
                return It->second;
            }
            else {
                Detail::UnsolidifyTexture(It->second);
                Detail::s_BufferTextureCache.erase(It);
            }
        }

        SDK::UTexture2D* LoadedTexture = Detail::ImportBufferAsTexture2D(World, BufferData, BufferSize);

        if (LoadedTexture) {
            Detail::SolidifyTexture(LoadedTexture);
            Detail::s_BufferTextureCache[BufferData] = LoadedTexture;
        }

        return LoadedTexture;
    }

    static SDK::UTexture2D* LoadTextureFromFile(const wchar_t* FilePath) {
        if (!FilePath) {
            return nullptr;
        }

        SDK::UWorld* World = Detail::GetReadyWorld();
        if (!World) {
            return nullptr;
        }

        auto It = Detail::s_FileTextureCache.find(FilePath);
        if (It != Detail::s_FileTextureCache.end()) {
            if (Detail::IsObjectAliveAndValid(It->second)) {
                return It->second;
            }
            else {
                Detail::UnsolidifyTexture(It->second);
                Detail::s_FileTextureCache.erase(It);
            }
        }

        SDK::FString PathStr(FilePath);
        SDK::UTexture2D* LoadedTexture = SDK::UKismetRenderingLibrary::ImportFileAsTexture2D(World, PathStr);
        if (LoadedTexture) {
            Detail::SolidifyTexture(LoadedTexture);
            Detail::s_FileTextureCache[FilePath] = LoadedTexture;
        }

        return LoadedTexture;
    }

    static SDK::UFont* LoadFontFromFile(const wchar_t* FilePath) {
        if (!FilePath) {
            return nullptr;
        }

        // 字体已幽灵化，永不被 GC 且终生有效，直接返回命中缓存即可
        auto It = Detail::s_FileFontCache.find(FilePath);
        if (It != Detail::s_FileFontCache.end()) {
            return It->second;
        }

        SDK::UFont* NewFont = Detail::CreatePermanentRuntimeFont();
        if (!NewFont) {
            return nullptr;
        }

        NewFont->FontCacheType = SDK::EFontCacheType::Runtime;

        const wchar_t* SafePathPtr = Detail::GetPermanentPathPtr(FilePath);

        // deque 就地构造指针
        SDK::FTypefaceEntry& Entry = Detail::s_PermanentTypefaceEntries.emplace_back();
        Entry.Name = SDK::FName();
        Entry.Font.FontFilename = SDK::FString(SafePathPtr);
        Entry.Font.Hinting = SDK::EFontHinting::Default;
        Entry.Font.LoadingPolicy = SDK::EFontLoadingPolicy::LazyLoad;
        Entry.Font.SubFaceIndex = 0;
        Entry.Font.FontFaceAsset = nullptr;

        *reinterpret_cast<Detail::FScriptArrayHelper*>(&NewFont->CompositeFont.DefaultTypeface.Fonts) = { &Entry, 1, 1 };

        // 将其变成幽灵对象，切断引擎退出时对该 CRT 内存的所有销毁链路
        Detail::GhostObject(NewFont);

        Detail::s_FileFontCache[FilePath] = NewFont;

        return NewFont;
    }

    static SDK::UFont* LoadFontFromBuffer(const unsigned char* BufferData, size_t BufferSize) {
        if (!BufferData || BufferSize == 0) {
            return nullptr;
        }

        auto It = Detail::s_BufferFontCache.find(BufferData);
        if (It != Detail::s_BufferFontCache.end()) {
            return It->second;
        }

        std::filesystem::path TempDir = std::filesystem::temp_directory_path();
        std::wstring TempFileName = L"mod_font_" + std::to_wstring(reinterpret_cast<uintptr_t>(BufferData)) + L".ttf";
        std::filesystem::path TempFilePath = TempDir / TempFileName;

        if (!std::filesystem::exists(TempFilePath)) {
            std::ofstream Out(TempFilePath, std::ios::binary);
            if (!Out.is_open()) {
                return nullptr;
            }
            Out.write(reinterpret_cast<const char*>(BufferData), BufferSize);
            Out.close();
        }

        SDK::UFont* LoadedFont = LoadFontFromFile(TempFilePath.c_str());
        if (LoadedFont) {
            Detail::s_BufferFontCache[BufferData] = LoadedFont;
        }

        return LoadedFont;
    }

    enum class HotkeyMode {
        None,
        HoldOn,
        ToggleOn,
        HoldOff,
        AlwaysOn
    };

    struct Vec2 { float x, y; };
    struct Color { float r, g, b, a; };

    // 颜色枚举与样式
    enum GuiCol_ {
        GuiCol_WindowBg,
        GuiCol_TitleBarBg,
        GuiCol_Text,
        GuiCol_TextDisabled,
        GuiCol_TextHighlight,
        GuiCol_Button,
        GuiCol_ButtonHovered,
        GuiCol_FrameBg,
        GuiCol_FrameBgHovered,
        GuiCol_SliderGrab,
        GuiCol_SliderKnob,
        GuiCol_CheckMark,
        GuiCol_Separator,
        GuiCol_PopupBg,
        GuiCol_PopupBorder,
        GuiCol_ResizeGrip,
        GuiCol_ResizeGripHovered,
        GuiCol_ResizeGripActive,
        GuiCol_Tab,
        GuiCol_TabHovered,
        GuiCol_TabActive,
        GuiCol_ActiveIndicator,
        GuiCol_InactiveIndicator,
        GuiCol_Border,
        GuiCol_ErrorText,
        GuiCol_TextShadow,
        GuiCol_TextOutline,
        GuiCol_ColorPickerDark,
        GuiCol_ColorPickerLight,
        GuiCol_CheckerboardLight,
        GuiCol_CheckerboardDark,
        GuiCol_ColorPickerShadow,
        GuiCol_ControlDisabled,
        GuiCol_SwitchBg,
        GuiCol_SwitchBgHovered,
        GuiCol_SwitchBgActive,
        GuiCol_SwitchBgActiveHovered,
        GuiCol_SwitchKnob,
        GuiCol_DropdownActive,
        GuiCol_Transparent,

        GuiCol_COUNT
    };

    enum GuiStyleVar_ {
        GuiStyleVar_Alpha,
        GuiStyleVar_WindowPadding,
        GuiStyleVar_FramePadding,
        GuiStyleVar_ItemSpacing,
        GuiStyleVar_ScrollbarSize,
        GuiStyleVar_ScrollbarMargin,
        GuiStyleVar_ResizeGripSize,
        GuiStyleVar_TabExtraWidth,
        GuiStyleVar_ControlOffsetMin,
        GuiStyleVar_ControlOffsetRatio,
        GuiStyleVar_CPPadding,
        GuiStyleVar_CPSVSize,
        GuiStyleVar_CPHueWidth,
        GuiStyleVar_CPAlphaWidth,
        GuiStyleVar_CPSpacing,
        GuiStyleVar_WindowMinSize,
        GuiStyleVar_FontScaleDpi,
        GuiStyleVar_IndentSpacing,

        GuiStyleVar_COUNT
    };
    using GuiStyleVar = int;

    struct GuiStyleMod {
        int Idx;
        union {
            float BackupFloat;
            Vec2 BackupVec2;
        };
        GuiStyleMod(int idx, float val) : Idx(idx), BackupFloat(val) {}
        GuiStyleMod(int idx, Vec2 val) : Idx(idx), BackupVec2(val) {}
    };

    struct GuiColorMod {
        int ColIdx;
        Color BackupColor;
    };

    enum ShadowChannel_ {
        Channel_Background = 0,
        Channel_Midground = 1,
        Channel_Foreground = 2
    };
    using ShadowChannel = int;

    enum ShadowWindowFlags_ {
        ShadowWindowFlags_None = 0,
        ShadowWindowFlags_NoResize = 1 << 0,
        ShadowWindowFlags_NoMove = 1 << 1,
        ShadowWindowFlags_NoScrollbar = 1 << 2,
        ShadowWindowFlags_NoTitleBar = 1 << 5,
        ShadowWindowFlags_NoMouseInputs = 1 << 6,
        ShadowWindowFlags_MenuBar = 1 << 7,

        ShadowWindowFlags_TextAlignLeft = 0,
        ShadowWindowFlags_TextAlignCenter = 1 << 3,
        ShadowWindowFlags_TextAlignRight = 1 << 4,
    };
    using ShadowWindowFlags = int;

    enum ShadowTabBarFlags_ {
        ShadowTabBarFlags_None = 0,
        ShadowTabBarFlags_Reorderable = 1 << 0,
        ShadowTabBarFlags_FittingPolicyScroll = 1 << 1,
        ShadowTabBarFlags_NoScrollbar = 1 << 2,
    };
    using ShadowTabBarFlags = int;

    enum ShadowColorPickerFlags_ {
        ShadowColorPickerFlags_None = 0,
        ShadowColorPickerFlags_NoText = 1 << 0,
        ShadowColorPickerFlags_NoRightAlign = 1 << 1,
    };
    using ShadowColorPickerFlags = int;

    enum ShadowSliderFlags_ {
        ShadowSliderFlags_None = 0,
        ShadowSliderFlags_NoText = 1 << 0,
        ShadowSliderFlags_NoRightAlign = 1 << 1,
    };
    using ShadowSliderFlags = int;

    enum ShadowHotkeyFlags_ {
        ShadowHotkeyFlags_None = 0,
        ShadowHotkeyFlags_NoText = 1 << 0,
        ShadowHotkeyFlags_NoRightAlign = 1 << 1,
        ShadowHotkeyFlags_NoStateDisplay = 1 << 2,
    };
    using ShadowHotkeyFlags = int;

    enum ShadowComboFlags_ {
        ShadowComboFlags_None = 0,
        ShadowComboFlags_NoText = 1 << 0,
        ShadowComboFlags_NoRightAlign = 1 << 1,
        ShadowComboFlags_FitText = 1 << 2,
    };
    using ShadowComboFlags = int;

    enum ShadowHoveredFlags_ {
        ShadowHoveredFlags_None = 0,
        ShadowHoveredFlags_AllowWhenBlockedByPopup = 1 << 0,
        ShadowHoveredFlags_AllowWhenBlockedByActiveItem = 1 << 1,
        ShadowHoveredFlags_AllowWhenDisabled = 1 << 2,
        ShadowHoveredFlags_Stationary = 1 << 3,
        ShadowHoveredFlags_DelayNone = 1 << 4,
        ShadowHoveredFlags_DelayShort = 1 << 5,
        ShadowHoveredFlags_DelayNormal = 1 << 6,
        ShadowHoveredFlags_NoSharedDelay = 1 << 7,
    };
    using ShadowHoveredFlags = int;

    enum ShadowTreeNodeFlags_ {
        ShadowTreeNodeFlags_None = 0,
        ShadowTreeNodeFlags_DefaultOpen = 1 << 0,
        ShadowTreeNodeFlags_Framed = 1 << 1,
        ShadowTreeNodeFlags_FitText = 1 << 2,
        ShadowTreeNodeFlags_NoIndent = 1 << 3
    };
    using ShadowTreeNodeFlags = int;

    enum ShadowInputTextFlags_ {
        ShadowInputTextFlags_None = 0,
        ShadowInputTextFlags_CharsDecimal = 1 << 0,
        ShadowInputTextFlags_CharsHexadecimal = 1 << 1,
        ShadowInputTextFlags_CharsScientific = 1 << 2,
        ShadowInputTextFlags_CharsUppercase = 1 << 3,
        ShadowInputTextFlags_CharsNoBlank = 1 << 4,
        ShadowInputTextFlags_EscapeClearsAll = 1 << 5,
        ShadowInputTextFlags_ReadOnly = 1 << 6,
        ShadowInputTextFlags_Password = 1 << 7,
        ShadowInputTextFlags_AutoSelectAll = 1 << 8,
        ShadowInputTextFlags_ParseEmptyRefVal = 1 << 9,
        ShadowInputTextFlags_DisplayEmptyRefVal = 1 << 10,
        ShadowInputTextFlags_NoName = 1 << 11,
        ShadowInputTextFlags_AlignCenter = 1 << 12
    };
    using ShadowInputTextFlags = int;

    enum ShadowMouseButton_
    {
        ShadowMouseButton_Left = 0,
        ShadowMouseButton_Right = 1,
        ShadowMouseButton_Middle = 2,
        ShadowMouseButton_COUNT = 5
    };
    using ShadowMouseButton = int;

    struct GuiStyle {
        Color Colors[GuiCol_COUNT];

        // 尺寸与间距标志配置
        float Alpha = 1.0f;                  // 全局透明度
        Vec2 WindowPadding = { 16.f, 16.f }; // 窗口内边距
        Vec2 FramePadding = { 8.f, 2.f };    // 控件内文本与边缘的间距 (X为水平边距, Y为垂直居中补偿)
        Vec2 ItemSpacing = { 10.f, 8.f };    // 控件之间的水平与垂直间距
        float ScrollbarSize = 10.f;          // 滚动条宽度
        float ScrollbarMargin = 4.f;         // 滚动条与右侧边缘的间距
        float ResizeGripSize = 16.f;         // 右下角缩放手柄尺寸
        float TabExtraWidth = 20.f;          // Tab标签额外预留宽度

        // 控件特定配置
        float ControlOffsetMin = 120.f;      // 右侧互动控件对齐的最小偏移量
        float ControlOffsetRatio = 0.4f;     // 右侧互动控件偏移量占窗口宽度的比例

        // ColorPicker 弹出层专属配置
        float CPPadding = 8.f;
        float CPSVSize = 200.f;
        float CPHueWidth = 20.f;
        float CPAlphaWidth = 20.f;
        float CPSpacing = 8.f;

        // 窗口最小尺寸 和 FontScaleDpi
        Vec2 WindowMinSize = { 200.f, 150.f };
        float FontScaleDpi = 1.0f;           // 默认 1.0，用于文本与菜单自适应

        // 通用控件间距与尺寸
        float DisabledAlpha = 0.5f;                 // 禁用状态透明度乘数
        float LabelSpacing = 10.f;                  // 标签文本与右侧控件之间的间距
        float WindowScrollBottomPadding = 4.f;      // 窗口滚动区域底部额外留白
        float WindowScrollMinViewHeight = 10.f;     // 窗口滚动区域最小可视高度
        float DefaultItemHeight = 20.f;             // 未测量到字体时的默认控件高度

        // Indent
        float IndentSpacing = 20.f;                 // 默认缩进间距

        // TreeNode
        float TreeNodeArrowSizeRatio = 0.55f;
        float TreeNodeTextSpacing = 8.f;
        float TreeNodeIndent = 20.f;

        // Checkbox
        float CheckboxCheckPaddingRatio = 0.2f;

        // Switch
        float SwitchPadding = 2.f;

        // Combo
        float ComboArrowSizeRatio = 0.5f;
        float ComboMinWidth = 100.f;
        float ComboMinWidthNoRightAlign = 50.f;

        // Slider
        float SliderMinWidth = 50.f;
        float SliderKnobWidth = 4.f;
        float SliderFocusBorderThickness = 1.f;
        float SliderInputExtraWidth = 4.f;
        float SliderKeyboardStepRatio = 0.01f;
        int   SliderDefaultPrecision = 3;

        // ColorPicker
        int   ColorPickerCheckerSize = 5;
        float ColorPickerHexBoxMinHeight = 24.f;
        float ColorPickerHexBoxExtraHeight = 4.f;
        float ColorPickerCursorSize = 8.f;
        float ColorPickerCursorInnerSize = 6.f;
        float ColorPickerCursorCenterSize = 4.f;

        // HotKey
        float HotkeyDotSizeMin = 6.f;
        float HotkeyDotSizeRatio = 0.4f;
        float HotkeyModePopupWidth = 100.f;

        // 窗口/菜单栏/标题栏
        float TitleBarMinHeight = 30.f;
        float TitleBarPaddingY = 10.f;
        float TitleBarTextOffsetX = 10.f;
        float TitleBarTextOffsetY = 7.f;
        float MenuBarBorderThickness = 1.f;

        // Tooltip
        float TooltipOffset = 15.f;
        float TooltipMinSize = 30.f;

        // TabBar
        float TabBarTabSpacing = 5.f;
        float TabBarSeparatorHeight = 2.f;
        float TabBarScrollbarReserve = 4.f;
        float TabBarScrollSpeed = 30.f;

        // 滚动条/列表
        float ScrollbarThumbMinSize = 20.f;
        float ScrollSpeed = 30.f;
        float ListBoxDefaultHeightItems = 5.f;

        // InputText
        float InputTextCursorWidth = 2.f;
        float InputTextSelectionPaddingY = 2.f;
        float InputTextMinWidth = 50.f;
        int   InputTextCursorBlinkIntervalMS = 500;
        float KeyRepeatDelay = 0.250f;               // 键盘长按连发初始延迟 (秒)
        float KeyRepeatRate = 0.050f;                // 键盘长按连发速率 (秒)

        // Menu
        float MenuArrowSizeRatio = 0.55f;
        float MenuArrowSpacing = 10.f;
        float MenuShortcutSpacing = 20.f;

        // Popup
        float PopupBorderInset = 1.f;
        float PopupFillInset = 2.f;
        float PopupInitialSize = 100.f;
        float PopupMinWidth = 10.f;
        float PopupHeightExtra = 4.f;

        // ResizeGrip
        float ResizeGripPad = 3.f;

        // 字体
        float NoSDFFontBaseSize = 100.f;
        float FallbackFontSize = 12.f;
        float FontSizeMin = 1.f;
        float FontSizeMax = 10000.f;

        // 悬停延迟
        float HoverDelayShortMS = 150.f;
        float HoverDelayNormalMS = 400.f;
        float HoverSharedDelayMS = 250.f;
        float MouseStationaryMoveThreshold = 2.f;   // 平方距离阈值 4.0f 的平方根
        float MouseStationaryTimeMS = 150.f;

        float SeparatorHeight = 4.f;
        float SeparatorThickness = 1.f;

        float InputFloatEmptyThreshold = 0.000001f;

        float TextShadowOffsetX = 1.0f;
        float TextShadowOffsetY = 1.0f;
    };

    struct TabDisplayInfo {
        size_t id;
        Vec2 pos;
        Vec2 size;
        std::string display;
        SDK::UFont* font = nullptr;
        float fontScale = 1.0f;
        bool noSDF = false;
    };

    struct FontContext {
        SDK::UFont* Font;
        float Scale;
        bool NoSDF = false;
    };

    struct TextOutlineContext {
        bool Outline = true;
        Color OutlineColor = { 0.f, 0.f, 0.f, 1.f };
    };

    enum class ShadowDrawCmdType {
        Line,
        Rect,
        RectFilled,
        Text,
        TriangleFilled,
        Triangle,
        Texture
    };

    struct ShadowDrawCmd {
        ShadowDrawCmdType type;
        Vec2 pos;
        Vec2 size;
        Color color;
        float thickness;
        std::string text;
        SDK::UFont* font;
        float fontScale;
        bool clippingEnabled;
        Vec2 clipMin;
        Vec2 clipMax;
        Vec2 p1, p2, p3;
        Color textShadowColor;
        Color textOutlineColor;
        bool textOutline;
        bool noSDF;
        SDK::UTexture* texture = nullptr;
    };

    struct ShadowDrawList {
        static constexpr size_t PreallocMemorySizeKB = 1024;
        static constexpr size_t PreallocMemorySize = PreallocMemorySizeKB * 1024;
        std::vector<ShadowDrawCmd> CmdBuffer;

        int _ChannelsCurrent = 0;
        int _ChannelsCount = 1;
        std::vector<std::vector<ShadowDrawCmd>> _Channels;

        std::vector<ShadowDrawCmd>& GetCmdBuffer() {
            if (_ChannelsCount > 1 && _ChannelsCurrent >= 0 && _ChannelsCurrent < static_cast<int>(_Channels.size())) {
                return _Channels[_ChannelsCurrent];
            }
            return CmdBuffer;
        }

        ShadowDrawList() {
            // sizeof(ShadowDrawCmd) 大约为 160 字节，此处预分配将会直接在堆上开辟 1MB 左右的空间
            CmdBuffer.reserve(PreallocMemorySize / sizeof(ShadowDrawCmd));
            // 预分配通道数组，避免首次 ChannelsSplit 时重新分配导致闪烁
            _Channels.resize(8);
        }

        void Clear() {
            CmdBuffer.clear();
            _ChannelsCount = 1;
            _ChannelsCurrent = 0;
            for (auto& ch : _Channels) {
                ch.clear();
            }
        }

        void ChannelsSplit(int count);
        void SetChannel(int channel_idx);
        void ChannelsMerge();

        void AddLine(Vec2 start, Vec2 end, Color color, float thickness = 1.0f);
        void AddRect(Vec2 pos, Vec2 size, Color color, float thickness = 1.0f);
        void AddRectFilled(Vec2 pos, Vec2 size, Color color);
        void AddCircleFilled(Vec2 center, float radius, Color color);
        void AddTexture(Vec2 pos, Vec2 size, Color color, SDK::UTexture* texture = nullptr);
        void AddTriangle(Vec2 p1, Vec2 p2, Vec2 p3, Color color, float thickness = 1.0f);
        void AddTriangleFilled(Vec2 p1, Vec2 p2, Vec2 p3, Color color);
        void AddText(Vec2 pos, Color color, std::string_view text);
        void AddText(SDK::UFont* font, float fontScale, Color shadowColor, Color outlineColor, Vec2 pos, Color color, std::string_view text, bool outline = false, bool noSDF = false);
    };

    struct ListBoxState {
        Vec2 ParentWindowPos;
        Vec2 ParentWindowSize;
        Vec2 ParentCursor;
        float ParentContentStartY;
        float ParentScrollY;
        float ParentContentHeight;
        bool ParentIsScrollApplied;
        ShadowWindowFlags ParentWindowFlags;
        float ParentCurrentScrollbarWidth;
        float ParentIndentX;

        size_t Id;
        Vec2 Pos;
        Vec2 Size;
    };

    struct ShadowWindow {
        size_t Id = 0;
        std::string Name;
        Vec2 Pos = { 100.f, 100.f };
        Vec2 Size = { 500.f, 400.f };
        bool IsDragging = false;
        Vec2 DragOffset = { 0.f, 0.f };
        bool IsResizing = false;
        Vec2 ResizeStartPos = { 0.f, 0.f };
        Vec2 ResizeStartSize = { 0.f, 0.f };
        bool IsHoveringResize = false;
        float ScrollY = 0.f;
        float ContentHeight = 0.f;
        bool IsDraggingScrollbar = false;
        float ScrollDragOffset = 0.f;
        float ContentStartY = 0.f;
        bool IsScrollApplied = false;
        ShadowWindowFlags CurrentWindowFlags = ShadowWindowFlags_None;
        float CurrentScrollbarWidth = 0.f;

        ShadowDrawList DrawList;
        uint64_t LastAccessedFrame = 0;
    };

    enum class RightAlignCmdType {
        RectBackground,
        TriangleArrow,
        TextShortcut
    };

    struct PopupRightAlignCmd {
        size_t CmdIndex;
        RightAlignCmdType CmdType;
    };

    struct PopupBackupState {
        size_t Id;
        Vec2 WindowPos;
        Vec2 WindowSize;
        Vec2 Cursor;
        float ContentStartY;
        float ScrollY;
        float LastItemMaxX;
        bool IsScrollApplied;
        ShadowWindowFlags CurrentWindowFlags;
        float IndentX;
        bool ClippingEnabled;
        Vec2 ClipMin;
        Vec2 ClipMax;
        std::vector<std::pair<Vec2, Vec2>> ClipStack;
        ShadowWindow* CurrentWindow;

        bool IsDragging;
        Vec2 DragOffset;

        bool Closed;

        // 消除1帧闪烁：记录当前弹窗背景绘制指令的索引以及初始裁剪框
        size_t BgBorderCmdIdx;
        size_t BgFilledCmdIdx;
        Vec2 PopupOldClipMax;
        float PopupOldWidth;
        std::vector<PopupRightAlignCmd> RightAlignCmds;
    };

    struct MenuState {
        bool IsOpen;
    };

    struct GuiContext {
        SDK::UCanvas* Canvas = nullptr;
        SDK::UFont* DefaultFont = nullptr;

        GuiStyle Style;
        bool StyleInitialized = false;

        double RealTimeSeconds = 0.0;
        double DeltaTime = 0.0;

        Vec2 MousePos = { 0.f, 0.f };
        bool MouseDown = false;
        bool MouseClicked = false;
        bool RightMouseDown = false;
        bool RightMouseClicked = false;
        bool MiddleMouseDown = false;
        bool MiddleMouseClicked = false;
        bool MouseClickedThisFrame = false;
        bool RightMouseClickedThisFrame = false;
        bool MiddleMouseClickedThisFrame = false;
        float MouseWheel = 0.f;

        bool KeyStates[256] = { false };
        bool HotkeyToggles[256] = { false };
        bool KeyPressed[256] = { false };
        double KeyPressTime[256] = { 0.0 };
        int* AssigningHotkey = nullptr;

        std::vector<size_t> ActivePopups;
        std::vector<PopupBackupState> PopupStack;
        bool HasNextWindowSize = false;
        Vec2 NextWindowSize = { 0.f, 0.f };

        std::unordered_map<size_t, ShadowWindow> Windows;
        std::vector<size_t> WindowDisplayOrder;
        ShadowWindow* CurrentWindow = nullptr;
        uint64_t FrameCount = 0;
        size_t HoveredWindowId = 0;
        size_t FocusedWindowId = 0;

        bool HasNextWindowPos = false;
        Vec2 NextWindowPos = { 0.f, 0.f };

        bool InPopup = false;

        ShadowDrawList TooltipDrawList;
        ShadowDrawList BackgroundDrawList;
        ShadowDrawList ForegroundDrawList;

        Vec2 WindowPos = { 100.f, 100.f };
        Vec2 WindowSize = { 500.f, 400.f };
        bool IsDragging = false;
        Vec2 DragOffset = { 0.f, 0.f };

        bool HasWindowSizeConstraints = false;
        Vec2 WindowSizeConstraintMin = { 0.f, 0.f };
        Vec2 WindowSizeConstraintMax = { 10000.f, 10000.f };

        std::vector<FontContext> FontStack;
        std::vector<SDK::UTexture*> TextureStack;
        std::vector<TextOutlineContext> TextOutlineStack;
        std::unordered_map<SDK::UFont*, int32_t> FontOriginalSizes;

        int BeginStack = 0;
        int TabBarStack = 0;
        int TabItemStack = 0;
        int TreeNodeStack = 0;
        std::vector<size_t> IDStack;
        std::string LastErrorMsg;

        int MenuBarStack = 0;
        int MenuStack = 0;
        std::vector<MenuState> MenuStateStack;
        bool MenuBarClickedThisFrame = false;

        Vec2 BackupMenuBarCursor = { 0.f, 0.f };
        float BackupMenuBarLastItemMaxX = 0.f;
        Vec2 BackupMenuBarClipMin = { 0.f, 0.f };
        Vec2 BackupMenuBarClipMax = { 0.f, 0.f };
        bool BackupMenuBarClippingEnabled = false;

        bool IsResizing = false;
        Vec2 ResizeStartPos = { 0.f, 0.f };
        Vec2 ResizeStartSize = { 0.f, 0.f };
        bool IsHoveringResize = false;

        bool ClippingEnabled = false;
        Vec2 ClipMin = { 0.f, 0.f };
        Vec2 ClipMax = { 0.f, 0.f };
        std::vector<std::pair<Vec2, Vec2>> ClipStack;

        Vec2 Cursor = { 0.f, 0.f };
        float ItemHeight = 20.f;

        float ScrollY = 0.f;
        float ContentHeight = 0.f;
        bool IsDraggingScrollbar = false;
        float ScrollDragOffset = 0.f;
        float ContentStartY = 0.f;
        bool IsScrollApplied = false;

        std::unordered_map<size_t, size_t> ActiveTabIdMap;
        bool InActiveTab = true;
        Vec2 TabCursor = { 0.f, 0.f };

        float* ColorPickerR = nullptr;
        float* ColorPickerG = nullptr;
        float* ColorPickerB = nullptr;
        float* ColorPickerA = nullptr;
        float ColorPickerH = 0.f;
        float ColorPickerS = 0.f;
        float ColorPickerV = 0.f;

        bool IsDraggingSV = false;
        bool IsDraggingHue = false;
        bool IsDraggingAlpha = false;
        bool IsTypingHex = false;
        std::string HexInputBuffer;

        size_t DraggingSliderId = 0;
        size_t FocusedSliderId = 0;

        size_t ActiveInputId = 0;
        int InputCursorPos = 0;
        int InputSelectionStart = -1;
        int InputSelectionEnd = -1;
        std::vector<char> InputChars;
        std::unordered_map<size_t, std::string> InputBuffers;

        std::unordered_map<size_t, float> SliderInputWidthCache;
        std::unordered_map<size_t, size_t> SliderInputLengthCache;

        ShadowWindowFlags CurrentWindowFlags = ShadowWindowFlags_None;
        ShadowTabBarFlags CurrentTabBarFlags = ShadowTabBarFlags_None;
        size_t CurrentTabBarId = 0;

        std::unordered_map<size_t, std::vector<size_t>> TabOrderMap;
        std::unordered_map<size_t, std::vector<size_t>> TabAppearedThisFrame;

        std::unordered_map<size_t, std::vector<TabDisplayInfo>> TabBarDisplayCache;
        std::unordered_map<size_t, std::unordered_map<size_t, float>> TabBarLayoutX;
        std::unordered_map<size_t, float> TabWidthCache;

        size_t DraggingTabId = 0;
        size_t DraggingTabBarId = 0;
        float DraggingTabGrabOffsetX = 0.f;
        float DraggingTabCurrentX = 0.f;

        std::unordered_map<size_t, float> TabBarScrollX;
        size_t DraggingTabBarScrollId = 0;
        float TabBarScrollDragOffset = 0.f;

        Vec2 TabBarOrigin = { 0.f, 0.f };
        float TabBarViewWidth = 0.f;
        float TabBarContentWidth = 0.f;
        float TabBarContentWidthAccum = 0.f;
        bool TabBarNeedsScrollbar = false;

        std::vector<std::pair<Vec2, Vec2>> TabBarHoverRects;
        std::vector<std::pair<Vec2, Vec2>> TabBarHoverRectsPending;

        float LastItemMaxX = 0.f;
        std::vector<bool> DisabledStack;

        Vec2 LastItemMin = { 0.f, 0.f };
        Vec2 LastItemMax = { 0.f, 0.f };
        bool LastItemDisabled = false;
        size_t LastItemId = 0;
        bool LastItemClicked[ShadowMouseButton_COUNT] = { false };

        size_t ActiveId = 0;
        size_t ActiveIdPreviousFrame = 0;

        size_t HoveredIdPreviousFrame = 0;
        size_t HoveredIdCurrentFrame = 0;
        size_t LastHoveredIdEval = 0;
        uint64_t HoveredIdTimerStart = 0;
        bool HoveredIdStationaryTriggered = false;
        bool HoveredIdDelayTriggered = false;

        Vec2 MousePosPrev = { 0.f, 0.f };
        uint64_t MouseStationaryStartTime = 0;
        bool MouseIsStationary = false;

        uint64_t SharedDelayExpirationTime = 0;
        bool SharedDelayActive = false;

        int WidgetCount = 0;

        std::unordered_map<size_t, Vec2> TooltipSizeCache;
        Vec2 CurrentTooltipSize = { 0.f, 0.f };
        bool InTooltip = false;

        Vec2 BackupWindowPos = { 0.f, 0.f };
        Vec2 BackupWindowSize = { 0.f, 0.f };
        Vec2 BackupCursor = { 0.f, 0.f };
        float BackupContentStartY = 0.f;
        float BackupScrollY = 0.f;
        float BackupLastItemMaxX = 0.f;
        bool BackupIsScrollApplied = false;
        ShadowWindowFlags BackupCurrentWindowFlags = ShadowWindowFlags_None;

        float CurrentScrollbarWidth = 0.f;

        std::vector<float> TextWrapPosStack;
        std::vector<bool> TextPixelSnapStack;

        std::vector<GuiStyleMod> StyleVarStack;
        std::vector<GuiColorMod> StyleColorStack;

        std::unordered_map<size_t, bool> TreeNodeOpenStates;
        float IndentX = 0.f;
        float BackupIndentX = 0.f;
        std::vector<bool> TreeNodeNoIndentStack;

        bool BackupClippingEnabled = false;
        Vec2 BackupClipMin = { 0.f, 0.f };
        Vec2 BackupClipMax = { 0.f, 0.f };
        std::vector<std::pair<Vec2, Vec2>> BackupClipStack;

        int ListBoxStack = 0;
        std::vector<ListBoxState> ListBoxStateStack;
        std::unordered_map<size_t, float> ListBoxScrollY;
        std::unordered_map<size_t, float> ListBoxContentHeight;
        size_t DraggingListBoxScrollId = 0;
        float ListBoxScrollDragOffset = 0.f;

        size_t HoveredListBoxIdCurrentFrame = 0;
        size_t HoveredListBoxIdPreviousFrame = 0;
    };

    struct ShadowIO {
        Vec2 DisplaySize;
        float DeltaTime;
    };

    inline ShadowIO g_IO;
    inline GuiContext g_Ctx;
    inline SDK::UFont*& DefaultFont = g_Ctx.DefaultFont;

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
            auto it = g_Ctx.FontOriginalSizes.find(font);
            if (it == g_Ctx.FontOriginalSizes.end()) {
                OriginalSize = font->LegacyFontSize;
                g_Ctx.FontOriginalSizes[font] = OriginalSize;
            }
            else {
                OriginalSize = it->second;
            }

            int32_t baseSize = OriginalSize > 0 ? OriginalSize : static_cast<int32_t>(g_Ctx.Style.FallbackFontSize);
            float finalScale = fontScale * g_Ctx.Style.FontScaleDpi;

            if (NoSDF) {
                // 确保字号为 100（仅在不等于 100 时执行写入）
                if (font->LegacyFontSize != static_cast<int32_t>(g_Ctx.Style.NoSDFFontBaseSize)) {
                    font->LegacyFontSize = static_cast<int32_t>(g_Ctx.Style.NoSDFFontBaseSize);
                }
                // 计算期望尺寸并 clamp，计算出相对于 100pt 字体所需的缩放倍率
                float desiredSize = std::clamp(static_cast<float>(baseSize) * finalScale, g_Ctx.Style.FontSizeMin, g_Ctx.Style.FontSizeMax);
                CalculatedScale = desiredSize / g_Ctx.Style.NoSDFFontBaseSize;
            }
            else {
                // 普通模式：直接修改 LegacyFontSize，scale 保持 1.0f
                int32_t targetSize = std::clamp(static_cast<int32_t>(baseSize * finalScale), static_cast<int32_t>(g_Ctx.Style.FontSizeMin), static_cast<int32_t>(g_Ctx.Style.FontSizeMax));
                font->LegacyFontSize = targetSize;
                CalculatedScale = 1.0f;
            }
        }

        ~ScopedFontScale() {
            // 仅在非 NoSDF 模式退出作用域时恢复原始字号，NoSDF 保持 100
            if (Font && !NoSDF) {
                Font->LegacyFontSize = OriginalSize;
            }
        }
    };

    inline ShadowIO& GetIO() {
        g_IO.DisplaySize = {
            g_Ctx.Canvas ? static_cast<float>(g_Ctx.Canvas->SizeX) : 0.0f,
            g_Ctx.Canvas ? static_cast<float>(g_Ctx.Canvas->SizeY) : 0.0f
        };
        g_IO.DeltaTime = static_cast<float>(g_Ctx.DeltaTime);

        return g_IO;
    }

    inline ShadowDrawList* GetWindowDrawList() {
        if (g_Ctx.InTooltip) return &g_Ctx.TooltipDrawList;
        if (g_Ctx.CurrentWindow) return &g_Ctx.CurrentWindow->DrawList;
        return &g_Ctx.BackgroundDrawList;
    }

    inline ShadowDrawList* GetBackgroundDrawList() {
        return &g_Ctx.BackgroundDrawList;
    }

    inline ShadowDrawList* GetForegroundDrawList() {
        return &g_Ctx.ForegroundDrawList;
    }

    inline GuiStyle& GetStyle() {
        return g_Ctx.Style;
    }

    // 深海主题
    inline void StyleColorsOcean() {
        auto& colors = g_Ctx.Style.Colors;

        colors[GuiCol_WindowBg] = { 0.003f, 0.005f, 0.009f, 1.000f };
        colors[GuiCol_PopupBg] = { 0.007f, 0.012f, 0.022f, 0.980f };
        colors[GuiCol_TitleBarBg] = { 0.002f, 0.004f, 0.007f, 1.000f };

        colors[GuiCol_Text] = { 0.750f, 0.800f, 0.860f, 1.000f };
        colors[GuiCol_TextHighlight] = { 1.000f, 1.000f, 1.000f, 1.000f };
        colors[GuiCol_TextDisabled] = { 0.150f, 0.180f, 0.220f, 1.000f };

        colors[GuiCol_FrameBg] = { 0.006f, 0.010f, 0.018f, 1.000f };
        colors[GuiCol_FrameBgHovered] = { 0.010f, 0.016f, 0.028f, 1.000f };

        colors[GuiCol_Button] = { 0.012f, 0.022f, 0.040f, 1.000f };
        colors[GuiCol_ButtonHovered] = { 0.022f, 0.038f, 0.068f, 1.000f };

        colors[GuiCol_Tab] = { 0.000f, 0.000f, 0.000f, 0.000f };
        colors[GuiCol_TabHovered] = { 0.010f, 0.018f, 0.032f, 1.000f };
        colors[GuiCol_TabActive] = { 0.016f, 0.030f, 0.055f, 1.000f };

        colors[GuiCol_SliderGrab] = { 0.025f, 0.180f, 0.520f, 1.000f };
        colors[GuiCol_SliderKnob] = { 0.900f, 0.950f, 1.000f, 1.000f };

        colors[GuiCol_CheckMark] = { 0.025f, 0.180f, 0.520f, 1.000f };
        colors[GuiCol_ActiveIndicator] = { 0.025f, 0.180f, 0.520f, 1.000f };
        colors[GuiCol_InactiveIndicator] = { 0.020f, 0.030f, 0.045f, 1.000f };

        colors[GuiCol_Border] = { 0.015f, 0.024f, 0.040f, 0.800f };
        colors[GuiCol_PopupBorder] = { 0.025f, 0.045f, 0.080f, 0.900f };
        colors[GuiCol_Separator] = { 0.010f, 0.016f, 0.026f, 1.000f };

        colors[GuiCol_ResizeGrip] = { 0.012f, 0.022f, 0.040f, 1.000f };
        colors[GuiCol_ResizeGripActive] = { 0.025f, 0.180f, 0.520f, 1.000f };
        colors[GuiCol_ResizeGripHovered] = { 0.040f, 0.250f, 0.650f, 1.000f };

        colors[GuiCol_ErrorText] = { 0.950f, 0.100f, 0.100f, 1.000f };
        colors[GuiCol_TextShadow] = { 0.000f, 0.000f, 0.000f, 0.500f };
        colors[GuiCol_TextOutline] = { 0.000f, 0.000f, 0.000f, 0.000f };
        colors[GuiCol_ColorPickerDark] = { 0.000f, 0.000f, 0.000f, 1.000f };
        colors[GuiCol_ColorPickerLight] = { 1.000f, 1.000f, 1.000f, 1.000f };

        colors[GuiCol_CheckerboardLight] = { 1.000f, 1.000f, 1.000f, 1.000f };
        colors[GuiCol_CheckerboardDark] = { 0.400f, 0.400f, 0.400f, 1.000f };
        colors[GuiCol_ColorPickerShadow] = { 0.000f, 0.000f, 0.000f, 1.000f };

        colors[GuiCol_ControlDisabled] = { 0.006f, 0.010f, 0.018f, 0.500f };

        colors[GuiCol_SwitchBg] = { 0.018f, 0.026f, 0.038f, 1.000f };
        colors[GuiCol_SwitchBgHovered] = { 0.028f, 0.040f, 0.058f, 1.000f };
        colors[GuiCol_SwitchBgActive] = { 0.025f, 0.180f, 0.520f, 1.000f };
        colors[GuiCol_SwitchBgActiveHovered] = { 0.040f, 0.250f, 0.650f, 1.000f };
        colors[GuiCol_SwitchKnob] = { 0.850f, 0.900f, 0.950f, 1.000f };

        colors[GuiCol_DropdownActive] = { 0.022f, 0.055f, 0.115f, 1.000f };

        colors[GuiCol_Transparent] = { 0.000f, 0.000f, 0.000f, 0.000f };
    }

    // 紫曜主题
    inline void StyleColorsAmethyst() {
        auto& colors = g_Ctx.Style.Colors;

        colors[GuiCol_WindowBg] = { 0.008f, 0.006f, 0.014f, 0.960f };
        colors[GuiCol_PopupBg] = { 0.010f, 0.008f, 0.018f, 0.980f };
        colors[GuiCol_TitleBarBg] = { 0.005f, 0.004f, 0.009f, 1.000f };

        colors[GuiCol_Text] = { 0.820f, 0.790f, 0.880f, 1.000f };
        colors[GuiCol_TextHighlight] = { 1.000f, 1.000f, 1.000f, 1.000f };
        colors[GuiCol_TextDisabled] = { 0.180f, 0.155f, 0.240f, 1.000f };

        colors[GuiCol_FrameBg] = { 0.020f, 0.014f, 0.036f, 0.900f };
        colors[GuiCol_FrameBgHovered] = { 0.035f, 0.024f, 0.062f, 0.950f };

        colors[GuiCol_Button] = { 0.028f, 0.019f, 0.050f, 0.900f };
        colors[GuiCol_ButtonHovered] = { 0.055f, 0.036f, 0.105f, 0.950f };

        colors[GuiCol_Tab] = { 0.010f, 0.007f, 0.018f, 1.000f };
        colors[GuiCol_TabHovered] = { 0.040f, 0.026f, 0.075f, 0.900f };
        colors[GuiCol_TabActive] = { 0.024f, 0.016f, 0.045f, 1.000f };

        colors[GuiCol_SliderGrab] = { 0.220f, 0.085f, 0.820f, 1.000f };
        colors[GuiCol_SliderKnob] = { 0.920f, 0.900f, 0.980f, 1.000f };

        colors[GuiCol_CheckMark] = { 0.260f, 0.110f, 0.920f, 1.000f };
        colors[GuiCol_ActiveIndicator] = { 0.260f, 0.110f, 0.920f, 1.000f };
        colors[GuiCol_InactiveIndicator] = { 0.030f, 0.022f, 0.050f, 1.000f };

        colors[GuiCol_Border] = { 0.065f, 0.042f, 0.125f, 0.650f };
        colors[GuiCol_PopupBorder] = { 0.120f, 0.070f, 0.250f, 0.850f };
        colors[GuiCol_Separator] = { 0.040f, 0.026f, 0.075f, 0.650f };

        colors[GuiCol_ResizeGrip] = { 0.028f, 0.019f, 0.050f, 0.500f };
        colors[GuiCol_ResizeGripActive] = { 0.260f, 0.110f, 0.920f, 1.000f };
        colors[GuiCol_ResizeGripHovered] = { 0.150f, 0.065f, 0.450f, 0.850f };

        colors[GuiCol_ErrorText] = { 0.900f, 0.080f, 0.120f, 1.000f };
        colors[GuiCol_TextShadow] = { 0.000f, 0.000f, 0.000f, 0.700f };
        colors[GuiCol_TextOutline] = { 0.000f, 0.000f, 0.000f, 0.000f };
        colors[GuiCol_ColorPickerDark] = { 0.000f, 0.000f, 0.000f, 1.000f };
        colors[GuiCol_ColorPickerLight] = { 1.000f, 1.000f, 1.000f, 1.000f };

        colors[GuiCol_CheckerboardLight] = { 1.000f, 1.000f, 1.000f, 1.000f };
        colors[GuiCol_CheckerboardDark] = { 0.450f, 0.450f, 0.450f, 1.000f };
        colors[GuiCol_ColorPickerShadow] = { 0.000f, 0.000f, 0.000f, 1.000f };

        colors[GuiCol_ControlDisabled] = { 0.015f, 0.011f, 0.025f, 0.450f };

        colors[GuiCol_SwitchBg] = { 0.020f, 0.014f, 0.036f, 0.900f };
        colors[GuiCol_SwitchBgHovered] = { 0.035f, 0.024f, 0.062f, 0.950f };
        colors[GuiCol_SwitchBgActive] = { 0.220f, 0.085f, 0.820f, 1.000f };
        colors[GuiCol_SwitchBgActiveHovered] = { 0.320f, 0.150f, 0.950f, 1.000f };
        colors[GuiCol_SwitchKnob] = { 0.920f, 0.900f, 0.980f, 1.000f };

        colors[GuiCol_DropdownActive] = { 0.045f, 0.030f, 0.085f, 0.950f };

        colors[GuiCol_Transparent] = { 0.000f, 0.000f, 0.000f, 0.000f };
    }

    // 黑暗主题
    inline void StyleColorsDark() {
        auto& colors = g_Ctx.Style.Colors;

        colors[GuiCol_WindowBg] = { 0.003f, 0.003f, 0.004f, 1.000f };
        colors[GuiCol_PopupBg] = { 0.004f, 0.004f, 0.005f, 0.980f };
        colors[GuiCol_TitleBarBg] = { 0.002f, 0.002f, 0.003f, 1.000f };

        colors[GuiCol_Text] = { 0.850f, 0.850f, 0.880f, 1.000f };
        colors[GuiCol_TextHighlight] = { 1.000f, 1.000f, 1.000f, 1.000f };
        colors[GuiCol_TextDisabled] = { 0.140f, 0.140f, 0.160f, 1.000f };

        colors[GuiCol_FrameBg] = { 0.006f, 0.006f, 0.008f, 1.000f };
        colors[GuiCol_FrameBgHovered] = { 0.010f, 0.010f, 0.013f, 1.000f };

        colors[GuiCol_Button] = { 0.010f, 0.010f, 0.013f, 1.000f };
        colors[GuiCol_ButtonHovered] = { 0.020f, 0.020f, 0.025f, 1.000f };

        colors[GuiCol_Tab] = { 0.000f, 0.000f, 0.000f, 0.000f };
        colors[GuiCol_TabHovered] = { 0.008f, 0.008f, 0.010f, 1.000f };
        colors[GuiCol_TabActive] = { 0.015f, 0.015f, 0.018f, 1.000f };

        colors[GuiCol_SliderGrab] = { 0.008f, 0.160f, 0.920f, 1.000f };
        colors[GuiCol_SliderKnob] = { 1.000f, 1.000f, 1.000f, 1.000f };

        colors[GuiCol_CheckMark] = { 0.008f, 0.160f, 0.920f, 1.000f };
        colors[GuiCol_ActiveIndicator] = { 0.008f, 0.160f, 0.920f, 1.000f };
        colors[GuiCol_InactiveIndicator] = { 0.030f, 0.030f, 0.035f, 1.000f };

        colors[GuiCol_Border] = { 0.015f, 0.015f, 0.018f, 0.600f };
        colors[GuiCol_PopupBorder] = { 0.020f, 0.020f, 0.025f, 0.800f };
        colors[GuiCol_Separator] = { 0.012f, 0.012f, 0.015f, 1.000f };

        colors[GuiCol_ResizeGrip] = { 0.010f, 0.010f, 0.013f, 1.000f };
        colors[GuiCol_ResizeGripActive] = { 0.008f, 0.160f, 0.920f, 1.000f };
        colors[GuiCol_ResizeGripHovered] = { 0.015f, 0.220f, 0.980f, 1.000f };

        colors[GuiCol_ErrorText] = { 0.900f, 0.050f, 0.050f, 1.000f };
        colors[GuiCol_TextShadow] = { 0.000f, 0.000f, 0.000f, 1.000f };
        colors[GuiCol_TextOutline] = { 0.000f, 0.000f, 0.000f, 1.000f };
        colors[GuiCol_ColorPickerDark] = { 0.000f, 0.000f, 0.000f, 1.000f };
        colors[GuiCol_ColorPickerLight] = { 1.000f, 1.000f, 1.000f, 1.000f };

        colors[GuiCol_CheckerboardLight] = { 1.000f, 1.000f, 1.000f, 1.000f };
        colors[GuiCol_CheckerboardDark] = { 0.400f, 0.400f, 0.400f, 1.000f };
        colors[GuiCol_ColorPickerShadow] = { 0.000f, 0.000f, 0.000f, 1.000f };

        colors[GuiCol_ControlDisabled] = { 0.006f, 0.006f, 0.008f, 0.500f };

        colors[GuiCol_SwitchBg] = { 0.030f, 0.030f, 0.035f, 1.000f };
        colors[GuiCol_SwitchBgHovered] = { 0.045f, 0.045f, 0.052f, 1.000f };
        colors[GuiCol_SwitchBgActive] = { 0.008f, 0.160f, 0.920f, 1.000f };
        colors[GuiCol_SwitchBgActiveHovered] = { 0.015f, 0.220f, 0.980f, 1.000f };
        colors[GuiCol_SwitchKnob] = { 0.900f, 0.900f, 0.950f, 1.000f };

        colors[GuiCol_DropdownActive] = { 0.015f, 0.015f, 0.018f, 1.000f };

        colors[GuiCol_Transparent] = { 0.000f, 0.000f, 0.000f, 0.000f };
    }

    // 灰色主题
    inline void StyleColorsGrey() {
        auto& colors = g_Ctx.Style.Colors;

        colors[GuiCol_WindowBg] = { 0.006f, 0.006f, 0.006f, 0.720f };
        colors[GuiCol_PopupBg] = { 0.008f, 0.008f, 0.008f, 0.900f };
        colors[GuiCol_TitleBarBg] = { 0.004f, 0.004f, 0.004f, 0.900f };

        colors[GuiCol_Text] = { 0.700f, 0.700f, 0.700f, 1.000f };
        colors[GuiCol_TextHighlight] = { 0.700f, 0.700f, 0.700f, 0.850f };
        colors[GuiCol_TextDisabled] = { 0.150f, 0.150f, 0.150f, 1.000f };

        colors[GuiCol_FrameBg] = { 0.008f, 0.008f, 0.008f, 0.800f };
        colors[GuiCol_FrameBgHovered] = { 0.020f, 0.020f, 0.020f, 0.900f };

        colors[GuiCol_Button] = { 0.010f, 0.010f, 0.010f, 0.800f };
        colors[GuiCol_ButtonHovered] = { 0.700f, 0.700f, 0.700f, 0.850f };

        colors[GuiCol_Tab] = { 0.006f, 0.006f, 0.006f, 0.850f };
        colors[GuiCol_TabHovered] = { 0.040f, 0.040f, 0.040f, 1.000f };
        colors[GuiCol_TabActive] = { 0.700f, 0.700f, 0.700f, 0.850f };

        colors[GuiCol_SliderGrab] = { 0.080f, 0.080f, 0.080f, 1.000f };
        colors[GuiCol_SliderKnob] = { 0.700f, 0.700f, 0.700f, 1.000f };

        colors[GuiCol_CheckMark] = { 0.700f, 0.700f, 0.700f, 0.900f };
        colors[GuiCol_ActiveIndicator] = { 0.080f, 0.080f, 0.080f, 1.000f };
        colors[GuiCol_InactiveIndicator] = { 0.020f, 0.020f, 0.020f, 1.000f };

        colors[GuiCol_Border] = { 0.025f, 0.025f, 0.025f, 0.400f };
        colors[GuiCol_PopupBorder] = { 0.035f, 0.035f, 0.035f, 0.600f };
        colors[GuiCol_Separator] = { 0.070f, 0.070f, 0.070f, 0.700f };

        colors[GuiCol_ResizeGrip] = { 0.010f, 0.010f, 0.010f, 1.000f };
        colors[GuiCol_ResizeGripActive] = { 0.080f, 0.080f, 0.080f, 1.000f };
        colors[GuiCol_ResizeGripHovered] = { 0.150f, 0.150f, 0.150f, 1.000f };

        colors[GuiCol_ErrorText] = { 0.900f, 0.080f, 0.080f, 1.000f };
        colors[GuiCol_TextShadow] = { 0.000f, 0.000f, 0.000f, 0.800f };
        colors[GuiCol_TextOutline] = { 0.000f, 0.000f, 0.000f, 0.000f };
        colors[GuiCol_ColorPickerDark] = { 0.000f, 0.000f, 0.000f, 1.000f };
        colors[GuiCol_ColorPickerLight] = { 1.000f, 1.000f, 1.000f, 1.000f };

        colors[GuiCol_CheckerboardLight] = { 1.000f, 1.000f, 1.000f, 1.000f };
        colors[GuiCol_CheckerboardDark] = { 0.400f, 0.400f, 0.400f, 1.000f };
        colors[GuiCol_ColorPickerShadow] = { 0.000f, 0.000f, 0.000f, 1.000f };

        colors[GuiCol_ControlDisabled] = { 0.006f, 0.006f, 0.006f, 0.500f };

        colors[GuiCol_SwitchBg] = { 0.020f, 0.020f, 0.020f, 1.000f };
        colors[GuiCol_SwitchBgHovered] = { 0.040f, 0.040f, 0.040f, 1.000f };
        colors[GuiCol_SwitchBgActive] = { 0.700f, 0.700f, 0.700f, 0.850f };
        colors[GuiCol_SwitchBgActiveHovered] = { 0.800f, 0.800f, 0.800f, 0.900f };
        colors[GuiCol_SwitchKnob] = { 0.010f, 0.010f, 0.010f, 1.000f };

        colors[GuiCol_DropdownActive] = { 0.700f, 0.700f, 0.700f, 0.850f };

        colors[GuiCol_Transparent] = { 0.000f, 0.000f, 0.000f, 0.000f };
    }

    inline Vec2 GetWindowSize() {
        return g_Ctx.WindowSize;
    }

    inline Vec2 GetWindowPos() {
        return g_Ctx.WindowPos;
    }

    inline void SetWindowPos(Vec2 pos) {
        if (g_Ctx.CurrentWindow) {
            g_Ctx.CurrentWindow->Pos = pos;
            g_Ctx.WindowPos = pos;
        }
        else {
            g_Ctx.HasNextWindowPos = true;
            g_Ctx.NextWindowPos = pos;
        }
    }

    inline void SetNextWindowPos(Vec2 pos) {
        g_Ctx.HasNextWindowPos = true;
        g_Ctx.NextWindowPos = pos;
    }

    inline void SetNextWindowSizeConstraints(Vec2 min_size, Vec2 max_size) {
        g_Ctx.HasWindowSizeConstraints = true;
        g_Ctx.WindowSizeConstraintMin = min_size;
        g_Ctx.WindowSizeConstraintMax = max_size;
    }

    inline void SetNextWindowSize(Vec2 size) {
        g_Ctx.HasNextWindowSize = true;
        g_Ctx.NextWindowSize = size;
    }

    inline Vec2 GetCursorStartPos() {
        return { g_Ctx.WindowPos.x + g_Ctx.Style.WindowPadding.x, g_Ctx.ContentStartY };
    }

    inline Vec2 GetCursorScreenPos() {
        return g_Ctx.Cursor;
    }

    inline void SetCursorScreenPos(Vec2 pos) {
        g_Ctx.Cursor = pos;
    }

    inline Vec2 GetCursorPos() {
        return {
            g_Ctx.Cursor.x - (g_Ctx.WindowPos.x + g_Ctx.Style.WindowPadding.x),
            g_Ctx.Cursor.y - g_Ctx.ContentStartY + (g_Ctx.IsScrollApplied ? g_Ctx.ScrollY : 0.f)
        };
    }

    inline float GetCursorPosX() {
        return GetCursorPos().x;
    }

    inline float GetCursorPosY() {
        return GetCursorPos().y;
    }

    inline void SetCursorPosX(float x) {
        g_Ctx.Cursor.x = g_Ctx.WindowPos.x + g_Ctx.Style.WindowPadding.x + x;
    }

    inline void SetCursorPosY(float y) {
        g_Ctx.Cursor.y = g_Ctx.ContentStartY + y - (g_Ctx.IsScrollApplied ? g_Ctx.ScrollY : 0.f);
    }

    inline void SetCursorPos(Vec2 pos) {
        SetCursorPosX(pos.x);
        SetCursorPosY(pos.y);
    }

    inline float GetScrollX() {
        if (g_Ctx.CurrentTabBarId != 0) {
            auto it = g_Ctx.TabBarScrollX.find(g_Ctx.CurrentTabBarId);
            if (it != g_Ctx.TabBarScrollX.end()) {
                return it->second;
            }
        }
        return 0.f;
    }

    inline float GetScrollY() {
        return g_Ctx.ScrollY;
    }

    inline float GetScrollMaxX() {
        if (g_Ctx.CurrentTabBarId != 0) {
            return std::max(0.f, g_Ctx.TabBarContentWidth - g_Ctx.TabBarViewWidth);
        }
        return 0.f;
    }

    inline float GetScrollMaxY() {
        if (!g_Ctx.ListBoxStateStack.empty()) {
            size_t id = g_Ctx.ListBoxStateStack.back().Id;
            float boxHeight = g_Ctx.ListBoxStateStack.back().Size.y;
            auto it = g_Ctx.ListBoxContentHeight.find(id);
            float contentHeight = (it != g_Ctx.ListBoxContentHeight.end()) ? it->second : 0.f;
            return std::max(0.f, contentHeight - boxHeight);
        }

        float viewHeight = (g_Ctx.WindowPos.y + g_Ctx.WindowSize.y) - g_Ctx.ContentStartY - g_Ctx.Style.ResizeGripSize - g_Ctx.Style.WindowScrollBottomPadding;
        if (viewHeight < g_Ctx.Style.WindowScrollMinViewHeight) viewHeight = g_Ctx.Style.WindowScrollMinViewHeight;
        return std::max(0.f, g_Ctx.ContentHeight - viewHeight);
    }

    // 允许放行透传给游戏的按键列表
    inline std::vector<int> AllowedKeys;
    inline void SetAllowedKeys(const std::vector<int>& keys) {
        AllowedKeys = keys;
    }

    // 允许放行透传给游戏的鼠标消息列表
    inline std::vector<UINT> AllowedMouseMsgs;
    inline void SetAllowedMouseMsgs(const std::vector<UINT>& msgs) {
        AllowedMouseMsgs = msgs;
    }

    inline bool IsKeyAllowed(int key) {
        return std::find(AllowedKeys.begin(), AllowedKeys.end(), key) != AllowedKeys.end();
    }

    inline bool IsMouseMsgAllowed(UINT uMsg) {
        // 先判断它到底是不是鼠标相关消息
        bool isMouseMsg = (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) || (uMsg == WM_INPUT);

        // 如果不是鼠标消息，直接返回 false
        if (!isMouseMsg) {
            return false;
        }

        return std::find(AllowedMouseMsgs.begin(), AllowedMouseMsgs.end(), uMsg) != AllowedMouseMsgs.end();
    }

    inline constexpr size_t HashString(std::string_view str) noexcept {
        size_t hash = 14695981039346656037ULL;
        for (char c : str) {
            hash ^= static_cast<size_t>(c);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    inline std::wstring ToWString(std::string_view utf8_str) {
        if (utf8_str.empty()) return L"";
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), static_cast<int>(utf8_str.size()), nullptr, 0);
        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), static_cast<int>(utf8_str.size()), &wstr[0], size_needed);
        return wstr;
    }

    // VK键码转字符串名称
    inline std::string GetKeyName(int vk) {
        if (vk == 0) return "None";
        switch (vk) {
        case VK_LBUTTON: return "Mouse 1";
        case VK_RBUTTON: return "Mouse 2";
        case VK_MBUTTON: return "Mouse 3";
        case VK_XBUTTON1: return "Mouse 4";
        case VK_XBUTTON2: return "Mouse 5";
        case VK_BACK: return "Backspace";
        case VK_TAB: return "Tab";
        case VK_RETURN: return "Enter";
        case VK_SHIFT: return "Shift";
        case VK_CONTROL: return "Ctrl";
        case VK_MENU: return "Alt";
        case VK_CAPITAL: return "Caps Lock";
        case VK_ESCAPE: return "Esc";
        case VK_SPACE: return "Space";
        case VK_PRIOR: return "Page Up";
        case VK_NEXT: return "Page Down";
        case VK_END: return "End";
        case VK_HOME: return "Home";
        case VK_LEFT: return "Left";
        case VK_UP: return "Up";
        case VK_RIGHT: return "Right";
        case VK_DOWN: return "Down";
        case VK_INSERT: return "Insert";
        case VK_DELETE: return "Delete";
        case VK_LWIN: return "LWin";
        case VK_RWIN: return "RWin";
        case VK_NUMPAD0: return "Num 0";
        case VK_NUMPAD1: return "Num 1";
        case VK_NUMPAD2: return "Num 2";
        case VK_NUMPAD3: return "Num 3";
        case VK_NUMPAD4: return "Num 4";
        case VK_NUMPAD5: return "Num 5";
        case VK_NUMPAD6: return "Num 6";
        case VK_NUMPAD7: return "Num 7";
        case VK_NUMPAD8: return "Num 8";
        case VK_NUMPAD9: return "Num 9";
        case VK_MULTIPLY: return "Num *";
        case VK_ADD: return "Num +";
        case VK_SUBTRACT: return "Num -";
        case VK_DECIMAL: return "Num .";
        case VK_DIVIDE: return "Num /";
        case VK_F1: return "F1";
        case VK_F2: return "F2";
        case VK_F3: return "F3";
        case VK_F4: return "F4";
        case VK_F5: return "F5";
        case VK_F6: return "F6";
        case VK_F7: return "F7";
        case VK_F8: return "F8";
        case VK_F9: return "F9";
        case VK_F10: return "F10";
        case VK_F11: return "F11";
        case VK_F12: return "F12";
        case VK_LSHIFT: return "LShift";
        case VK_RSHIFT: return "RShift";
        case VK_LCONTROL: return "LCtrl";
        case VK_RCONTROL: return "RCtrl";
        case VK_LMENU: return "LAlt";
        case VK_RMENU: return "RAlt";
        }
        if (vk >= '0' && vk <= '9') return std::string(1, static_cast<char>(vk));
        if (vk >= 'A' && vk <= 'Z') return std::string(1, static_cast<char>(vk));
        return std::format("Unk {:X}", vk);
    }

    inline void RGBtoHSV(float r, float g, float b, float& h, float& s, float& v) {
        float max_val = std::max({ r, g, b });
        float min_val = std::min({ r, g, b });
        v = max_val;
        float delta = max_val - min_val;
        if (max_val > 0.0f) { s = delta / max_val; }
        else { s = 0.0f; h = 0.0f; return; }
        if (delta == 0.0f) { h = 0.0f; return; }
        if (r >= max_val) { h = (g - b) / delta; }
        else if (g >= max_val) { h = 2.0f + (b - r) / delta; }
        else { h = 4.0f + (r - g) / delta; }
        h *= 60.0f;
        if (h < 0.0f) h += 360.0f;
        h /= 360.0f;
    }

    inline void HSVtoRGB(float h, float s, float v, float& r, float& g, float& b) {
        if (s <= 0.0f) { r = g = b = v; return; }
        h *= 360.0f;
        if (h >= 360.0f) h = 0.0f;
        h /= 60.0f;
        int i = static_cast<int>(h);
        float ff = h - i;
        float p = v * (1.0f - s);
        float q = v * (1.0f - (s * ff));
        float t = v * (1.0f - (s * (1.0f - ff)));
        switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: default: r = v; g = p; b = q; break;
        }
    }

    inline bool IsMouseHoveringRaw(Vec2 pos, Vec2 size) {
        return g_Ctx.MousePos.x >= pos.x && g_Ctx.MousePos.x <= pos.x + size.x &&
            g_Ctx.MousePos.y >= pos.y && g_Ctx.MousePos.y <= pos.y + size.y;
    }

    inline bool IsMouseHovering(Vec2 pos, Vec2 size) {
        if (g_Ctx.CurrentWindowFlags & ShadowWindowFlags_NoMouseInputs) {
            return false;
        }

        // 窗口 Z 轴遮挡处理：不属于当前悬停窗口的背景元素直接屏蔽交互（排除拖拽操作本身）
        if (!g_Ctx.InPopup && !g_Ctx.InTooltip && g_Ctx.CurrentWindow) {
            if (g_Ctx.HoveredWindowId != g_Ctx.CurrentWindow->Id) {
                bool activeInThisWindow = g_Ctx.IsDragging || g_Ctx.IsResizing || g_Ctx.IsDraggingScrollbar ||
                    g_Ctx.DraggingSliderId != 0 || g_Ctx.DraggingTabId != 0 ||
                    g_Ctx.DraggingTabBarScrollId != 0 || g_Ctx.DraggingListBoxScrollId != 0;

                if (!activeInThisWindow) {
                    return false;
                }
            }
        }

        // 弹窗存在且当前正在绘制背景控件时屏蔽交互（弹窗优先级最高，多层级弹窗互相遮挡判定）
        if (!g_Ctx.ActivePopups.empty()) {
            if (!g_Ctx.InPopup) {
                return false; // 不是任何弹窗内的控件，直接屏蔽
            }
            if (!g_Ctx.PopupStack.empty()) {
                size_t currentPopupId = g_Ctx.PopupStack.back().Id;
                bool isObscured = false;
                bool foundCurrent = false;
                for (size_t pid : g_Ctx.ActivePopups) {
                    if (foundCurrent) {
                        // 在 ActivePopups 里排名靠后的属于更顶层的弹窗
                        auto& higherWin = g_Ctx.Windows[pid];
                        if (IsMouseHoveringRaw(higherWin.Pos, higherWin.Size)) {
                            isObscured = true;
                            break;
                        }
                    }
                    if (pid == currentPopupId) foundCurrent = true;
                }
                if (isObscured) return false;
            }
        }

        return IsMouseHoveringRaw(pos, size);
    }

    inline bool TryAssignKey(int vk) {
        if (g_Ctx.AssigningHotkey) {
            if (vk == VK_ESCAPE) {
                *g_Ctx.AssigningHotkey = 0;
            }
            else {
                *g_Ctx.AssigningHotkey = vk;
            }
            g_Ctx.AssigningHotkey = nullptr;
            return true;
        }
        return false;
    }

    inline bool HandleKeyDown(int vk, bool isRepeat = false) {
        if (TryAssignKey(vk)) return true;
        if (!g_Ctx.KeyStates[vk]) {
            g_Ctx.HotkeyToggles[vk] = !g_Ctx.HotkeyToggles[vk];
            g_Ctx.KeyPressed[vk] = true; // 仅在初次按下的那一帧置为 true
            g_Ctx.KeyPressTime[vk] = g_Ctx.RealTimeSeconds;
        }
        // 移除对 isRepeat 的 KeyPressed 赋值，完全交由框架的高精度时钟接管连发
        g_Ctx.KeyStates[vk] = true;
        return false;
    }

    inline void HandleKeyUp(int vk) {
        g_Ctx.KeyStates[vk] = false;
        g_Ctx.KeyPressTime[vk] = 0.0;
    }

    inline void ApplyHexInput(size_t hexId) {
        auto it = g_Ctx.InputBuffers.find(hexId);
        if (it == g_Ctx.InputBuffers.end()) return;
        const std::string& hex = it->second;

        if (hex.size() < 6) return;

        uint8_t rgba[4] = { 0, 0, 0, 255 };
        size_t count = std::min(hex.size() / 2, size_t(4));
        for (size_t i = 0; i < count; ++i) {
            uint8_t val = 0;
            auto [ptr, ec] = std::from_chars(hex.data() + i * 2, hex.data() + i * 2 + 2, val, 16);
            if (ec != std::errc() || ptr != hex.data() + i * 2 + 2) {
                return;
            }
            rgba[i] = val;
        }

        *g_Ctx.ColorPickerR = rgba[0] / 255.f;
        *g_Ctx.ColorPickerG = rgba[1] / 255.f;
        *g_Ctx.ColorPickerB = rgba[2] / 255.f;
        if (g_Ctx.ColorPickerA) *g_Ctx.ColorPickerA = rgba[3] / 255.f;

        RGBtoHSV(*g_Ctx.ColorPickerR, *g_Ctx.ColorPickerG, *g_Ctx.ColorPickerB,
            g_Ctx.ColorPickerH, g_Ctx.ColorPickerS, g_Ctx.ColorPickerV);
    }

    // 新增：存储热键的完整信息（包括模式指针和is_active指针）
    struct RegisteredHotkeyInfo {
        int* hotkey;
        HotkeyMode* mode;
        bool* is_active;
    };

    // 替换原来的简单vector为带完整信息的vector
    inline std::vector<RegisteredHotkeyInfo> g_RegisteredHotkeyInfos;

    // 新增：检查某个按键是否是已注册的热键
    inline bool IsHotkeyRegistered(int vk) {
        for (auto& info : g_RegisteredHotkeyInfos) {
            if (*info.hotkey == vk && vk != 0) {
                return true;
            }
        }
        return false;
    }

    // 新增：更新所有已注册热键的 is_active 状态（每帧调用，无论菜单是否打开）
    inline void UpdateAllHotkeyStates() {
        for (auto& info : g_RegisteredHotkeyInfos) {
            if (info.is_active && info.mode) {
                switch (*info.mode) {
                case HotkeyMode::None:
                    *info.is_active = false;
                    break;
                case HotkeyMode::HoldOn:
                    *info.is_active = (*info.hotkey != 0) && g_Ctx.KeyStates[*info.hotkey];
                    break;
                case HotkeyMode::HoldOff:
                    *info.is_active = (*info.hotkey != 0) && !g_Ctx.KeyStates[*info.hotkey];
                    break;
                case HotkeyMode::ToggleOn:
                    *info.is_active = (*info.hotkey != 0) && g_Ctx.HotkeyToggles[*info.hotkey];
                    break;
                case HotkeyMode::AlwaysOn:
                    *info.is_active = true;
                    break;
                }
            }
            else if (info.is_active) {
                // 没有模式指针（简单版本的HotKey），is_active 就是 hotkey 是否被按下
                *info.is_active = (*info.hotkey != 0) && g_Ctx.KeyStates[*info.hotkey];
            }
        }
    }

    // 新增：处理全局热键输入（菜单关闭时也需要调用）
    inline bool ProcessGlobalHotkeys(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) {
            if (wParam < 256) {
                int vk = static_cast<int>(wParam);
                if (IsHotkeyRegistered(vk)) {
                    bool isRepeat = (lParam & (1 << 30)) != 0;
                    HandleKeyDown(vk, isRepeat);
                    return true;
                }
            }
        }
        else if (uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP) {
            if (wParam < 256) {
                int vk = static_cast<int>(wParam);
                if (IsHotkeyRegistered(vk)) {
                    HandleKeyUp(vk);
                    return true;
                }
            }
        }
        return false;
    }

    // 注册函数
    inline void RegisterHotkey(int* hotkey, HotkeyMode* mode, bool* is_active) {
        // 检查是否已注册（通过hotkey指针判断）
        for (auto& info : g_RegisteredHotkeyInfos) {
            if (info.hotkey == hotkey) {
                // 已注册，更新信息
                info.mode = mode;
                info.is_active = is_active;
                return;
            }
        }
        // 未注册，添加新条目
        g_RegisteredHotkeyInfos.push_back({ hotkey, mode, is_active });
    }

    inline LRESULT Input(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_MOUSEWHEEL: {
            short zDelta = HIWORD(wParam);
            g_Ctx.MouseWheel += static_cast<float>(zDelta) / 120.f;
            break;
        }
        case WM_MOUSEMOVE:
            g_Ctx.MousePos.x = static_cast<float>(LOWORD(lParam));
            g_Ctx.MousePos.y = static_cast<float>(HIWORD(lParam));
            break;
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONDOWN: {
            bool wasDown = g_Ctx.KeyStates[VK_LBUTTON];
            if (HandleKeyDown(VK_LBUTTON)) return 0;
            g_Ctx.MouseDown = true;
            if (!wasDown) {
                g_Ctx.MouseClicked = true;
            }
            break;
        }
        case WM_LBUTTONUP:
            HandleKeyUp(VK_LBUTTON);
            g_Ctx.MouseDown = false;
            g_Ctx.IsDragging = false;
            g_Ctx.IsResizing = false;
            break;
        case WM_RBUTTONDBLCLK:
        case WM_RBUTTONDOWN: {
            bool wasDown = g_Ctx.KeyStates[VK_RBUTTON];
            if (HandleKeyDown(VK_RBUTTON)) return 0;
            g_Ctx.RightMouseDown = true;
            if (!wasDown) {
                g_Ctx.RightMouseClicked = true;
            }
            break;
        }
        case WM_RBUTTONUP:
            HandleKeyUp(VK_RBUTTON);
            g_Ctx.RightMouseDown = false;
            break;
        case WM_MBUTTONDBLCLK:
        case WM_MBUTTONDOWN: {
            bool wasDown = g_Ctx.KeyStates[VK_MBUTTON];
            if (HandleKeyDown(VK_MBUTTON)) return 0;
            g_Ctx.MiddleMouseDown = true;
            if (!wasDown) {
                g_Ctx.MiddleMouseClicked = true;
            }
            break;
        }
        case WM_MBUTTONUP:
            HandleKeyUp(VK_MBUTTON);
            g_Ctx.MiddleMouseDown = false;
            break;
        case WM_XBUTTONDBLCLK:
        case WM_XBUTTONDOWN: {
            int vk = (HIWORD(wParam) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
            if (HandleKeyDown(vk)) return 0;
            break;
        }
        case WM_XBUTTONUP: {
            int vk = (HIWORD(wParam) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
            HandleKeyUp(vk);
            break;
        }
        case WM_CHAR: {
            // 放行可打印字符，供 Slider 和 InputText 直接录入
            if (wParam >= 32 && wParam < 127) {
                g_Ctx.InputChars.push_back(static_cast<char>(wParam));
            }
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wParam < 256) {
                int vk = static_cast<int>(wParam);
                if (!g_Ctx.KeyStates[vk]) {
                    g_Ctx.KeyPressTime[vk] = g_Ctx.RealTimeSeconds;
                }
                if (!IsHotkeyRegistered(vk)) {
                    bool isRepeat = (lParam & (1 << 30)) != 0;
                    if (HandleKeyDown(vk, isRepeat)) return 0;
                }
                if (g_Ctx.ActiveInputId != 0) return 0;
            }
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (wParam < 256) {
                int vk = static_cast<int>(wParam);
                g_Ctx.KeyPressTime[vk] = 0.0;
                if (!IsHotkeyRegistered(vk)) {
                    HandleKeyUp(vk);
                }
                if (g_Ctx.ActiveInputId != 0) return 0;
            }
            break;
        }
        return 0;
    }

    inline void CloseCurrentPopup() {
        if (g_Ctx.ActivePopups.empty())
            return;

        // 如果当前处于弹窗上下文，标记栈顶备份为已关闭
        if (g_Ctx.InPopup && !g_Ctx.PopupStack.empty()) {
            auto& backup = g_Ctx.PopupStack.back();
            backup.Closed = true;
        }

        g_Ctx.ActivePopups.pop_back();
    }

    inline void PushClipRect(Vec2 min, Vec2 max) {
        g_Ctx.ClipStack.push_back({ min, max });
        g_Ctx.ClippingEnabled = true;

        if (g_Ctx.ClipStack.size() > 1) {
            Vec2 prevMin = g_Ctx.ClipMin;
            Vec2 prevMax = g_Ctx.ClipMax;
            g_Ctx.ClipMin.x = std::max(prevMin.x, min.x);
            g_Ctx.ClipMin.y = std::max(prevMin.y, min.y);
            g_Ctx.ClipMax.x = std::min(prevMax.x, max.x);
            g_Ctx.ClipMax.y = std::min(prevMax.y, max.y);
        }
        else {
            g_Ctx.ClipMin = min;
            g_Ctx.ClipMax = max;
        }
    }

    inline void PopClipRect() {
        if (!g_Ctx.ClipStack.empty()) {
            g_Ctx.ClipStack.pop_back();
        }

        if (g_Ctx.ClipStack.empty()) {
            g_Ctx.ClippingEnabled = false;
        }
        else {
            // 从头重建当前的剪裁交集区域
            g_Ctx.ClipMin = g_Ctx.ClipStack[0].first;
            g_Ctx.ClipMax = g_Ctx.ClipStack[0].second;
            for (size_t i = 1; i < g_Ctx.ClipStack.size(); ++i) {
                g_Ctx.ClipMin.x = std::max(g_Ctx.ClipMin.x, g_Ctx.ClipStack[i].first.x);
                g_Ctx.ClipMin.y = std::max(g_Ctx.ClipMin.y, g_Ctx.ClipStack[i].first.y);
                g_Ctx.ClipMax.x = std::min(g_Ctx.ClipMax.x, g_Ctx.ClipStack[i].second.x);
                g_Ctx.ClipMax.y = std::min(g_Ctx.ClipMax.y, g_Ctx.ClipStack[i].second.y);
            }
        }
    }

    inline bool IsRectVisible(Vec2 pos, Vec2 size) {
        if (!g_Ctx.ClippingEnabled) return true;
        if (pos.x + size.x < g_Ctx.ClipMin.x || pos.x > g_Ctx.ClipMax.x ||
            pos.y + size.y < g_Ctx.ClipMin.y || pos.y > g_Ctx.ClipMax.y) {
            return false;
        }
        return true;
    }

    inline bool IsRectFullyVisible(Vec2 pos, Vec2 size) {
        if (!g_Ctx.ClippingEnabled) return true;
        return pos.x >= g_Ctx.ClipMin.x && pos.y >= g_Ctx.ClipMin.y &&
            (pos.x + size.x) <= g_Ctx.ClipMax.x && (pos.y + size.y) <= g_Ctx.ClipMax.y;
    }

    inline void ClipRect(Vec2& pos, Vec2& size) {
        if (!g_Ctx.ClippingEnabled) return;

        if (pos.x < g_Ctx.ClipMin.x) {
            size.x -= (g_Ctx.ClipMin.x - pos.x);
            pos.x = g_Ctx.ClipMin.x;
        }
        if (pos.y < g_Ctx.ClipMin.y) {
            size.y -= (g_Ctx.ClipMin.y - pos.y);
            pos.y = g_Ctx.ClipMin.y;
        }
        if (pos.x + size.x > g_Ctx.ClipMax.x) {
            size.x = g_Ctx.ClipMax.x - pos.x;
        }
        if (pos.y + size.y > g_Ctx.ClipMax.y) {
            size.y = g_Ctx.ClipMax.y - pos.y;
        }

        if (size.x < 0.f) size.x = 0.f;
        if (size.y < 0.f) size.y = 0.f;
    }

    inline void InternalDrawLine(Vec2 start, Vec2 end, Color color, float thickness, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, SDK::UTexture* texture = nullptr) {
        if (!g_Ctx.Canvas) return;
        SDK::UTexture* tex = texture ? texture : (g_Ctx.Canvas ? g_Ctx.Canvas->DefaultTexture : nullptr);
        if (!tex) return;

        // 新增：Liang-Barsky 线段裁剪算法
        // 如果开启了裁剪，对线段端点进行裁剪
        if (clipEnabled) {
            float dx = end.x - start.x;
            float dy = end.y - start.y;
            float t0 = 0.0f, t1 = 1.0f;

            auto ClipTest = [](float p, float q, float& t0, float& t1) -> bool {
                if (p == 0.0f) {
                    // 线段平行于边界且位于边界外
                    return q >= 0.0f;
                }
                float r = q / p;
                if (p < 0.0f) {
                    // 从外部进入
                    if (r > t1) return false;
                    if (r > t0) t0 = r;
                }
                else {
                    // 从内部离开
                    if (r < t0) return false;
                    if (r < t1) t1 = r;
                }
                return true;
                };

            // 针对四条边界进行裁剪测试
            if (!ClipTest(-dx, start.x - clipMin.x, t0, t1)) return;
            if (!ClipTest(dx, clipMax.x - start.x, t0, t1)) return;
            if (!ClipTest(-dy, start.y - clipMin.y, t0, t1)) return;
            if (!ClipTest(dy, clipMax.y - start.y, t0, t1)) return;

            // 如果 t1 < 1.0f，说明 end 点需要被裁剪
            if (t1 < 1.0f) {
                end.x = start.x + t1 * dx;
                end.y = start.y + t1 * dy;
            }
            // 如果 t0 > 0.0f，说明 start 点需要被裁剪
            if (t0 > 0.0f) {
                start.x += t0 * dx;
                start.y += t0 * dy;
            }
        }

        // 重新计算 dx, dy，因为 start 和 end 可能已经被修改
        float dx = end.x - start.x;
        float dy = end.y - start.y;
        float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.0f) return;

        float angle = std::atan2(dy, dx) * (180.0f / 3.14159265358979323846f);

        SDK::FVector2D uePos{ static_cast<float>(start.x), static_cast<float>(start.y) };
        SDK::FVector2D ueSize{ static_cast<float>(length), static_cast<float>(thickness) };
        SDK::FLinearColor ueColor{ color.r, color.g, color.b, color.a };

        g_Ctx.Canvas->K2_DrawTexture(
            tex,
            uePos, ueSize,
            SDK::FVector2D{ 0.0f, 0.0f }, SDK::FVector2D{ 1.0f, 1.0f },
            ueColor, SDK::EBlendMode::BLEND_Translucent,
            angle, SDK::FVector2D{ 0.0f, 0.5f }
        );
    }

    inline void InternalDrawRect(Vec2 pos, Vec2 size, Color color, float thickness, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, SDK::UTexture* texture = nullptr) {
        if (!g_Ctx.Canvas) return;
        if (size.x <= 0.f || size.y <= 0.f) return;

        // 1. AABB 完全在外部直接剔除
        if (clipEnabled) {
            if (pos.x + size.x < clipMin.x || pos.x > clipMax.x ||
                pos.y + size.y < clipMin.y || pos.y > clipMax.y)
                return;
        }

        float halfThick = thickness * 0.5f;

        // 1. 上边 (水平线)
        InternalDrawLine({ pos.x - halfThick, pos.y }, { pos.x + size.x + halfThick, pos.y }, color, thickness, clipEnabled, clipMin, clipMax, texture);

        // 2. 下边 (水平线)
        InternalDrawLine({ pos.x - halfThick, pos.y + size.y }, { pos.x + size.x + halfThick, pos.y + size.y }, color, thickness, clipEnabled, clipMin, clipMax, texture);

        // 3. 左边 (垂直线)
        InternalDrawLine({ pos.x, pos.y + halfThick }, { pos.x, pos.y + size.y - halfThick }, color, thickness, clipEnabled, clipMin, clipMax, texture);

        // 4. 右边 (垂直线)
        InternalDrawLine({ pos.x + size.x, pos.y + halfThick }, { pos.x + size.x, pos.y + size.y - halfThick }, color, thickness, clipEnabled, clipMin, clipMax, texture);
    }

    inline void InternalDrawRectFilled(Vec2 pos, Vec2 size, Color color, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, SDK::UTexture* texture = nullptr) {
        if (!g_Ctx.Canvas) return;
        SDK::UTexture* tex = texture ? texture : g_Ctx.Canvas->DefaultTexture;
        if (!tex) return;

        if (clipEnabled) {
            if (pos.x + size.x < clipMin.x || pos.x > clipMax.x || pos.y + size.y < clipMin.y || pos.y > clipMax.y) return;
            if (pos.x < clipMin.x) { size.x -= (clipMin.x - pos.x); pos.x = clipMin.x; }
            if (pos.y < clipMin.y) { size.y -= (clipMin.y - pos.y); pos.y = clipMin.y; }
            if (pos.x + size.x > clipMax.x) size.x = clipMax.x - pos.x;
            if (pos.y + size.y > clipMax.y) size.y = clipMax.y - pos.y;
            if (size.x <= 0.f || size.y <= 0.f) return;
        }

        SDK::FLinearColor ueColor{ color.r, color.g, color.b, color.a };
        SDK::FVector2D uePos{ static_cast<float>(pos.x), static_cast<float>(pos.y) };
        SDK::FVector2D ueSize{ static_cast<float>(size.x), static_cast<float>(size.y) };

        g_Ctx.Canvas->K2_DrawTexture(
            tex, uePos, ueSize,
            SDK::FVector2D{ 0.0f, 0.0f }, SDK::FVector2D{ 1.0f, 1.0f },
            ueColor, SDK::EBlendMode::BLEND_Translucent, 0.0f, SDK::FVector2D{ 0.0f, 0.0f }
        );
    }

    inline void InternalDrawText(const std::string& text, Vec2 pos, Color color, SDK::UFont* font, float fontScale, Color textShadowColor, Color textOutlineColor, bool textOutline, bool noSDF = false) {
        if (!g_Ctx.Canvas || !font) return;

        ScopedFontScale fontGuard(font, fontScale, noSDF);

        SDK::FVector2D scale{ fontGuard.CalculatedScale, fontGuard.CalculatedScale };
        SDK::FLinearColor ueColor{ color.r, color.g, color.b, color.a };
        SDK::FVector2D uePos{ static_cast<float>(pos.x), static_cast<float>(pos.y) };

        SDK::FLinearColor shadow{ textShadowColor.r, textShadowColor.g, textShadowColor.b, textShadowColor.a };
        SDK::FLinearColor outline{ textOutlineColor.r, textOutlineColor.g, textOutlineColor.b, textOutlineColor.a };
        SDK::FVector2D shadowOff{ g_Ctx.Style.TextShadowOffsetX, g_Ctx.Style.TextShadowOffsetY };

        std::wstring wstr = ToWString(text);
        g_Ctx.Canvas->K2_DrawText(font, SDK::FString(wstr.c_str()), uePos, scale, ueColor, 0.0f, shadow, shadowOff, false, false, textOutline, outline);
    }

    inline void InternalDrawTriangleFilled(Vec2 p1, Vec2 p2, Vec2 p3, Color color, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, SDK::UTexture* texture = nullptr) {
        Vec2 v[3] = { p1, p2, p3 };
        if (v[0].y > v[1].y) std::swap(v[0], v[1]);
        if (v[0].y > v[2].y) std::swap(v[0], v[2]);
        if (v[1].y > v[2].y) std::swap(v[1], v[2]);
        if (std::abs(v[0].y - v[2].y) < 0.5f) return;

        auto interpolateX = [](float y, const Vec2& a, const Vec2& b) {
            if (std::abs(a.y - b.y) < 0.001f) return (a.x + b.x) * 0.5f;
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
                InternalDrawRectFilled({ x_min, static_cast<float>(y) }, { w, 1.0f }, color, clipEnabled, clipMin, clipMax, texture);
            }
        }
    }

    inline Vec2 MeasureTextSize(std::wstring_view text) {
        SDK::UFont* font = g_Ctx.DefaultFont;
        float scaleVal = 1.0f;
        bool noSDF = false;
        if (!g_Ctx.FontStack.empty()) {
            font = g_Ctx.FontStack.back().Font;
            scaleVal = g_Ctx.FontStack.back().Scale;
            noSDF = g_Ctx.FontStack.back().NoSDF;
        }
        if (!g_Ctx.Canvas || !font) return { 0.f, 0.f };

        ScopedFontScale fontGuard(font, scaleVal, noSDF);

        SDK::FVector2D scale{ fontGuard.CalculatedScale, fontGuard.CalculatedScale };

        std::wstring wstr(text);
        SDK::FVector2D size = g_Ctx.Canvas->K2_TextSize(font, SDK::FString(wstr.c_str()), scale);
        return { static_cast<float>(size.X), static_cast<float>(size.Y) };
    }

    inline Vec2 MeasureTextSize(std::string_view text) {
        return MeasureTextSize(ToWString(text));
    }

    inline float MeasureCharWidth(wchar_t ch) {
        return MeasureTextSize(std::wstring_view(&ch, 1)).x;
    }

    inline float MeasureTextHeight(std::string_view text) {
        if (text.empty()) return 0.f;

        Vec2 size = MeasureTextSize(text);
        return size.y;
    }

    inline void UpdateItemHeight() {
        Vec2 charSize = MeasureTextSize("A");
        g_Ctx.ItemHeight = charSize.y > 0.f ? charSize.y + g_Ctx.Style.FramePadding.y * 2.f : g_Ctx.Style.DefaultItemHeight;
    }

    inline void PushFont(SDK::UFont* font, float scale = 1.0f) {
        g_Ctx.FontStack.push_back({ font, scale, false });
        UpdateItemHeight();
    }

    inline void PopFont() {
        if (!g_Ctx.FontStack.empty()) {
            g_Ctx.FontStack.pop_back();
            UpdateItemHeight();
        }
    }

    inline void PushFontNoSDF(SDK::UFont* font, float scale = 1.0f) {
        if (font) {
            // 备份目标字体的原始 LegacyFontSize（若尚未备份）
            auto it = g_Ctx.FontOriginalSizes.find(font);
            if (it == g_Ctx.FontOriginalSizes.end()) {
                g_Ctx.FontOriginalSizes[font] = font->LegacyFontSize;
            }
            // 仅在 LegacyFontSize 不等于 100 时执行一次设置
            if (font->LegacyFontSize != 100) {
                font->LegacyFontSize = 100;
            }
        }
        g_Ctx.FontStack.push_back({ font, scale, true });
        UpdateItemHeight();
    }

    inline void PushTexture(SDK::UTexture* texture) {
        g_Ctx.TextureStack.push_back(texture);
    }

    inline void PopTexture() {
        if (!g_Ctx.TextureStack.empty()) {
            g_Ctx.TextureStack.pop_back();
        }
    }

    inline size_t MixID(size_t id) {
        if (!g_Ctx.IDStack.empty()) {
            id ^= g_Ctx.IDStack.back() + 0x9e3779b9 + (id << 6) + (id >> 2);
        }
        return id;
    }

    inline size_t GetID(std::string_view str_id) {
        return MixID(HashString(str_id));
    }

    inline size_t GetID(int int_id) {
        return MixID(std::hash<int>()(int_id));
    }

    inline size_t GetID(const void* ptr_id) {
        return MixID(std::hash<const void*>()(ptr_id));
    }

    inline void PushID(int int_id) {
        g_Ctx.IDStack.push_back(GetID(int_id));
    }

    inline void PushID(std::string_view str_id) {
        g_Ctx.IDStack.push_back(GetID(str_id));
    }

    inline void PushID(const void* ptr_id) {
        g_Ctx.IDStack.push_back(GetID(ptr_id));
    }

    inline void PopID() {
        if (!g_Ctx.IDStack.empty()) {
            g_Ctx.IDStack.pop_back();
        }
    }

    inline void ParseLabel(std::string_view raw, std::string_view& display_name, size_t& id) {
        id = GetID(raw);

        size_t pos = raw.find("##");
        display_name = (pos != std::string_view::npos) ? raw.substr(0, pos) : raw;
    }

    inline void OpenPopup(size_t id) {
        if (std::find(g_Ctx.ActivePopups.begin(), g_Ctx.ActivePopups.end(), id) == g_Ctx.ActivePopups.end()) {
            g_Ctx.ActivePopups.push_back(id);
        }
    }

    inline void OpenPopup(std::string_view name) {
        OpenPopup(GetID(name));
    }

    inline bool IsPopupOpen(size_t id) {
        return std::find(g_Ctx.ActivePopups.begin(), g_Ctx.ActivePopups.end(), id) != g_Ctx.ActivePopups.end();
    }

    inline bool IsPopupOpen(std::string_view name) {
        return IsPopupOpen(GetID(name));
    }

    inline void PushTextWrapPos(float wrap_pos_x = 0.0f) {
        // 如果传入的是 0.0f（默认参数）或负数，则使用当前窗口的默认右边界
        if (wrap_pos_x <= 0.0f) {
            float right_margin = g_Ctx.Style.WindowPadding.x + g_Ctx.CurrentScrollbarWidth + g_Ctx.Style.ScrollbarMargin;
            wrap_pos_x = g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - right_margin;
        }
        else {
            wrap_pos_x = g_Ctx.Cursor.x + wrap_pos_x;
        }

        // 如果堆栈不为空，需要将新的位置与上一个位置进行比较，取较小值，确保嵌套时换行越来越靠左。
        if (!g_Ctx.TextWrapPosStack.empty()) {
            wrap_pos_x = std::min(wrap_pos_x, g_Ctx.TextWrapPosStack.back());
        }

        g_Ctx.TextWrapPosStack.push_back(wrap_pos_x);
    }

    inline void PopTextWrapPos() {
        if (!g_Ctx.TextWrapPosStack.empty()) {
            g_Ctx.TextWrapPosStack.pop_back();
        }
    }

    inline bool IsDisabled() {
        return !g_Ctx.DisabledStack.empty() && g_Ctx.DisabledStack.back();
    }

    inline Color GetColor(int idx) {
        Color col = (idx >= 0 && idx < GuiCol_COUNT) ? g_Ctx.Style.Colors[idx] : Color{ 1.f, 1.f, 1.f, 1.f };
        if (IsDisabled()) {
            col.a *= g_Ctx.Style.DisabledAlpha;
        }
        col.a *= g_Ctx.Style.Alpha;
        return col;
    }

    inline Color GetColor(GuiCol_ idx) {
        return GetColor(static_cast<int>(idx));
    }

    inline Color GetColor(Color col) {
        if (IsDisabled()) {
            col.a *= g_Ctx.Style.DisabledAlpha;
        }
        col.a *= g_Ctx.Style.Alpha;
        return col;
    }

    inline void PushTextOutline(std::optional<Color> outlineColor = std::nullopt) {
        Color col = GetColor(GuiCol_TextOutline);
        if (!g_Ctx.TextOutlineStack.empty()) {
            col = g_Ctx.TextOutlineStack.back().OutlineColor;
        }
        if (outlineColor.has_value()) {
            col = outlineColor.value();
        }
        g_Ctx.TextOutlineStack.push_back({ true, col });
    }

    inline void PopTextOutline() {
        if (!g_Ctx.TextOutlineStack.empty()) {
            g_Ctx.TextOutlineStack.pop_back();
        }
    }

    inline void PushTextPixelSnap(bool snap = true) {
        g_Ctx.TextPixelSnapStack.push_back(snap);
    }

    inline void PopTextPixelSnap() {
        if (!g_Ctx.TextPixelSnapStack.empty()) {
            g_Ctx.TextPixelSnapStack.pop_back();
        }
    }

    inline void PushStyleColor(int idx, Color val) {
        if (idx >= 0 && idx < GuiCol_COUNT) {
            g_Ctx.StyleColorStack.push_back({ idx, g_Ctx.Style.Colors[idx] });
            g_Ctx.Style.Colors[idx] = val;
        }
    }

    inline void PopStyleColor(int count = 1) {
        while (count > 0 && !g_Ctx.StyleColorStack.empty()) {
            const auto& mod = g_Ctx.StyleColorStack.back();
            g_Ctx.Style.Colors[mod.ColIdx] = mod.BackupColor;
            g_Ctx.StyleColorStack.pop_back();
            count--;
        }
    }

    inline void PushStyleVar(GuiStyleVar idx, float val) {
        switch (idx) {
        case GuiStyleVar_Alpha:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.Alpha);
            g_Ctx.Style.Alpha = val;
            break;
        case GuiStyleVar_ScrollbarSize:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.ScrollbarSize);
            g_Ctx.Style.ScrollbarSize = val;
            break;
        case GuiStyleVar_ScrollbarMargin:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.ScrollbarMargin);
            g_Ctx.Style.ScrollbarMargin = val;
            break;
        case GuiStyleVar_ResizeGripSize:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.ResizeGripSize);
            g_Ctx.Style.ResizeGripSize = val;
            break;
        case GuiStyleVar_TabExtraWidth:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.TabExtraWidth);
            g_Ctx.Style.TabExtraWidth = val;
            break;
        case GuiStyleVar_ControlOffsetMin:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.ControlOffsetMin);
            g_Ctx.Style.ControlOffsetMin = val;
            break;
        case GuiStyleVar_ControlOffsetRatio:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.ControlOffsetRatio);
            g_Ctx.Style.ControlOffsetRatio = val;
            break;
        case GuiStyleVar_CPPadding:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.CPPadding);
            g_Ctx.Style.CPPadding = val;
            break;
        case GuiStyleVar_CPSVSize:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.CPSVSize);
            g_Ctx.Style.CPSVSize = val;
            break;
        case GuiStyleVar_CPHueWidth:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.CPHueWidth);
            g_Ctx.Style.CPHueWidth = val;
            break;
        case GuiStyleVar_CPAlphaWidth:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.CPAlphaWidth);
            g_Ctx.Style.CPAlphaWidth = val;
            break;
        case GuiStyleVar_CPSpacing:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.CPSpacing);
            g_Ctx.Style.CPSpacing = val;
            break;
        case GuiStyleVar_FontScaleDpi:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.FontScaleDpi);
            g_Ctx.Style.FontScaleDpi = val;
            break;
        case GuiStyleVar_IndentSpacing:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.IndentSpacing);
            g_Ctx.Style.IndentSpacing = val;
            break;
        default:
            break;
        }
    }

    inline void PushStyleVar(GuiStyleVar idx, Vec2 val) {
        switch (idx) {
        case GuiStyleVar_WindowPadding:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.WindowPadding);
            g_Ctx.Style.WindowPadding = val;
            break;
        case GuiStyleVar_FramePadding:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.FramePadding);
            g_Ctx.Style.FramePadding = val;
            UpdateItemHeight();
            break;
        case GuiStyleVar_ItemSpacing:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.ItemSpacing);
            g_Ctx.Style.ItemSpacing = val;
            break;
        case GuiStyleVar_WindowMinSize:
            g_Ctx.StyleVarStack.emplace_back(idx, g_Ctx.Style.WindowMinSize);
            g_Ctx.Style.WindowMinSize = val;
            break;
        default:
            break;
        }
    }

    inline void PopStyleVar(int count = 1) {
        while (count > 0 && !g_Ctx.StyleVarStack.empty()) {
            const auto& mod = g_Ctx.StyleVarStack.back();
            switch (mod.Idx) {
            case GuiStyleVar_Alpha:
                g_Ctx.Style.Alpha = mod.BackupFloat;
                break;
            case GuiStyleVar_WindowPadding:
                g_Ctx.Style.WindowPadding = mod.BackupVec2;
                break;
            case GuiStyleVar_FramePadding:
                g_Ctx.Style.FramePadding = mod.BackupVec2;
                UpdateItemHeight();
                break;
            case GuiStyleVar_ItemSpacing:
                g_Ctx.Style.ItemSpacing = mod.BackupVec2;
                break;
            case GuiStyleVar_ScrollbarSize:
                g_Ctx.Style.ScrollbarSize = mod.BackupFloat;
                break;
            case GuiStyleVar_ScrollbarMargin:
                g_Ctx.Style.ScrollbarMargin = mod.BackupFloat;
                break;
            case GuiStyleVar_ResizeGripSize:
                g_Ctx.Style.ResizeGripSize = mod.BackupFloat;
                break;
            case GuiStyleVar_TabExtraWidth:
                g_Ctx.Style.TabExtraWidth = mod.BackupFloat;
                break;
            case GuiStyleVar_ControlOffsetMin:
                g_Ctx.Style.ControlOffsetMin = mod.BackupFloat;
                break;
            case GuiStyleVar_ControlOffsetRatio:
                g_Ctx.Style.ControlOffsetRatio = mod.BackupFloat;
                break;
            case GuiStyleVar_CPPadding:
                g_Ctx.Style.CPPadding = mod.BackupFloat;
                break;
            case GuiStyleVar_CPSVSize:
                g_Ctx.Style.CPSVSize = mod.BackupFloat;
                break;
            case GuiStyleVar_CPHueWidth:
                g_Ctx.Style.CPHueWidth = mod.BackupFloat;
                break;
            case GuiStyleVar_CPAlphaWidth:
                g_Ctx.Style.CPAlphaWidth = mod.BackupFloat;
                break;
            case GuiStyleVar_CPSpacing:
                g_Ctx.Style.CPSpacing = mod.BackupFloat;
                break;
            case GuiStyleVar_WindowMinSize:
                g_Ctx.Style.WindowMinSize = mod.BackupVec2;
                break;
            case GuiStyleVar_FontScaleDpi:
                g_Ctx.Style.FontScaleDpi = mod.BackupFloat;
                break;
            case GuiStyleVar_IndentSpacing:
                g_Ctx.Style.IndentSpacing = mod.BackupFloat;
                break;
            default:
                break;
            }
            g_Ctx.StyleVarStack.pop_back();
            count--;
        }
    }

    inline std::string ClipTextString(std::string_view text, Vec2 pos, Vec2& outPos, bool& shouldDraw) {
        shouldDraw = true;
        outPos = pos;
        if (!g_Ctx.ClippingEnabled) return std::string(text);

        float textHeight = MeasureTextHeight(text);
        if (pos.y + textHeight > g_Ctx.ClipMax.y || pos.y < g_Ctx.ClipMin.y) {
            shouldDraw = false;
            return "";
        }

        std::wstring wstr = ToWString(text);
        if (wstr.empty() || pos.x >= g_Ctx.ClipMax.x) {
            shouldDraw = false;
            return "";
        }

        if (pos.x < g_Ctx.ClipMin.x) {
            float currentX = pos.x;
            std::wstring clippedWstr;
            bool foundStart = false;
            for (size_t i = 0; i < wstr.size(); ++i) {
                float charWidth = MeasureCharWidth(wstr[i]);
                if (!foundStart) {
                    if (currentX >= g_Ctx.ClipMin.x) {
                        foundStart = true;
                        clippedWstr += wstr[i];
                        outPos.x = currentX;
                    }
                }
                else {
                    clippedWstr += wstr[i];
                }
                currentX += charWidth;
            }
            if (!foundStart) {
                shouldDraw = false;
                return "";
            }
            wstr = clippedWstr;
        }

        float totalWidth = 0.f;
        for (wchar_t ch : wstr) totalWidth += MeasureCharWidth(ch);

        while (!wstr.empty() && outPos.x + totalWidth > g_Ctx.ClipMax.x) {
            wchar_t lastChar = wstr.back();
            float lastCharWidth = MeasureCharWidth(lastChar);
            totalWidth -= lastCharWidth;
            wstr.pop_back();
        }

        if (wstr.empty()) {
            shouldDraw = false;
            return "";
        }

        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
        std::string result(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &result[0], size_needed, nullptr, nullptr);
        shouldDraw = true;
        return result;
    }

    inline void InternalDrawTriangle(Vec2 p1, Vec2 p2, Vec2 p3, Color color, float thickness, bool clipEnabled, Vec2 clipMin, Vec2 clipMax, SDK::UTexture* texture = nullptr) {
        InternalDrawLine(p1, p2, color, thickness, clipEnabled, clipMin, clipMax, texture);
        InternalDrawLine(p2, p3, color, thickness, clipEnabled, clipMin, clipMax, texture);
        InternalDrawLine(p3, p1, color, thickness, clipEnabled, clipMin, clipMax, texture);
    }

    inline void ShadowDrawList::AddLine(Vec2 start, Vec2 end, Color color, float thickness) {
        GetCmdBuffer().push_back({ ShadowDrawCmdType::Line, start, end, color, thickness, "", nullptr, 1.0f, g_Ctx.ClippingEnabled, g_Ctx.ClipMin, g_Ctx.ClipMax, {0,0}, {0,0}, {0,0}, {0,0,0,0}, {0,0,0,0}, false, false, !g_Ctx.TextureStack.empty() ? g_Ctx.TextureStack.back() : nullptr });
    }

    inline void ShadowDrawList::AddRect(Vec2 pos, Vec2 size, Color color, float thickness) {
        GetCmdBuffer().push_back({ ShadowDrawCmdType::Rect, pos, size, color, thickness, "", nullptr, 1.0f, g_Ctx.ClippingEnabled, g_Ctx.ClipMin, g_Ctx.ClipMax, {0,0}, {0,0}, {0,0}, {0,0,0,0}, {0,0,0,0}, false, false, !g_Ctx.TextureStack.empty() ? g_Ctx.TextureStack.back() : nullptr });
    }

    inline void ShadowDrawList::AddRectFilled(Vec2 pos, Vec2 size, Color color) {
        GetCmdBuffer().push_back({ ShadowDrawCmdType::RectFilled, pos, size, color, 1.0f, "", nullptr, 1.0f, g_Ctx.ClippingEnabled, g_Ctx.ClipMin, g_Ctx.ClipMax, {0,0}, {0,0}, {0,0}, {0,0,0,0}, {0,0,0,0}, false, false, !g_Ctx.TextureStack.empty() ? g_Ctx.TextureStack.back() : nullptr });
    }

    inline void ShadowDrawList::AddTexture(Vec2 pos, Vec2 size, Color color, SDK::UTexture* texture) {
        SDK::UTexture* tex = texture ? texture : (!g_Ctx.TextureStack.empty() ? g_Ctx.TextureStack.back() : nullptr);
        GetCmdBuffer().push_back({ ShadowDrawCmdType::Texture, pos, size, color, 1.0f, "", nullptr, 1.0f, g_Ctx.ClippingEnabled, g_Ctx.ClipMin, g_Ctx.ClipMax, {0,0}, {0,0}, {0,0}, {0,0,0,0}, {0,0,0,0}, false, false, tex });
    }

    inline void ShadowDrawList::AddTriangle(Vec2 p1, Vec2 p2, Vec2 p3, Color color, float thickness) {
        GetCmdBuffer().push_back({ ShadowDrawCmdType::Triangle, {0,0}, {0,0}, color, thickness, "", nullptr, 1.0f, g_Ctx.ClippingEnabled, g_Ctx.ClipMin, g_Ctx.ClipMax, p1, p2, p3, {0,0,0,0}, {0,0,0,0}, false, false, !g_Ctx.TextureStack.empty() ? g_Ctx.TextureStack.back() : nullptr });
    }

    inline void ShadowDrawList::AddTriangleFilled(Vec2 p1, Vec2 p2, Vec2 p3, Color color) {
        GetCmdBuffer().push_back({ ShadowDrawCmdType::TriangleFilled, {0,0}, {0,0}, color, 1.0f, "", nullptr, 1.0f, g_Ctx.ClippingEnabled, g_Ctx.ClipMin, g_Ctx.ClipMax, p1, p2, p3, {0,0,0,0}, {0,0,0,0}, false, false, !g_Ctx.TextureStack.empty() ? g_Ctx.TextureStack.back() : nullptr });
    }

    inline void ShadowDrawList::AddCircleFilled(Vec2 center, float radius, Color color) {
        if (radius <= 0.0f) return;

        float diameter = radius * 2.0f;
        SDK::UTexture2D* texture = nullptr;

        if (diameter <= 16.0f) {
            texture = LoadTextureFromBuffer(Shadow_Texture::CircleFilled_16, sizeof(Shadow_Texture::CircleFilled_16));
        }
        else if (diameter <= 32.0f) {
            texture = LoadTextureFromBuffer(Shadow_Texture::CircleFilled_32, sizeof(Shadow_Texture::CircleFilled_32));
        }
        else if (diameter <= 64.0f) {
            texture = LoadTextureFromBuffer(Shadow_Texture::CircleFilled_64, sizeof(Shadow_Texture::CircleFilled_64));
        }
        else if (diameter <= 128.0f) {
            texture = LoadTextureFromBuffer(Shadow_Texture::CircleFilled_128, sizeof(Shadow_Texture::CircleFilled_128));
        }
        else if (diameter <= 256.0f) {
            texture = LoadTextureFromBuffer(Shadow_Texture::CircleFilled_256, sizeof(Shadow_Texture::CircleFilled_256));
        }
        else if (diameter <= 512.0f) {
            texture = LoadTextureFromBuffer(Shadow_Texture::CircleFilled_512, sizeof(Shadow_Texture::CircleFilled_512));
        }
        else {
            texture = LoadTextureFromBuffer(Shadow_Texture::CircleFilled_1024, sizeof(Shadow_Texture::CircleFilled_1024));
        }

        if (!texture) return;

        Vec2 pos = { center.x - radius, center.y - radius };
        Vec2 size = { diameter, diameter };

        AddTexture(pos, size, color, texture);
    }

    inline void ShadowDrawList::AddText(Vec2 pos, Color color, std::string_view text) {
        if (!g_Ctx.TextPixelSnapStack.empty() && g_Ctx.TextPixelSnapStack.back()) {
            pos.x = std::round(pos.x);
            pos.y = std::round(pos.y);
        }

        SDK::UFont* font = g_Ctx.DefaultFont;
        float scaleVal = 1.0f;
        bool noSDF = false;
        if (!g_Ctx.FontStack.empty()) {
            font = g_Ctx.FontStack.back().Font;
            scaleVal = g_Ctx.FontStack.back().Scale;
            noSDF = g_Ctx.FontStack.back().NoSDF;
        }
        if (!g_Ctx.Canvas || !font) return;
        Vec2 clippedPos = pos;
        bool shouldDraw = true;
        std::string clippedText = ClipTextString(text, pos, clippedPos, shouldDraw);
        if (!shouldDraw || clippedText.empty()) return;

        bool outline = false;
        Color outlineColor = GetColor(GuiCol_TextOutline);
        if (!g_Ctx.TextOutlineStack.empty()) {
            outline = g_Ctx.TextOutlineStack.back().Outline;
            outlineColor = g_Ctx.TextOutlineStack.back().OutlineColor;
        }

        Vec2 textSize = MeasureTextSize(clippedText);
        GetCmdBuffer().push_back({ ShadowDrawCmdType::Text, clippedPos, textSize, color, 1.0f, clippedText, font, scaleVal, g_Ctx.ClippingEnabled, g_Ctx.ClipMin, g_Ctx.ClipMax, {0,0}, {0,0}, {0,0}, GetColor(GuiCol_TextShadow), outlineColor, outline, noSDF, nullptr });
    }

    inline void ShadowDrawList::AddText(SDK::UFont* font, float fontScale, Color shadowColor, Color outlineColor, Vec2 pos, Color color, std::string_view text, bool outline, bool noSDF) {
        if (!g_Ctx.Canvas || !font) return;

        if (!g_Ctx.TextPixelSnapStack.empty() && g_Ctx.TextPixelSnapStack.back()) {
            pos.x = std::round(pos.x);
            pos.y = std::round(pos.y);
        }

        Vec2 clippedPos = pos;
        bool shouldDraw = true;
        std::string clippedText = ClipTextString(text, pos, clippedPos, shouldDraw);
        if (!shouldDraw || clippedText.empty()) return;

        ScopedFontScale fontGuard(font, fontScale, noSDF);
        SDK::FVector2D scale{ fontGuard.CalculatedScale, fontGuard.CalculatedScale };
        std::wstring wstr = ToWString(clippedText);
        SDK::FVector2D s = g_Ctx.Canvas->K2_TextSize(font, SDK::FString(wstr.c_str()), scale);
        Vec2 textSize = { static_cast<float>(s.X), static_cast<float>(s.Y) };

        GetCmdBuffer().push_back({ ShadowDrawCmdType::Text, clippedPos, textSize, color, 1.0f, clippedText, font, fontScale, g_Ctx.ClippingEnabled, g_Ctx.ClipMin, g_Ctx.ClipMax, {0,0}, {0,0}, {0,0}, shadowColor, outlineColor, outline, noSDF, nullptr });
    }

    inline void ShadowDrawList::ChannelsSplit(int count) {
        if (count <= 1) {
            _ChannelsCount = 1;
            _ChannelsCurrent = 0;
            return;
        }

        _ChannelsCount = count;
        _ChannelsCurrent = 0;
        if (_Channels.size() < static_cast<size_t>(count)) {
            _Channels.resize(count);
        }
        for (int i = 0; i < count; ++i) {
            _Channels[i].clear();
        }
    }

    inline void ShadowDrawList::SetChannel(int channel_idx) {
        if (channel_idx >= 0 && channel_idx < _ChannelsCount) {
            _ChannelsCurrent = channel_idx;
        }
    }

    inline void ShadowDrawList::ChannelsMerge() {
        if (_ChannelsCount <= 1) {
            return;
        }

        for (int i = 0; i < _ChannelsCount; ++i) {
            auto& ch = _Channels[i];
            if (!ch.empty()) {
                CmdBuffer.insert(
                    CmdBuffer.end(),
                    std::make_move_iterator(ch.begin()),
                    std::make_move_iterator(ch.end())
                );
                ch.clear();
            }
        }

        _ChannelsCurrent = 0;
        _ChannelsCount = 1;
    }

    // --- 剪贴板操作 ---
    inline void SetClipboardText(const std::string& text) {
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
            if (hMem) {
                void* p = GlobalLock(hMem);
                if (p) {
                    memcpy(p, text.c_str(), text.size() + 1);
                    GlobalUnlock(hMem);
                    if (!SetClipboardData(CF_TEXT, hMem)) {
                        GlobalFree(hMem);
                    }
                }
                else {
                    GlobalFree(hMem);
                }
            }
            CloseClipboard();
        }
    }

    inline std::string GetClipboardText() {
        std::string result;
        if (OpenClipboard(nullptr)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData) {
                char* pszText = static_cast<char*>(GlobalLock(hData));
                if (pszText) {
                    result = pszText;
                }
                GlobalUnlock(hData);
            }
            CloseClipboard();
        }
        return result;
    }

    inline float GetControlOffsetX() {
        return std::max(g_Ctx.Style.ControlOffsetMin, g_Ctx.WindowSize.x * g_Ctx.Style.ControlOffsetRatio);
    }

    inline float GetRightMargin() {
        return g_Ctx.Style.WindowPadding.x + g_Ctx.CurrentScrollbarWidth + g_Ctx.Style.ScrollbarMargin;
    }

    inline void SetLastItemInfo(Vec2 min, Vec2 max, size_t id, bool disabled = false) {
        g_Ctx.LastItemMin = min;
        g_Ctx.LastItemMax = max;
        g_Ctx.LastItemId = id;
        g_Ctx.LastItemDisabled = disabled;

        Vec2 size = { max.x - min.x, max.y - min.y };
        bool hovered = !disabled && IsRectVisible(min, size) && IsMouseHovering(min, size);

        g_Ctx.LastItemClicked[ShadowMouseButton_Left] = hovered && g_Ctx.MouseClickedThisFrame;
        g_Ctx.LastItemClicked[ShadowMouseButton_Right] = hovered && g_Ctx.RightMouseClickedThisFrame;
        g_Ctx.LastItemClicked[ShadowMouseButton_Middle] = hovered && g_Ctx.MiddleMouseClickedThisFrame;

        if (g_Ctx.InTooltip) {
            g_Ctx.CurrentTooltipSize.x = std::max(g_Ctx.CurrentTooltipSize.x, max.x - g_Ctx.WindowPos.x + g_Ctx.Style.WindowPadding.x);
            g_Ctx.CurrentTooltipSize.y = std::max(g_Ctx.CurrentTooltipSize.y, max.y - g_Ctx.WindowPos.y + g_Ctx.Style.WindowPadding.y);
        }
    }

    inline void ItemSize(Vec2 min, Vec2 max) {
        g_Ctx.LastItemMaxX = max.x;
        SetCursorPosY(GetCursorPosY() + (max.y - min.y) + g_Ctx.Style.ItemSpacing.y);
        SetCursorPosX(g_Ctx.IndentX);
    }

    inline void ItemSize(Vec2 size) {
        ItemSize(g_Ctx.Cursor, { g_Ctx.Cursor.x + size.x, g_Ctx.Cursor.y + size.y });
    }

    inline bool ItemAdd(Vec2 min, Vec2 max, size_t id, bool disabled = false) {
        SetLastItemInfo(min, max, id, disabled);
        return IsRectVisible(min, { max.x - min.x, max.y - min.y });
    }

    inline bool ItemAdd(Vec2 size, size_t id, bool disabled = false) {
        return ItemAdd(g_Ctx.Cursor, { g_Ctx.Cursor.x + size.x, g_Ctx.Cursor.y + size.y }, id, disabled);
    }

    inline bool IsItemActive() {
        if (g_Ctx.LastItemId == 0) return false;
        return g_Ctx.ActiveId == g_Ctx.LastItemId;
    }

    inline bool IsItemActivated() {
        if (g_Ctx.LastItemId == 0) return false;
        return g_Ctx.ActiveId == g_Ctx.LastItemId && g_Ctx.ActiveIdPreviousFrame != g_Ctx.LastItemId;
    }

    inline bool IsItemDeactivated() {
        if (g_Ctx.LastItemId == 0) return false;
        return g_Ctx.ActiveIdPreviousFrame == g_Ctx.LastItemId && g_Ctx.ActiveId != g_Ctx.LastItemId;
    }

    inline bool IsItemClicked(ShadowMouseButton button = 0) {
        if (button < 0 || button >= ShadowMouseButton_COUNT) return false;
        return g_Ctx.LastItemClicked[button];
    }

    inline bool IsItemVisible() {
        if (!g_Ctx.InActiveTab) return false;
        Vec2 size = { g_Ctx.LastItemMax.x - g_Ctx.LastItemMin.x, g_Ctx.LastItemMax.y - g_Ctx.LastItemMin.y };
        if (size.x <= 0.0f && size.y <= 0.0f) return false;
        return IsRectVisible(g_Ctx.LastItemMin, size);
    }

    inline Vec2 GetItemRectMin() {
        return g_Ctx.LastItemMin;
    }

    inline Vec2 GetItemRectMax() {
        return g_Ctx.LastItemMax;
    }

    inline Vec2 GetItemRectSize() {
        return { g_Ctx.LastItemMax.x - g_Ctx.LastItemMin.x, g_Ctx.LastItemMax.y - g_Ctx.LastItemMin.y };
    }

    inline size_t GetItemID() {
        return g_Ctx.LastItemId;
    }

    inline void BeginDisabled(bool disabled = true) {
        bool current = IsDisabled();
        // 如果外层已经被禁用，内层强制继承禁用状态
        g_Ctx.DisabledStack.push_back(current || disabled);
    }

    inline void EndDisabled() {
        if (!g_Ctx.DisabledStack.empty()) {
            g_Ctx.DisabledStack.pop_back();
        }
    }

    inline bool BeginPopup(std::string_view name, ShadowWindowFlags flags = ShadowWindowFlags_None) {
        std::string_view display; size_t id;
        ParseLabel(name, display, id);

        if (!IsPopupOpen(id)) {
            PopupBackupState dummy{};
            dummy.Id = 0;
            dummy.Closed = false;
            g_Ctx.PopupStack.push_back(dummy);
            return false;
        }

        PopupBackupState backup{};
        backup.Id = id;
        backup.WindowPos = g_Ctx.WindowPos;
        backup.WindowSize = g_Ctx.WindowSize;
        backup.Cursor = g_Ctx.Cursor;
        backup.ContentStartY = g_Ctx.ContentStartY;
        backup.ScrollY = g_Ctx.ScrollY;
        backup.LastItemMaxX = g_Ctx.LastItemMaxX;
        backup.IsScrollApplied = g_Ctx.IsScrollApplied;
        backup.CurrentWindowFlags = g_Ctx.CurrentWindowFlags;
        backup.IndentX = g_Ctx.IndentX;
        backup.ClippingEnabled = g_Ctx.ClippingEnabled;
        backup.ClipMin = g_Ctx.ClipMin;
        backup.ClipMax = g_Ctx.ClipMax;
        backup.ClipStack = g_Ctx.ClipStack;
        backup.CurrentWindow = g_Ctx.CurrentWindow;
        backup.IsDragging = g_Ctx.IsDragging;
        backup.DragOffset = g_Ctx.DragOffset;
        backup.Closed = false;
        backup.BgBorderCmdIdx = 0;
        backup.BgFilledCmdIdx = 0;
        backup.PopupOldClipMax = { 0.f, 0.f };
        backup.PopupOldWidth = 0.f;
        backup.RightAlignCmds.clear();

        g_Ctx.PopupStack.push_back(backup);

        g_Ctx.InPopup = true;

        auto& win = g_Ctx.Windows[id];
        g_Ctx.CurrentWindow = &win;

        if (win.Id == 0) {
            win.Id = id;
            win.Name = std::string(display);
            win.Pos = g_Ctx.WindowPos;
            win.Size = { g_Ctx.Style.PopupInitialSize, g_Ctx.Style.PopupInitialSize };
        }

        win.LastAccessedFrame = g_Ctx.FrameCount;

        if (g_Ctx.HasNextWindowPos) {
            win.Pos = g_Ctx.NextWindowPos;
            g_Ctx.HasNextWindowPos = false;
        }
        g_Ctx.WindowPos = win.Pos;

        if (g_Ctx.HasNextWindowSize) {
            win.Size = g_Ctx.NextWindowSize;
            if (win.Size.x <= 0.f) {
                win.Size.x = g_Ctx.Style.PopupInitialSize; // 初始安全宽度，防止裁剪区域过小导致后续控件无法显示
            }
            if (win.Size.y <= 0.f) {
                win.Size.y = g_Ctx.ItemHeight;
            }
            g_Ctx.HasNextWindowSize = false;
        }
        g_Ctx.WindowSize = win.Size;
        g_Ctx.PopupStack.back().PopupOldWidth = win.Size.x;

        g_Ctx.IsDragging = win.IsDragging;
        g_Ctx.DragOffset = win.DragOffset;
        g_Ctx.CurrentWindowFlags = flags | ShadowWindowFlags_NoTitleBar | ShadowWindowFlags_NoResize | ShadowWindowFlags_NoScrollbar;

        bool noMove = (g_Ctx.CurrentWindowFlags & ShadowWindowFlags_NoMove) != 0;
        bool hoveringWholeWindow = IsMouseHoveringRaw(g_Ctx.WindowPos, g_Ctx.WindowSize);

        bool isOtherDragging = (g_Ctx.DraggingSliderId != 0) || g_Ctx.IsDraggingSV || g_Ctx.IsDraggingHue || g_Ctx.IsDraggingAlpha || g_Ctx.DraggingTabId != 0 || g_Ctx.DraggingTabBarScrollId != 0 || g_Ctx.DraggingListBoxScrollId != 0;
        for (const auto& pair : g_Ctx.Windows) {
            if (pair.first != id) {
                if (pair.second.IsDragging || pair.second.IsResizing || pair.second.IsDraggingScrollbar) {
                    isOtherDragging = true;
                    break;
                }
            }
        }

        bool isTopmostPopup = !g_Ctx.ActivePopups.empty() && g_Ctx.ActivePopups.back() == id;

        if (!noMove && !g_Ctx.IsDragging && hoveringWholeWindow && g_Ctx.MouseClicked && !isOtherDragging && isTopmostPopup) {
            g_Ctx.IsDragging = true;
            g_Ctx.DragOffset.x = g_Ctx.MousePos.x - g_Ctx.WindowPos.x;
            g_Ctx.DragOffset.y = g_Ctx.MousePos.y - g_Ctx.WindowPos.y;
        }
        if (g_Ctx.IsDragging && isOtherDragging) g_Ctx.IsDragging = false;

        if (!noMove && g_Ctx.IsDragging) {
            g_Ctx.WindowPos.x = g_Ctx.MousePos.x - g_Ctx.DragOffset.x;
            g_Ctx.WindowPos.y = g_Ctx.MousePos.y - g_Ctx.DragOffset.y;
            win.Pos = g_Ctx.WindowPos;
            if (!g_Ctx.MouseDown) g_Ctx.IsDragging = false;
        }

        g_Ctx.LastItemMaxX = 0.f;
        g_Ctx.LastItemMin = { 0.f, 0.f };
        g_Ctx.LastItemMax = { 0.f, 0.f };
        g_Ctx.LastItemId = 0;
        g_Ctx.LastItemDisabled = false;

        g_Ctx.ClippingEnabled = false;
        g_Ctx.ClipStack.clear();

        ShadowDrawList* drawList = GetWindowDrawList();
        g_Ctx.PopupStack.back().BgBorderCmdIdx = drawList->GetCmdBuffer().size();
        drawList->AddRect(g_Ctx.WindowPos, g_Ctx.WindowSize, GetColor(GuiCol_PopupBorder));
        g_Ctx.PopupStack.back().BgFilledCmdIdx = drawList->GetCmdBuffer().size();
        drawList->AddRectFilled({ g_Ctx.WindowPos.x + g_Ctx.Style.PopupBorderInset, g_Ctx.WindowPos.y + g_Ctx.Style.PopupBorderInset }, { g_Ctx.WindowSize.x - g_Ctx.Style.PopupFillInset, g_Ctx.WindowSize.y - g_Ctx.Style.PopupFillInset }, GetColor(GuiCol_PopupBg));

        g_Ctx.IndentX = 0.f;
        g_Ctx.Cursor = { g_Ctx.WindowPos.x + g_Ctx.Style.WindowPadding.x, g_Ctx.WindowPos.y + g_Ctx.Style.WindowPadding.y };
        g_Ctx.ContentStartY = g_Ctx.Cursor.y;
        g_Ctx.ScrollY = 0.f;
        g_Ctx.IsScrollApplied = false;

        g_Ctx.PopupStack.back().PopupOldClipMax = { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x, g_Ctx.WindowPos.y + g_Ctx.WindowSize.y };
        PushClipRect(g_Ctx.WindowPos, g_Ctx.PopupStack.back().PopupOldClipMax);

        return true;
    }

    inline void EndPopup() {
        if (g_Ctx.PopupStack.empty()) return;

        auto backup = g_Ctx.PopupStack.back();
        g_Ctx.PopupStack.pop_back();

        // 跳过无效的 dummy 状态
        if (backup.Id == 0) {
            return;
        }

        PopClipRect();

        // 如果弹窗已被提前关闭，不更新窗口尺寸（其他状态仍需要恢复）
        if (!backup.Closed) {
            auto& win = g_Ctx.Windows[backup.Id];
            win.Size.y = g_Ctx.Cursor.y - g_Ctx.WindowPos.y + g_Ctx.Style.WindowPadding.y;
            // 完全跟随内容宽度，仅保留最小宽度限制
            win.Size.x = std::max(g_Ctx.Style.PopupMinWidth, g_Ctx.LastItemMaxX - g_Ctx.WindowPos.x + g_Ctx.Style.WindowPadding.x);

            // 追溯修复首帧弹窗闪烁问题 (更新已发射的背景命令与子裁剪区域)
            auto& cmds = win.DrawList.GetCmdBuffer();
            if (backup.BgBorderCmdIdx < cmds.size()) {
                cmds[backup.BgBorderCmdIdx].size = win.Size;
            }
            if (backup.BgFilledCmdIdx < cmds.size()) {
                cmds[backup.BgFilledCmdIdx].size = { win.Size.x - g_Ctx.Style.PopupFillInset, win.Size.y - g_Ctx.Style.PopupFillInset };
            }

            Vec2 trueClipMax = { g_Ctx.WindowPos.x + win.Size.x, g_Ctx.WindowPos.y + win.Size.y };
            for (size_t i = backup.BgBorderCmdIdx; i < cmds.size(); ++i) {
                if (cmds[i].clippingEnabled &&
                    std::abs(cmds[i].clipMax.x - backup.PopupOldClipMax.x) < 0.1f &&
                    std::abs(cmds[i].clipMax.y - backup.PopupOldClipMax.y) < 0.1f) {
                    cmds[i].clipMax = trueClipMax;
                }
            }

            float dx = win.Size.x - backup.PopupOldWidth;
            if (dx != 0.f) {
                for (const auto& raCmd : backup.RightAlignCmds) {
                    if (raCmd.CmdIndex < cmds.size()) {
                        if (raCmd.CmdType == RightAlignCmdType::RectBackground) {
                            cmds[raCmd.CmdIndex].size.x += dx;
                        }
                        else if (raCmd.CmdType == RightAlignCmdType::TriangleArrow) {
                            cmds[raCmd.CmdIndex].p1.x += dx;
                            cmds[raCmd.CmdIndex].p2.x += dx;
                            cmds[raCmd.CmdIndex].p3.x += dx;
                        }
                        else if (raCmd.CmdType == RightAlignCmdType::TextShortcut) {
                            cmds[raCmd.CmdIndex].pos.x += dx;
                        }
                    }
                }
            }

            win.IsDragging = g_Ctx.IsDragging;
            win.DragOffset = g_Ctx.DragOffset;
        }

        // 恢复父级状态（无论如何都需要恢复）
        g_Ctx.WindowPos = backup.WindowPos;
        g_Ctx.WindowSize = backup.WindowSize;
        g_Ctx.Cursor = backup.Cursor;
        g_Ctx.ContentStartY = backup.ContentStartY;
        g_Ctx.ScrollY = backup.ScrollY;
        g_Ctx.LastItemMaxX = backup.LastItemMaxX;
        g_Ctx.IsScrollApplied = backup.IsScrollApplied;
        g_Ctx.CurrentWindowFlags = backup.CurrentWindowFlags;
        g_Ctx.IndentX = backup.IndentX;

        g_Ctx.ClippingEnabled = backup.ClippingEnabled;
        g_Ctx.ClipMin = backup.ClipMin;
        g_Ctx.ClipMax = backup.ClipMax;
        g_Ctx.ClipStack = backup.ClipStack;
        g_Ctx.CurrentWindow = backup.CurrentWindow;
        g_Ctx.IsDragging = backup.IsDragging;
        g_Ctx.DragOffset = backup.DragOffset;

        if (g_Ctx.PopupStack.empty()) {
            g_Ctx.InPopup = false;
        }
    }

    inline bool TreeNode(std::string_view name, ShadowTreeNodeFlags flags = ShadowTreeNodeFlags_None, Vec2 size_arg = { 0.f, 0.f }) {
        // 无条件压栈：与其他 Begin 函数一致，用户必须在 if 作用域外调用 TreePop()
        g_Ctx.TreeNodeStack++;

        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;

        bool noIndent = (flags & ShadowTreeNodeFlags_NoIndent) != 0;
        g_Ctx.TreeNodeNoIndentStack.push_back(noIndent);

        if (!g_Ctx.InActiveTab) {
            if (!noIndent) {
                g_Ctx.IndentX += g_Ctx.Style.TreeNodeIndent;
            }
            SetCursorPosX(g_Ctx.IndentX);
            return false;
        }

        if (g_Ctx.TreeNodeOpenStates.find(id) == g_Ctx.TreeNodeOpenStates.end()) {
            g_Ctx.TreeNodeOpenStates[id] = (flags & ShadowTreeNodeFlags_DefaultOpen) != 0;
        }

        bool isOpen = g_Ctx.TreeNodeOpenStates[id];
        bool isFramed = (flags & ShadowTreeNodeFlags_Framed) != 0;
        bool isFitText = (flags & ShadowTreeNodeFlags_FitText) != 0;

        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;
        float arrowSize = itemHeight * g_Ctx.Style.TreeNodeArrowSizeRatio;
        float textWidth = MeasureTextSize(display).x;
        Vec2 interactSize = { arrowSize + g_Ctx.Style.LabelSpacing + textWidth, itemHeight };

        if (isFramed) {
            if (isFitText) {
                interactSize.x = g_Ctx.Style.FramePadding.x + arrowSize + g_Ctx.Style.TreeNodeTextSpacing + textWidth + g_Ctx.Style.FramePadding.x;
            }
            else {
                float rightMargin = GetRightMargin();
                interactSize.x = std::max(interactSize.x, g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - rightMargin - g_Ctx.Cursor.x);
            }
        }

        if (size_arg.x > 0.f) interactSize.x = size_arg.x;

        bool disabled = IsDisabled();
        Vec2 pos = g_Ctx.Cursor;
        Vec2 min = pos;
        Vec2 max = { pos.x + interactSize.x, pos.y + itemHeight };

        ItemSize(min, max);
        if (!noIndent) {
            g_Ctx.IndentX += g_Ctx.Style.TreeNodeIndent;
            SetCursorPosX(g_Ctx.IndentX);
        }

        if (!ItemAdd(min, max, id, disabled)) {
            return isOpen;
        }

        bool hovered = !disabled && IsMouseHovering(pos, interactSize);
        if (hovered && g_Ctx.MouseDown) {
            g_Ctx.ActiveId = id;
        }

        if (hovered && g_Ctx.MouseClicked) {
            isOpen = !isOpen;
            g_Ctx.TreeNodeOpenStates[id] = isOpen;
            g_Ctx.MouseClicked = false;
        }

        Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
        Color arrowColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
        if (hovered && !disabled) {
            textColor = GetColor(GuiCol_TextHighlight);
            arrowColor = GetColor(GuiCol_TextHighlight);
        }

        if (isFramed) {
            Color bgColor = disabled ? GetColor(GuiCol_ControlDisabled) : (hovered ? GetColor(GuiCol_FrameBgHovered) : GetColor(GuiCol_FrameBg));
            GetWindowDrawList()->AddRectFilled(pos, interactSize, bgColor);
        }

        float centerY = pos.y + itemHeight * 0.5f;
        float arrowX = pos.x + (isFramed ? g_Ctx.Style.FramePadding.x : 0.f);

        if (isOpen) {
            Vec2 p1 = { arrowX, centerY - arrowSize * 0.25f };
            Vec2 p2 = { arrowX + arrowSize, centerY - arrowSize * 0.25f };
            Vec2 p3 = { arrowX + arrowSize * 0.5f, centerY + arrowSize * 0.25f };
            GetWindowDrawList()->AddTriangleFilled(p1, p2, p3, arrowColor);
        }
        else {
            Vec2 p1 = { arrowX + arrowSize * 0.25f, centerY - arrowSize * 0.5f };
            Vec2 p2 = { arrowX + arrowSize * 0.75f, centerY };
            Vec2 p3 = { arrowX + arrowSize * 0.25f, centerY + arrowSize * 0.5f };
            GetWindowDrawList()->AddTriangleFilled(p1, p2, p3, arrowColor);
        }

        float textStartX = std::round(arrowX + arrowSize + g_Ctx.Style.TreeNodeTextSpacing);
        GetWindowDrawList()->AddText({ textStartX, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, display);

        return isOpen;
    }

    inline void TreePop() {
        // 无条件弹出：与 TreeNode 头部无条件压栈对应，用户必须在 if 作用域外调用
        g_Ctx.TreeNodeStack--;

        bool noIndent = false;
        if (!g_Ctx.TreeNodeNoIndentStack.empty()) {
            noIndent = g_Ctx.TreeNodeNoIndentStack.back();
            g_Ctx.TreeNodeNoIndentStack.pop_back();
        }

        if (!noIndent) {
            g_Ctx.IndentX = std::max(0.f, g_Ctx.IndentX - g_Ctx.Style.TreeNodeIndent);
        }

        SetCursorPosX(g_Ctx.IndentX);
    }

    inline void Indent(float indent_w = 0.0f) {
        if (!g_Ctx.InActiveTab) return;
        float actual_indent = (indent_w > 0.0f) ? indent_w : g_Ctx.Style.IndentSpacing;
        g_Ctx.IndentX += actual_indent;
        SetCursorPosX(g_Ctx.IndentX);
    }

    inline void Unindent(float indent_w = 0.0f) {
        if (!g_Ctx.InActiveTab) return;
        float actual_indent = (indent_w > 0.0f) ? indent_w : g_Ctx.Style.IndentSpacing;
        g_Ctx.IndentX = std::max(0.f, g_Ctx.IndentX - actual_indent);
        SetCursorPosX(g_Ctx.IndentX);
    }

    inline void Spacing(Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return;
        SetCursorPosY(GetCursorPosY() + (size_arg.y > 0.f ? size_arg.y : g_Ctx.Style.ItemSpacing.y));
        if (size_arg.x > 0.f) SetCursorPosX(GetCursorPosX() + size_arg.x);
    }

    inline void SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f) {
        if (!g_Ctx.InActiveTab) return;
        // 撤销上一个控件产生的Y轴换行递增
        SetCursorPosY(GetCursorPosY() - (g_Ctx.ItemHeight + g_Ctx.Style.ItemSpacing.y));

        if (offset_from_start_x > 0.0f) {
            SetCursorPosX(offset_from_start_x + g_Ctx.IndentX);
        }
        else {
            float sp = (spacing < 0.0f) ? g_Ctx.Style.ItemSpacing.x : spacing;
            SetCursorScreenPos({ g_Ctx.LastItemMaxX + sp, g_Ctx.Cursor.y });
        }
    }

    inline void NewLine() {
        if (!g_Ctx.InActiveTab) return;
        SetCursorPosY(GetCursorPosY() + g_Ctx.ItemHeight + g_Ctx.Style.ItemSpacing.y);
        SetCursorPosX(g_Ctx.IndentX);
        g_Ctx.LastItemMaxX = g_Ctx.Cursor.x;
    }

    inline bool IsItemHovered(ShadowHoveredFlags flags = ShadowHoveredFlags_None) {
        if (!g_Ctx.InActiveTab) return false;

        Vec2 min = g_Ctx.LastItemMin;
        Vec2 max = g_Ctx.LastItemMax;
        size_t id = g_Ctx.LastItemId;

        bool disabled = g_Ctx.LastItemDisabled;
        if (disabled && !(flags & ShadowHoveredFlags_AllowWhenDisabled)) {
            return false;
        }

        if (!IsRectVisible(min, { max.x - min.x, max.y - min.y })) {
            return false;
        }

        bool hasActiveItem = g_Ctx.ActiveInputId != 0 || g_Ctx.DraggingSliderId != 0 ||
            !g_Ctx.ActivePopups.empty() ||
            g_Ctx.IsDragging || g_Ctx.IsResizing || g_Ctx.IsDraggingScrollbar ||
            g_Ctx.DraggingTabId != 0 || g_Ctx.DraggingTabBarScrollId != 0 ||
            g_Ctx.DraggingListBoxScrollId != 0;

        if (hasActiveItem && !(flags & ShadowHoveredFlags_AllowWhenBlockedByActiveItem)) {
            bool isSelfActive = false;
            if (id != 0) {
                if (g_Ctx.DraggingSliderId == id) {
                    isSelfActive = true;
                }
                g_Ctx.IDStack.push_back(id);
                size_t sliderInputId = GetID("##SliderInput");
                g_Ctx.IDStack.pop_back();
                if (g_Ctx.ActiveInputId == sliderInputId) {
                    isSelfActive = true;
                }
            }
            if (!isSelfActive) {
                return false;
            }
        }

        bool hoveringRaw = IsMouseHoveringRaw(min, { max.x - min.x, max.y - min.y });
        bool hoveringClipped = IsMouseHovering(min, { max.x - min.x, max.y - min.y });

        if (!hoveringRaw) return false;
        if (!hoveringClipped && !(flags & ShadowHoveredFlags_AllowWhenBlockedByPopup)) {
            return false;
        }

        size_t currentId = (id != 0) ? id : (static_cast<size_t>(g_Ctx.WidgetCount) + 1000000ULL);
        g_Ctx.HoveredIdCurrentFrame = currentId;

        uint64_t currentTime = static_cast<uint64_t>(g_Ctx.RealTimeSeconds * 1000.0);

        if (g_Ctx.LastHoveredIdEval != currentId) {
            if (g_Ctx.HoveredIdPreviousFrame != currentId) {
                g_Ctx.HoveredIdTimerStart = currentTime;
                g_Ctx.HoveredIdStationaryTriggered = false;
                g_Ctx.HoveredIdDelayTriggered = false;
            }
            g_Ctx.LastHoveredIdEval = currentId;
        }

        if (g_Ctx.MouseIsStationary) {
            g_Ctx.HoveredIdStationaryTriggered = true;
        }

        if (flags & ShadowHoveredFlags_Stationary) {
            if (!g_Ctx.HoveredIdStationaryTriggered) {
                return false;
            }
        }

        uint32_t requiredDelay = 0;
        if (flags & ShadowHoveredFlags_DelayNormal) requiredDelay = static_cast<uint32_t>(g_Ctx.Style.HoverDelayNormalMS);
        else if (flags & ShadowHoveredFlags_DelayShort) requiredDelay = static_cast<uint32_t>(g_Ctx.Style.HoverDelayShortMS);

        if (requiredDelay > 0) {
            bool useSharedDelay = !(flags & ShadowHoveredFlags_NoSharedDelay);
            if (useSharedDelay && g_Ctx.SharedDelayActive && currentTime <= g_Ctx.SharedDelayExpirationTime) {
            }
            else {
                uint64_t waitTime = currentTime - std::max(g_Ctx.HoveredIdTimerStart, g_Ctx.MouseStationaryStartTime);
                if (!g_Ctx.HoveredIdDelayTriggered) {
                    if (waitTime >= requiredDelay) {
                        g_Ctx.HoveredIdDelayTriggered = true;
                    }
                    else {
                        return false;
                    }
                }
            }

            if (useSharedDelay && (g_Ctx.HoveredIdDelayTriggered || requiredDelay == 0)) {
                g_Ctx.SharedDelayActive = true;
                g_Ctx.SharedDelayExpirationTime = currentTime + static_cast<uint64_t>(g_Ctx.Style.HoverSharedDelayMS);
            }
        }

        return true;
    }

    inline bool BeginListBox(std::string_view name, Vec2 size) {
        g_Ctx.ListBoxStack++;
        if (!g_Ctx.InActiveTab) return false;
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;

        if (!display.empty()) {
            GetWindowDrawList()->AddText({ g_Ctx.Cursor.x, g_Ctx.Cursor.y + g_Ctx.Style.FramePadding.y }, GetColor(GuiCol_Text), display);
            SetCursorPosY(GetCursorPosY() + g_Ctx.ItemHeight + g_Ctx.Style.ItemSpacing.y);
        }

        float boxWidth = size.x;
        if (boxWidth <= 0.f) {
            boxWidth = std::max(10.f, g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - GetRightMargin() - g_Ctx.Cursor.x);
        }
        float boxHeight = size.y;
        if (boxHeight <= 0.f) {
            boxHeight = g_Ctx.ItemHeight * g_Ctx.Style.ListBoxDefaultHeightItems;
        }

        Vec2 boxPos = g_Ctx.Cursor;
        ListBoxState backup;
        backup.ParentWindowPos = g_Ctx.WindowPos;
        backup.ParentWindowSize = g_Ctx.WindowSize;
        backup.ParentCursor = g_Ctx.Cursor;
        backup.ParentContentStartY = g_Ctx.ContentStartY;
        backup.ParentScrollY = g_Ctx.ScrollY;
        backup.ParentContentHeight = g_Ctx.ContentHeight;
        backup.ParentIsScrollApplied = g_Ctx.IsScrollApplied;
        backup.ParentWindowFlags = g_Ctx.CurrentWindowFlags;
        backup.ParentCurrentScrollbarWidth = g_Ctx.CurrentScrollbarWidth;
        backup.ParentIndentX = g_Ctx.IndentX;
        backup.Id = id;
        backup.Pos = boxPos;
        backup.Size = { boxWidth, boxHeight };

        g_Ctx.ListBoxStateStack.push_back(backup);
        g_Ctx.IndentX = 0.f;

        GetWindowDrawList()->AddRectFilled(boxPos, { boxWidth, boxHeight }, GetColor(GuiCol_FrameBg));
        GetWindowDrawList()->AddRect(boxPos, { boxWidth, boxHeight }, GetColor(GuiCol_Border));

        g_Ctx.WindowPos = boxPos;
        g_Ctx.WindowSize = { boxWidth, boxHeight };
        g_Ctx.CurrentWindowFlags = ShadowWindowFlags_NoResize | ShadowWindowFlags_NoMove | ShadowWindowFlags_NoTitleBar;
        PushStyleVar(GuiStyleVar_WindowPadding, { g_Ctx.Style.FramePadding.x, g_Ctx.Style.FramePadding.y });

        SetCursorScreenPos({ boxPos.x + g_Ctx.Style.WindowPadding.x + g_Ctx.IndentX, boxPos.y + g_Ctx.Style.WindowPadding.y });
        g_Ctx.ContentStartY = g_Ctx.Cursor.y;

        float maxScroll = GetScrollMaxY();
        float& scrollY = g_Ctx.ListBoxScrollY[id];
        scrollY = std::clamp(scrollY, 0.f, maxScroll);
        g_Ctx.ScrollY = scrollY;
        g_Ctx.IsScrollApplied = true;

        if (maxScroll > 0.f) {
            g_Ctx.CurrentScrollbarWidth = g_Ctx.Style.ScrollbarSize;
        }
        else {
            g_Ctx.CurrentScrollbarWidth = 0.f;
        }

        bool hoveringListBox = IsMouseHovering(boxPos, { boxWidth, boxHeight });
        if (hoveringListBox) {
            g_Ctx.HoveredListBoxIdCurrentFrame = id;
            if (g_Ctx.MouseWheel != 0.f) {
                scrollY -= g_Ctx.MouseWheel * g_Ctx.Style.ScrollSpeed;
                scrollY = std::clamp(scrollY, 0.f, maxScroll);
                g_Ctx.ScrollY = scrollY;
                g_Ctx.MouseWheel = 0.f;
            }
        }

        SetCursorPosY(GetCursorPosY() - g_Ctx.ScrollY);
        PushClipRect(boxPos, { boxPos.x + boxWidth, boxPos.y + boxHeight });

        return true;
    }

    inline void EndListBox() {
        g_Ctx.ListBoxStack--;
        if (!g_Ctx.InActiveTab) return;
        if (g_Ctx.ListBoxStateStack.empty()) return;

        ListBoxState backup = g_Ctx.ListBoxStateStack.back();
        // 暂时不 pop，先更新内容高度，让 GetScrollMaxY() 能读取到正确的 ListBox 上下文

        float actualCursorY = g_Ctx.Cursor.y + (g_Ctx.IsScrollApplied ? g_Ctx.ScrollY : 0.f);
        g_Ctx.ListBoxContentHeight[backup.Id] = actualCursorY - g_Ctx.ContentStartY + g_Ctx.Style.WindowPadding.y;

        float maxScroll = GetScrollMaxY();   // 此时栈顶仍是 backup，返回该 ListBox 的 maxScroll
        float viewHeight = backup.Size.y;
        float contentHeight = g_Ctx.ListBoxContentHeight[backup.Id];

        g_Ctx.ListBoxStateStack.pop_back();

        PopClipRect();
        float& scrollY = g_Ctx.ListBoxScrollY[backup.Id];

        g_Ctx.CurrentScrollbarWidth = 0.f;

        if (contentHeight > viewHeight) {
            float scrollbarWidth = g_Ctx.Style.ScrollbarSize;
            float scrollbarMarginRight = g_Ctx.Style.ScrollbarMargin;

            Vec2 trackPos = { backup.Pos.x + backup.Size.x - scrollbarWidth - scrollbarMarginRight, backup.Pos.y };
            Vec2 trackSize = { scrollbarWidth, viewHeight };

            GetWindowDrawList()->AddRectFilled(trackPos, trackSize, GetColor(GuiCol_FrameBg));

            float thumbHeight = std::max(g_Ctx.Style.ScrollbarThumbMinSize, (viewHeight / contentHeight) * trackSize.y);
            float thumbY = trackPos.y + (maxScroll > 0.f ? (scrollY / maxScroll) * (trackSize.y - thumbHeight) : 0.f);
            Vec2 thumbPos = { trackPos.x, thumbY };
            Vec2 thumbSize = { scrollbarWidth, thumbHeight };

            bool hoveringThumb = IsMouseHovering(thumbPos, thumbSize);
            bool hoveringTrack = IsMouseHovering(trackPos, trackSize);

            if (g_Ctx.MouseClicked && hoveringThumb) {
                g_Ctx.DraggingListBoxScrollId = backup.Id;
                g_Ctx.ListBoxScrollDragOffset = g_Ctx.MousePos.y - thumbPos.y;
                g_Ctx.MouseClicked = false;
            }
            else if (g_Ctx.MouseClicked && hoveringTrack) {
                if (g_Ctx.MousePos.y < thumbPos.y) scrollY -= viewHeight;
                else scrollY += viewHeight;
                scrollY = std::clamp(scrollY, 0.f, maxScroll);
                g_Ctx.MouseClicked = false;
            }

            if (g_Ctx.DraggingListBoxScrollId == backup.Id) {
                if (g_Ctx.MouseDown) {
                    float newThumbY = g_Ctx.MousePos.y - g_Ctx.ListBoxScrollDragOffset;
                    float ratio = (newThumbY - trackPos.y) / std::max(1.f, trackSize.y - thumbHeight);
                    scrollY = std::clamp(ratio * maxScroll, 0.f, maxScroll);
                }
                else {
                    g_Ctx.DraggingListBoxScrollId = 0;
                }
            }

            Color thumbColor = (g_Ctx.DraggingListBoxScrollId == backup.Id)
                ? GetColor(GuiCol_SliderGrab)
                : (hoveringThumb ? GetColor(GuiCol_FrameBgHovered) : GetColor(GuiCol_Border));
            GetWindowDrawList()->AddRectFilled(thumbPos, thumbSize, thumbColor);
        }
        else {
            if (g_Ctx.DraggingListBoxScrollId == backup.Id) {
                g_Ctx.DraggingListBoxScrollId = 0;
            }
        }

        g_Ctx.WindowPos = backup.ParentWindowPos;
        g_Ctx.WindowSize = backup.ParentWindowSize;
        SetCursorScreenPos(backup.ParentCursor);
        g_Ctx.ContentStartY = backup.ParentContentStartY;
        g_Ctx.ScrollY = backup.ParentScrollY;
        g_Ctx.ContentHeight = backup.ParentContentHeight;
        g_Ctx.IsScrollApplied = backup.ParentIsScrollApplied;
        g_Ctx.CurrentWindowFlags = backup.ParentWindowFlags;
        g_Ctx.CurrentScrollbarWidth = backup.ParentCurrentScrollbarWidth;
        PopStyleVar();
        g_Ctx.IndentX = backup.ParentIndentX;

        SetLastItemInfo(backup.Pos, { backup.Pos.x + backup.Size.x, backup.Pos.y + backup.Size.y }, backup.Id, false);
        g_Ctx.LastItemMaxX = backup.Pos.x + backup.Size.x;

        SetCursorPosY(GetCursorPosY() + backup.Size.y + g_Ctx.Style.ItemSpacing.y);
        SetCursorPosX(g_Ctx.IndentX);
    }

    inline bool InputTextEx(size_t id, Vec2 pos, Vec2 size, std::string& text, ShadowInputTextFlags flags = 0, bool is_popup = false, std::string_view hint = "") {
        bool disabled = IsDisabled();
        bool hovered = is_popup ? IsMouseHoveringRaw(pos, size) : IsMouseHovering(pos, size);

        bool isReadOnly = (flags & ShadowInputTextFlags_ReadOnly) != 0;
        bool isPassword = (flags & ShadowInputTextFlags_Password) != 0;

        if (g_Ctx.MouseClicked) {
            if (hovered && !disabled) {
                g_Ctx.ActiveInputId = id;
                g_Ctx.ActiveId = id;
                g_Ctx.InputCursorPos = static_cast<int>(text.size());
                g_Ctx.InputSelectionStart = -1;
                g_Ctx.InputSelectionEnd = -1;
                g_Ctx.MouseClicked = false;

                if (flags & ShadowInputTextFlags_AutoSelectAll) {
                    g_Ctx.InputSelectionStart = 0;
                    g_Ctx.InputSelectionEnd = static_cast<int>(text.size());
                    g_Ctx.InputCursorPos = static_cast<int>(text.size());
                }
            }
            else if (g_Ctx.ActiveInputId == id) {
                g_Ctx.ActiveInputId = 0;
            }
        }

        bool isActive = (g_Ctx.ActiveInputId == id) && !disabled;
        if (isActive) {
            g_Ctx.ActiveId = id;
        }
        bool valueChanged = false;

        if (isActive) {
            g_Ctx.InputCursorPos = std::clamp(g_Ctx.InputCursorPos, 0, static_cast<int>(text.size()));
            bool ctrlDown = g_Ctx.KeyStates[VK_CONTROL];
            bool shiftDown = g_Ctx.KeyStates[VK_SHIFT];

            auto DeleteSelection = [&]() {
                if (isReadOnly) return false;
                if (g_Ctx.InputSelectionStart != -1 && g_Ctx.InputSelectionEnd != -1 && g_Ctx.InputSelectionStart != g_Ctx.InputSelectionEnd) {
                    int s = std::clamp(std::min(g_Ctx.InputSelectionStart, g_Ctx.InputSelectionEnd), 0, (int)text.size());
                    int e = std::clamp(std::max(g_Ctx.InputSelectionStart, g_Ctx.InputSelectionEnd), 0, (int)text.size());
                    text.erase(s, e - s);
                    g_Ctx.InputCursorPos = s;
                    g_Ctx.InputSelectionStart = g_Ctx.InputSelectionEnd = -1;
                    valueChanged = true;
                    return true;
                }
                return false;
                };

            if (!isReadOnly) {
                for (char c : g_Ctx.InputChars) {
                    if (flags & ShadowInputTextFlags_CharsNoBlank) if (c == ' ' || c == '\t') continue;
                    if (flags & ShadowInputTextFlags_CharsUppercase) if (c >= 'a' && c <= 'z') c -= 32;
                    if (flags & ShadowInputTextFlags_CharsDecimal) if (!(c >= '0' && c <= '9') && c != '.' && c != '+' && c != '-' && c != '*' && c != '/') continue;
                    if (flags & ShadowInputTextFlags_CharsHexadecimal) if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f') && !(c >= 'A' && c <= 'F')) continue;
                    if (flags & ShadowInputTextFlags_CharsScientific) if (!(c >= '0' && c <= '9') && c != '.' && c != '+' && c != '-' && c != '*' && c != '/' && c != 'e' && c != 'E') continue;

                    DeleteSelection();
                    text.insert(text.begin() + g_Ctx.InputCursorPos, c);
                    g_Ctx.InputCursorPos++;
                    valueChanged = true;
                }
            }

            if (ctrlDown) {
                if (g_Ctx.KeyPressed['A']) {
                    g_Ctx.InputSelectionStart = 0;
                    g_Ctx.InputSelectionEnd = static_cast<int>(text.size());
                    g_Ctx.InputCursorPos = static_cast<int>(text.size());
                }
                if (g_Ctx.KeyPressed['C'] || g_Ctx.KeyPressed['X']) {
                    if (!isPassword) {
                        if (g_Ctx.InputSelectionStart != -1 && g_Ctx.InputSelectionEnd != -1 && g_Ctx.InputSelectionStart != g_Ctx.InputSelectionEnd) {
                            int s = std::clamp(std::min(g_Ctx.InputSelectionStart, g_Ctx.InputSelectionEnd), 0, (int)text.size());
                            int e = std::clamp(std::max(g_Ctx.InputSelectionStart, g_Ctx.InputSelectionEnd), 0, (int)text.size());
                            SetClipboardText(text.substr(s, e - s));
                            if (g_Ctx.KeyPressed['X'] && !isReadOnly) DeleteSelection();
                        }
                        else if (g_Ctx.KeyPressed['C']) {
                            SetClipboardText(text);
                        }
                    }
                }
                if (g_Ctx.KeyPressed['V'] && !isReadOnly) {
                    std::string clip = GetClipboardText();
                    if (!clip.empty()) {
                        std::string filtered;
                        for (char c : clip) {
                            if (c < 32 || c >= 127) continue;
                            if (flags & ShadowInputTextFlags_CharsNoBlank) if (c == ' ' || c == '\t') continue;
                            if (flags & ShadowInputTextFlags_CharsUppercase) if (c >= 'a' && c <= 'z') c -= 32;
                            if (flags & ShadowInputTextFlags_CharsDecimal) if (!(c >= '0' && c <= '9') && c != '.' && c != '+' && c != '-' && c != '*' && c != '/') continue;
                            if (flags & ShadowInputTextFlags_CharsHexadecimal) if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f') && !(c >= 'A' && c <= 'F')) continue;
                            if (flags & ShadowInputTextFlags_CharsScientific) if (!(c >= '0' && c <= '9') && c != '.' && c != '+' && c != '-' && c != '*' && c != '/' && c != 'e' && c != 'E') continue;
                            filtered += c;
                        }
                        if (!filtered.empty()) {
                            DeleteSelection();
                            text.insert(g_Ctx.InputCursorPos, filtered);
                            g_Ctx.InputCursorPos += static_cast<int>(filtered.size());
                            valueChanged = true;
                        }
                    }
                }
            }
            else {
                if (g_Ctx.KeyPressed[VK_LEFT]) {
                    if (shiftDown) { if (g_Ctx.InputSelectionStart == -1) g_Ctx.InputSelectionStart = g_Ctx.InputCursorPos; }
                    else { g_Ctx.InputSelectionStart = g_Ctx.InputSelectionEnd = -1; }
                    if (g_Ctx.InputCursorPos > 0) g_Ctx.InputCursorPos--;
                    if (shiftDown) g_Ctx.InputSelectionEnd = g_Ctx.InputCursorPos;
                }
                if (g_Ctx.KeyPressed[VK_RIGHT]) {
                    if (shiftDown) { if (g_Ctx.InputSelectionStart == -1) g_Ctx.InputSelectionStart = g_Ctx.InputCursorPos; }
                    else { g_Ctx.InputSelectionStart = g_Ctx.InputSelectionEnd = -1; }
                    if (g_Ctx.InputCursorPos < static_cast<int>(text.size())) g_Ctx.InputCursorPos++;
                    if (shiftDown) g_Ctx.InputSelectionEnd = g_Ctx.InputCursorPos;
                }
                if (g_Ctx.KeyPressed[VK_BACK] && !isReadOnly) {
                    if (!DeleteSelection() && g_Ctx.InputCursorPos > 0) {
                        text.erase(g_Ctx.InputCursorPos - 1, 1);
                        g_Ctx.InputCursorPos--;
                        valueChanged = true;
                    }
                }
                if (g_Ctx.KeyPressed[VK_DELETE] && !isReadOnly) {
                    if (!DeleteSelection() && g_Ctx.InputCursorPos < static_cast<int>(text.size())) {
                        text.erase(g_Ctx.InputCursorPos, 1);
                        valueChanged = true;
                    }
                }
                if (g_Ctx.KeyPressed[VK_HOME]) {
                    if (shiftDown) { if (g_Ctx.InputSelectionStart == -1) g_Ctx.InputSelectionStart = g_Ctx.InputCursorPos; }
                    else { g_Ctx.InputSelectionStart = g_Ctx.InputSelectionEnd = -1; }
                    g_Ctx.InputCursorPos = 0;
                    if (shiftDown) g_Ctx.InputSelectionEnd = g_Ctx.InputCursorPos;
                }
                if (g_Ctx.KeyPressed[VK_END]) {
                    if (shiftDown) { if (g_Ctx.InputSelectionStart == -1) g_Ctx.InputSelectionStart = g_Ctx.InputCursorPos; }
                    else { g_Ctx.InputSelectionStart = g_Ctx.InputSelectionEnd = -1; }
                    g_Ctx.InputCursorPos = static_cast<int>(text.size());
                    if (shiftDown) g_Ctx.InputSelectionEnd = g_Ctx.InputCursorPos;
                }
                if (g_Ctx.KeyPressed[VK_RETURN] || g_Ctx.KeyPressed[VK_ESCAPE]) {
                    if (g_Ctx.KeyPressed[VK_ESCAPE] && (flags & ShadowInputTextFlags_EscapeClearsAll)) {
                        if (!text.empty() && !isReadOnly) {
                            text.clear();
                            g_Ctx.InputCursorPos = 0;
                            g_Ctx.InputSelectionStart = -1;
                            g_Ctx.InputSelectionEnd = -1;
                            valueChanged = true;
                        }
                        else {
                            g_Ctx.ActiveInputId = 0;
                        }
                    }
                    else {
                        g_Ctx.ActiveInputId = 0;
                    }
                }
            }
        }

        Color bgColor = disabled ? GetColor(GuiCol_ControlDisabled) : (isActive ? GetColor(GuiCol_FrameBgHovered) : (hovered ? GetColor(GuiCol_FrameBgHovered) : GetColor(GuiCol_FrameBg)));
        GetWindowDrawList()->AddRectFilled(pos, size, bgColor);

        PushClipRect(pos, { pos.x + size.x, pos.y + size.y });

        std::string displayText = text;
        if (isPassword) displayText.assign(text.size(), '*');

        float maxTextWidth = size.x - g_Ctx.Style.FramePadding.x * 2.f;
        float textScrollX = 0.f;

        if (isActive) {
            std::string preCursor = displayText.substr(0, g_Ctx.InputCursorPos);
            float curW = MeasureTextSize(preCursor).x;
            if (curW > maxTextWidth) {
                textScrollX = curW - maxTextWidth;
            }
        }

        float textX = pos.x + g_Ctx.Style.FramePadding.x - textScrollX;
        float textY = pos.y + g_Ctx.Style.FramePadding.y;

        if (isActive && g_Ctx.InputSelectionStart != -1 && g_Ctx.InputSelectionEnd != -1 && g_Ctx.InputSelectionStart != g_Ctx.InputSelectionEnd) {
            int s = std::clamp(std::min(g_Ctx.InputSelectionStart, g_Ctx.InputSelectionEnd), 0, (int)displayText.size());
            int e = std::clamp(std::max(g_Ctx.InputSelectionStart, g_Ctx.InputSelectionEnd), 0, (int)displayText.size());
            std::string preStr = displayText.substr(0, s);
            std::string selStr = displayText.substr(s, e - s);
            float preW = MeasureTextSize(preStr).x;
            float selW = MeasureTextSize(selStr).x;
            GetWindowDrawList()->AddRectFilled({ textX + preW, pos.y + g_Ctx.Style.InputTextSelectionPaddingY }, { selW, size.y - g_Ctx.Style.InputTextSelectionPaddingY * 2.f }, GetColor(GuiCol_SliderGrab));
        }

        if (text.empty() && !isActive && !hint.empty()) {
            Color hintColor = GetColor(GuiCol_TextDisabled);
            GetWindowDrawList()->AddText({ textX, textY }, hintColor, hint);
        }
        else {
            Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
            GetWindowDrawList()->AddText({ textX, textY }, textColor, displayText);
        }

        if (isActive) {
            std::string preCursor = displayText.substr(0, g_Ctx.InputCursorPos);
            float curW = MeasureTextSize(preCursor).x;

            uint64_t currentMS = static_cast<uint64_t>(g_Ctx.RealTimeSeconds * 1000.0);

            if ((currentMS / g_Ctx.Style.InputTextCursorBlinkIntervalMS) % 2 == 0) {
                Color cursorColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
                GetWindowDrawList()->AddRectFilled({ textX + curW, textY }, { g_Ctx.Style.InputTextCursorWidth, size.y - g_Ctx.Style.InputTextSelectionPaddingY * 2.f }, cursorColor);
            }
        }
        PopClipRect();

        return valueChanged;
    }

    inline bool BeginMenuBar() {
        g_Ctx.MenuBarStack++;
        if (!g_Ctx.CurrentWindow || !(g_Ctx.CurrentWindowFlags & ShadowWindowFlags_MenuBar)) {
            g_Ctx.MenuStateStack.push_back({ false });
            return false;
        }

        // 【新增】暂时弹出 Begin 压入的内容区专属剪裁区域，回到受主窗口保护的层级，防止菜单栏被误切
        PopClipRect();

        g_Ctx.BackupMenuBarCursor = g_Ctx.Cursor;
        g_Ctx.BackupMenuBarLastItemMaxX = g_Ctx.LastItemMaxX;
        g_Ctx.BackupMenuBarClipMin = g_Ctx.ClipMin;
        g_Ctx.BackupMenuBarClipMax = g_Ctx.ClipMax;
        g_Ctx.BackupMenuBarClippingEnabled = g_Ctx.ClippingEnabled;

        float titleBarHeight = (g_Ctx.CurrentWindowFlags & ShadowWindowFlags_NoTitleBar) ? 0.f : std::max(g_Ctx.Style.TitleBarMinHeight, g_Ctx.ItemHeight + g_Ctx.Style.TitleBarPaddingY);

        // 与 Dear ImGui 一致：菜单栏高度等于 ItemHeight，菜单项本身占满菜单栏高度
        float menuBarHeight = g_Ctx.ItemHeight;

        SetCursorScreenPos({ g_Ctx.WindowPos.x + g_Ctx.Style.WindowPadding.x, g_Ctx.WindowPos.y + titleBarHeight });
        g_Ctx.LastItemMaxX = g_Ctx.Cursor.x;

        PushClipRect({ g_Ctx.WindowPos.x, g_Ctx.WindowPos.y + titleBarHeight }, { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x, g_Ctx.WindowPos.y + titleBarHeight + menuBarHeight });

        g_Ctx.MenuStateStack.push_back({ true });
        return true;
    }

    inline void EndMenuBar() {
        if (g_Ctx.MenuStateStack.empty()) return;
        MenuState state = g_Ctx.MenuStateStack.back();
        g_Ctx.MenuStateStack.pop_back();

        if (state.IsOpen) {
            PopClipRect();
            SetCursorScreenPos(g_Ctx.BackupMenuBarCursor);
            g_Ctx.LastItemMaxX = g_Ctx.BackupMenuBarLastItemMaxX;

            // 【新增】菜单栏绘制完毕，重新压入内容区专属剪裁区域，继续保护下方滚动内容
            PushClipRect(
                { g_Ctx.WindowPos.x, g_Ctx.ContentStartY },
                { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x, g_Ctx.WindowPos.y + g_Ctx.WindowSize.y }
            );
        }
        g_Ctx.MenuBarStack--;
    }

    inline bool BeginMenu(std::string_view name, bool enabled = true) {
        g_Ctx.MenuStack++;
        std::string_view display; size_t id; ParseLabel(name, display, id);

        bool in_menubar = (g_Ctx.MenuBarStack > 0 && g_Ctx.MenuStack == 1);
        float textWidth = MeasureTextSize(display).x;
        float paddingX = g_Ctx.Style.FramePadding.x * 2.f;
        float arrowSize = g_Ctx.ItemHeight * g_Ctx.Style.MenuArrowSizeRatio;

        float minWidth;
        Vec2 itemSize;
        if (in_menubar) {
            minWidth = textWidth + paddingX;
            itemSize = { minWidth, g_Ctx.ItemHeight };
        }
        else {
            minWidth = textWidth + paddingX + arrowSize + g_Ctx.Style.MenuArrowSpacing;
            float availableWidth = g_Ctx.WindowSize.x - g_Ctx.Style.WindowPadding.x * 2.f;
            itemSize = { std::max(minWidth, availableWidth), g_Ctx.ItemHeight };
        }

        bool disabled = IsDisabled() || !enabled;

        Vec2 pos;
        if (in_menubar) {
            pos = { g_Ctx.LastItemMaxX, g_Ctx.Cursor.y };
            SetCursorScreenPos(pos);
        }
        else {
            pos = g_Ctx.Cursor;
        }

        bool hovered = !disabled && IsMouseHovering(pos, itemSize);
        if (hovered && !in_menubar) {
            while (g_Ctx.ActivePopups.size() > g_Ctx.PopupStack.size()) {
                g_Ctx.ActivePopups.pop_back();
            }
        }
        if (hovered && g_Ctx.MouseDown) g_Ctx.ActiveId = id;

        bool clicked = hovered && g_Ctx.MouseClicked;
        bool is_open = IsPopupOpen(id);

        if (hovered && !is_open) {
            if (clicked || (in_menubar && g_Ctx.MenuBarClickedThisFrame)) {
                if (in_menubar) {
                    g_Ctx.ActivePopups.clear();
                    g_Ctx.MenuBarClickedThisFrame = true;
                }
                OpenPopup(id);
                is_open = true;
            }
            else if (!in_menubar) {
                OpenPopup(id);
                is_open = true;
            }
        }

        Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
        Color bgColor = is_open ? GetColor(GuiCol_FrameBgHovered) : (hovered ? GetColor(GuiCol_FrameBgHovered) : GetColor(GuiCol_Transparent));

        if (bgColor.a > 0.0f) {
            if (!in_menubar && g_Ctx.InPopup && !g_Ctx.PopupStack.empty()) {
                g_Ctx.PopupStack.back().RightAlignCmds.push_back({ GetWindowDrawList()->GetCmdBuffer().size(), RightAlignCmdType::RectBackground });
            }
            GetWindowDrawList()->AddRectFilled(pos, itemSize, bgColor);
        }

        GetWindowDrawList()->AddText({ pos.x + g_Ctx.Style.FramePadding.x, pos.y + g_Ctx.Style.FramePadding.y }, textColor, display);

        if (!in_menubar) {
            float centerY = pos.y + itemSize.y * 0.5f;
            float arrowX = pos.x + itemSize.x - arrowSize - g_Ctx.Style.FramePadding.x;
            Vec2 p1 = { arrowX + arrowSize * 0.25f, centerY - arrowSize * 0.5f };
            Vec2 p2 = { arrowX + arrowSize * 0.75f, centerY };
            Vec2 p3 = { arrowX + arrowSize * 0.25f, centerY + arrowSize * 0.5f };
            Color triCol = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
            if (g_Ctx.InPopup && !g_Ctx.PopupStack.empty()) {
                g_Ctx.PopupStack.back().RightAlignCmds.push_back({ GetWindowDrawList()->GetCmdBuffer().size(), RightAlignCmdType::TriangleArrow });
            }
            GetWindowDrawList()->AddTriangleFilled(p1, p2, p3, triCol);
        }

        if (in_menubar) {
            g_Ctx.LastItemMaxX = pos.x + itemSize.x;
            SetLastItemInfo(pos, { pos.x + itemSize.x, pos.y + itemSize.y }, id, disabled);
        }
        else {
            SetLastItemInfo(pos, { pos.x + itemSize.x, pos.y + itemSize.y }, id, disabled);
            g_Ctx.LastItemMaxX = std::max(g_Ctx.LastItemMaxX, pos.x + minWidth);
            SetCursorPosY(GetCursorPosY() + itemSize.y);
            SetCursorPosX(g_Ctx.IndentX);
        }

        bool popup_ret = false;
        if (is_open) {
            if (in_menubar) {
                SetNextWindowPos({ pos.x, pos.y + itemSize.y });
            }
            else {
                SetNextWindowPos({ pos.x + itemSize.x, pos.y });
            }
            PushStyleVar(GuiStyleVar_WindowPadding, { g_Ctx.Style.FramePadding.x, g_Ctx.Style.FramePadding.y });
            popup_ret = BeginPopup(name, ShadowWindowFlags_NoMove | ShadowWindowFlags_NoTitleBar | ShadowWindowFlags_NoScrollbar | ShadowWindowFlags_NoResize);

            if (!popup_ret) {
                PopStyleVar();
                is_open = false;
            }
        }

        g_Ctx.MenuStateStack.push_back({ popup_ret });
        return popup_ret;
    }

    inline void EndMenu() {
        if (g_Ctx.MenuStateStack.empty()) return;
        MenuState state = g_Ctx.MenuStateStack.back();
        g_Ctx.MenuStateStack.pop_back();

        if (state.IsOpen) {
            EndPopup();
            PopStyleVar();
        }
        g_Ctx.MenuStack--;
    }

    inline bool MenuItem(std::string_view name, std::string_view shortcut = "", bool* p_selected = nullptr, bool enabled = true) {
        std::string_view display; size_t id; ParseLabel(name, display, id);

        bool in_menubar = (g_Ctx.MenuBarStack > 0 && g_Ctx.MenuStack == 0);

        float textWidth = MeasureTextSize(display).x;
        float shortcutWidth = shortcut.empty() ? 0.f : MeasureTextSize(shortcut).x;
        float paddingX = g_Ctx.Style.FramePadding.x * 2.f;

        float minWidth;
        Vec2 itemSize;
        if (in_menubar) {
            minWidth = textWidth + paddingX;
            itemSize = { minWidth, g_Ctx.ItemHeight };
        }
        else {
            minWidth = textWidth + (shortcutWidth > 0.f ? shortcutWidth + g_Ctx.Style.MenuShortcutSpacing : 0.f) + paddingX;
            float availableWidth = g_Ctx.WindowSize.x - g_Ctx.Style.WindowPadding.x * 2.f;
            itemSize = { std::max(minWidth, availableWidth), g_Ctx.ItemHeight };
        }

        bool disabled = IsDisabled() || !enabled;

        Vec2 pos;
        if (in_menubar) {
            pos = { g_Ctx.LastItemMaxX, g_Ctx.Cursor.y };
            SetCursorScreenPos(pos);
        }
        else {
            pos = g_Ctx.Cursor;
        }

        bool hovered = !disabled && IsMouseHovering(pos, itemSize);
        if (hovered && !in_menubar) {
            while (g_Ctx.ActivePopups.size() > g_Ctx.PopupStack.size()) {
                g_Ctx.ActivePopups.pop_back();
            }
        }
        if (hovered && g_Ctx.MouseDown) g_Ctx.ActiveId = id;

        bool clicked = hovered && g_Ctx.MouseClicked;
        bool selected = p_selected ? *p_selected : false;

        if (clicked) {
            if (p_selected) *p_selected = !*p_selected;
            g_Ctx.ActivePopups.clear();
            g_Ctx.MenuBarClickedThisFrame = false;
            g_Ctx.MouseClicked = false;
        }

        Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
        Color bgColor = (hovered || selected) ? GetColor(GuiCol_FrameBgHovered) : GetColor(GuiCol_Transparent);

        if (bgColor.a > 0.0f) {
            if (!in_menubar && g_Ctx.InPopup && !g_Ctx.PopupStack.empty()) {
                g_Ctx.PopupStack.back().RightAlignCmds.push_back({ GetWindowDrawList()->GetCmdBuffer().size(), RightAlignCmdType::RectBackground });
            }
            GetWindowDrawList()->AddRectFilled(pos, itemSize, bgColor);
        }

        GetWindowDrawList()->AddText({ pos.x + g_Ctx.Style.FramePadding.x, pos.y + g_Ctx.Style.FramePadding.y }, textColor, display);

        if (!shortcut.empty() && !in_menubar) {
            Color shortcutColor = GetColor(GuiCol_TextDisabled);
            if (g_Ctx.InPopup && !g_Ctx.PopupStack.empty()) {
                g_Ctx.PopupStack.back().RightAlignCmds.push_back({ GetWindowDrawList()->GetCmdBuffer().size(), RightAlignCmdType::TextShortcut });
            }
            GetWindowDrawList()->AddText({ pos.x + itemSize.x - shortcutWidth - g_Ctx.Style.FramePadding.x, pos.y + g_Ctx.Style.FramePadding.y }, shortcutColor, shortcut);
        }

        if (in_menubar) {
            g_Ctx.LastItemMaxX = pos.x + itemSize.x;
            SetLastItemInfo(pos, { pos.x + itemSize.x, pos.y + itemSize.y }, id, disabled);
        }
        else {
            SetLastItemInfo(pos, { pos.x + itemSize.x, pos.y + itemSize.y }, id, disabled);
            g_Ctx.LastItemMaxX = std::max(g_Ctx.LastItemMaxX, pos.x + minWidth);
            SetCursorPosY(GetCursorPosY() + itemSize.y);
            SetCursorPosX(g_Ctx.IndentX);
        }

        return clicked;
    }

    inline void CheckAndDrawErrors() {
        std::string errorMsg;
        if (g_Ctx.BeginStack > 0) errorMsg = std::format("ERROR: Begin() called {} time(s) without matching End()!", g_Ctx.BeginStack);
        else if (g_Ctx.BeginStack < 0) errorMsg = std::format("ERROR: End() called {} time(s) without matching Begin()!", -g_Ctx.BeginStack);
        else if (g_Ctx.PopupStack.size() > 0) errorMsg = std::format("ERROR: BeginPopup() called {} time(s) without matching EndPopup()!", g_Ctx.PopupStack.size());
        else if (g_Ctx.TabBarStack > 0) errorMsg = std::format("ERROR: BeginTabBar() called {} time(s) without matching EndTabBar()!", g_Ctx.TabBarStack);
        else if (g_Ctx.TabBarStack < 0) errorMsg = std::format("ERROR: EndTabBar() called {} time(s) without matching BeginTabBar()!", -g_Ctx.TabBarStack);
        else if (g_Ctx.TabItemStack > 0) errorMsg = std::format("ERROR: BeginTabItem() called {} time(s) without matching EndTabItem()!", g_Ctx.TabItemStack);
        else if (g_Ctx.TabItemStack < 0) errorMsg = std::format("ERROR: EndTabItem() called {} time(s) without matching BeginTabItem()!", -g_Ctx.TabItemStack);
        else if (g_Ctx.TreeNodeStack > 0) errorMsg = std::format("ERROR: TreeNode() called {} time(s) without matching TreePop()!", g_Ctx.TreeNodeStack);
        else if (g_Ctx.TreeNodeStack < 0) errorMsg = std::format("ERROR: TreePop() called {} time(s) without matching TreeNode()!", -g_Ctx.TreeNodeStack);
        else if (g_Ctx.ListBoxStack > 0) errorMsg = std::format("ERROR: BeginListBox() called {} time(s) without matching EndListBox()!", g_Ctx.ListBoxStack);
        else if (g_Ctx.ListBoxStack < 0) errorMsg = std::format("ERROR: EndListBox() called {} time(s) without matching BeginListBox()!", -g_Ctx.ListBoxStack);
        else if (g_Ctx.MenuBarStack > 0) errorMsg = std::format("ERROR: BeginMenuBar() called {} time(s) without matching EndMenuBar()!", g_Ctx.MenuBarStack);
        else if (g_Ctx.MenuBarStack < 0) errorMsg = std::format("ERROR: EndMenuBar() called {} time(s) without matching BeginMenuBar()!", -g_Ctx.MenuBarStack);
        else if (g_Ctx.MenuStack > 0) errorMsg = std::format("ERROR: BeginMenu() called {} time(s) without matching EndMenu()!", g_Ctx.MenuStack);
        else if (g_Ctx.MenuStack < 0) errorMsg = std::format("ERROR: EndMenu() called {} time(s) without matching BeginMenu()!", -g_Ctx.MenuStack);
        else if (g_Ctx.FontStack.size() > 0) errorMsg = std::format("ERROR: PushFont() called {} time(s) without matching PopFont()!", g_Ctx.FontStack.size());
        else if (g_Ctx.TextureStack.size() > 0) errorMsg = std::format("ERROR: PushTexture() called {} time(s) without matching PopTexture()!", g_Ctx.TextureStack.size());
        else if (g_Ctx.TextOutlineStack.size() > 0) errorMsg = std::format("ERROR: PushTextOutline() called {} time(s) without matching PopTextOutline()!", g_Ctx.TextOutlineStack.size());
        else if (g_Ctx.TextPixelSnapStack.size() > 0) errorMsg = std::format("ERROR: PushTextPixelSnap() called {} time(s) without matching PopTextPixelSnap()!", g_Ctx.TextPixelSnapStack.size());
        else if (g_Ctx.ClipStack.size() > 0) errorMsg = std::format("ERROR: PushClipRect() called {} time(s) without matching PopClipRect()!", g_Ctx.ClipStack.size());
        else if (g_Ctx.DisabledStack.size() > 0) errorMsg = std::format("ERROR: BeginDisabled() called {} time(s) without matching EndDisabled()!", g_Ctx.DisabledStack.size());
        else if (g_Ctx.TextWrapPosStack.size() > 0) errorMsg = std::format("ERROR: PushTextWrapPos() called {} time(s) without matching PopTextWrapPos()!", g_Ctx.TextWrapPosStack.size());
        else if (g_Ctx.StyleVarStack.size() > 0) errorMsg = std::format("ERROR: PushStyleVar() called {} time(s) without matching PopStyleVar()!", g_Ctx.StyleVarStack.size());
        else if (g_Ctx.StyleColorStack.size() > 0) errorMsg = std::format("ERROR: PushStyleColor() called {} time(s) without matching PopStyleColor()!", g_Ctx.StyleColorStack.size());
        else if (g_Ctx.InTooltip) errorMsg = "ERROR: BeginTooltip() called without matching EndTooltip()!";

        if (!errorMsg.empty()) {
            GetWindowDrawList()->AddText(g_Ctx.DefaultFont, 1.0f, GetColor(GuiCol_TextShadow), GetColor(GuiCol_TextOutline), { 5.f, 5.f }, GetColor(GuiCol_ErrorText), errorMsg);
        }
    }

    inline void TextColored(Color color, std::string_view text, Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return;

        Vec2 size = MeasureTextSize(text);
        float itemWidth = size_arg.x > 0.f ? size_arg.x : size.x;
        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;

        bool disabled = IsDisabled();
        Vec2 pos = g_Ctx.Cursor;
        Vec2 min = pos;
        Vec2 max = { pos.x + itemWidth, pos.y + itemHeight };

        ItemSize(min, max);
        if (!ItemAdd(min, max, ++g_Ctx.WidgetCount, disabled)) {
            return;
        }

        Color drawColor = GetColor(color);

        GetWindowDrawList()->AddText({ pos.x, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, drawColor, text);
    }

    inline void TextWrapped(Color color, std::string_view text, Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return;

        bool disabled = IsDisabled();
        Color drawColor = GetColor(color);

        float wrapMaxX = g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - GetRightMargin();
        if (g_Ctx.ClippingEnabled) {
            wrapMaxX = std::min(wrapMaxX, g_Ctx.ClipMax.x);
        }

        if (!g_Ctx.TextWrapPosStack.empty()) {
            wrapMaxX = g_Ctx.TextWrapPosStack.back();
        }

        float maxLineWidth = wrapMaxX - g_Ctx.Cursor.x;
        if (size_arg.x > 0.f) maxLineWidth = size_arg.x;
        if (maxLineWidth <= 1.0f) maxLineWidth = 1.0f;

        std::wstring wtext = ToWString(text);
        float startY = g_Ctx.Cursor.y;
        float maxLineX = g_Ctx.Cursor.x;

        std::wstring currentLine;
        float currentLineWidth = 0.f;

        auto FlushLine = [&](bool addSpacing) {
            if (!currentLine.empty()) {
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, currentLine.c_str(), static_cast<int>(currentLine.size()), nullptr, 0, nullptr, nullptr);
                std::string utf8Line(size_needed, 0);
                WideCharToMultiByte(CP_UTF8, 0, currentLine.c_str(), static_cast<int>(currentLine.size()), &utf8Line[0], size_needed, nullptr, nullptr);

                if (IsRectVisible(g_Ctx.Cursor, { currentLineWidth, g_Ctx.ItemHeight })) {
                    GetWindowDrawList()->AddText({ g_Ctx.Cursor.x, g_Ctx.Cursor.y + g_Ctx.Style.FramePadding.y }, drawColor, utf8Line);
                }
                maxLineX = std::max(maxLineX, g_Ctx.Cursor.x + currentLineWidth);
            }
            if (addSpacing) {
                SetCursorPosY(GetCursorPosY() + g_Ctx.ItemHeight + g_Ctx.Style.ItemSpacing.y);
                SetCursorPosX(g_Ctx.IndentX);
            }
            currentLine.clear();
            currentLineWidth = 0.f;
            };

        size_t i = 0;
        size_t len = wtext.length();

        while (i < len) {
            if (wtext[i] == L'\n') {
                FlushLine(true);
                i++;
                continue;
            }

            size_t wordStart = i;
            bool isSpace = (wtext[i] == L' ');
            if (isSpace) {
                while (i < len && wtext[i] == L' ') {
                    i++;
                }
            }
            else {
                while (i < len && wtext[i] != L' ' && wtext[i] != L'\n') {
                    i++;
                }
            }

            std::wstring token = wtext.substr(wordStart, i - wordStart);

            if (isSpace) {
                float tokenWidth = 0.f;
                for (wchar_t ch : token) tokenWidth += MeasureCharWidth(ch);
                if (currentLineWidth + tokenWidth <= maxLineWidth) {
                    currentLine += token;
                    currentLineWidth += tokenWidth;
                }
                else {
                    FlushLine(true);
                }
            }
            else {
                float tokenWidth = 0.f;
                for (wchar_t ch : token) tokenWidth += MeasureCharWidth(ch);

                if (currentLineWidth + tokenWidth <= maxLineWidth) {
                    currentLine += token;
                    currentLineWidth += tokenWidth;
                }
                else if (currentLineWidth > 0.f) {
                    FlushLine(true);
                    if (tokenWidth <= maxLineWidth) {
                        currentLine = token;
                        currentLineWidth = tokenWidth;
                    }
                    else {
                        for (wchar_t ch : token) {
                            float chWidth = MeasureCharWidth(ch);
                            if (currentLineWidth + chWidth > maxLineWidth && currentLineWidth > 0.f) FlushLine(true);
                            currentLine += ch;
                            currentLineWidth += chWidth;
                        }
                    }
                }
                else {
                    for (wchar_t ch : token) {
                        float chWidth = MeasureCharWidth(ch);
                        if (currentLineWidth + chWidth > maxLineWidth && currentLineWidth > 0.f) FlushLine(true);
                        currentLine += ch;
                        currentLineWidth += chWidth;
                    }
                }
            }
        }

        if (!currentLine.empty()) {
            FlushLine(false);
            SetCursorPosY(GetCursorPosY() + g_Ctx.ItemHeight + g_Ctx.Style.ItemSpacing.y);
        }

        float finalY = g_Ctx.Cursor.y - g_Ctx.Style.ItemSpacing.y;
        float blockHeight = finalY - startY;
        if (size_arg.y > 0.f && size_arg.y > blockHeight) {
            g_Ctx.Cursor.y += (size_arg.y - blockHeight);
            finalY = startY + size_arg.y;
        }

        SetLastItemInfo({ g_Ctx.WindowPos.x + g_Ctx.Style.WindowPadding.x + g_Ctx.IndentX, startY }, { maxLineX, finalY }, ++g_Ctx.WidgetCount, disabled);
        g_Ctx.LastItemMaxX = maxLineX;
        SetCursorPosX(g_Ctx.IndentX);
    }

    inline void Separator(Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return;

        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.Style.SeparatorHeight;
        float x1 = g_Ctx.Cursor.x;
        float y = g_Ctx.Cursor.y + itemHeight * 0.5f;
        float x2 = size_arg.x > 0.f ? (g_Ctx.Cursor.x + size_arg.x) : (g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - GetRightMargin());

        Vec2 min = { x1, g_Ctx.Cursor.y };
        Vec2 max = { x2, g_Ctx.Cursor.y + itemHeight };

        ItemSize(min, max);
        if (size_arg.x > 0.f) {
            g_Ctx.LastItemMaxX = std::max(g_Ctx.LastItemMaxX, x2);
        }

        if (ItemAdd(min, max, ++g_Ctx.WidgetCount, false)) {
            GetWindowDrawList()->AddLine({ x1, y }, { x2, y }, GetColor(GuiCol_Separator), g_Ctx.Style.SeparatorThickness);
        }
    }

    inline void Dummy(Vec2 size) {
        if (!g_Ctx.InActiveTab) return;

        Vec2 min = g_Ctx.Cursor;
        Vec2 max = { g_Ctx.Cursor.x + size.x, g_Ctx.Cursor.y + size.y };

        ItemSize(min, max);
        ItemAdd(min, max, ++g_Ctx.WidgetCount, false);
    }

    inline void Text(std::string_view text, Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return;

        Vec2 size = MeasureTextSize(text);
        float itemWidth = size_arg.x > 0.f ? size_arg.x : size.x;
        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;

        bool disabled = IsDisabled();
        Vec2 pos = g_Ctx.Cursor;
        Vec2 min = pos;
        Vec2 max = { pos.x + itemWidth, pos.y + itemHeight };

        ItemSize(min, max);
        if (!ItemAdd(min, max, ++g_Ctx.WidgetCount, disabled)) {
            return;
        }

        Color drawColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);

        GetWindowDrawList()->AddText({ pos.x, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, drawColor, text);
    }

    inline void TextDisabled(std::string_view text, Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return;

        Vec2 size = MeasureTextSize(text);
        float itemWidth = size_arg.x > 0.f ? size_arg.x : size.x;
        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;

        Vec2 pos = g_Ctx.Cursor;
        Vec2 min = pos;
        Vec2 max = { pos.x + itemWidth, pos.y + itemHeight };

        ItemSize(min, max);
        if (!ItemAdd(min, max, ++g_Ctx.WidgetCount, true)) {
            return;
        }

        Color drawColor = GetColor(GuiCol_TextDisabled);
        GetWindowDrawList()->AddText({ pos.x, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, drawColor, text);
    }

    inline void NewFrame(SDK::UCanvas* Canvas) {
        g_Ctx.Canvas = Canvas;

        if (auto world = SDK::UWorld::GetWorld()) {
            g_Ctx.RealTimeSeconds = SDK::UGameplayStatics::GetRealTimeSeconds(world);
            g_Ctx.DeltaTime = SDK::UGameplayStatics::GetWorldDeltaSeconds(world);
        }
        else {
            g_Ctx.RealTimeSeconds = 0.0;
            g_Ctx.DeltaTime = 0.0;
        }

        // 高精度时钟接管长按连发脉冲触发
        if (g_Ctx.Style.KeyRepeatRate > 0.0f) {
            for (int vk = 0; vk < 256; ++vk) {
                if (g_Ctx.KeyStates[vk]) {
                    double t = g_Ctx.RealTimeSeconds - g_Ctx.KeyPressTime[vk];
                    double t_prev = t - g_Ctx.DeltaTime;
                    if (t >= g_Ctx.Style.KeyRepeatDelay) {
                        int count_curr = static_cast<int>((t - g_Ctx.Style.KeyRepeatDelay) / g_Ctx.Style.KeyRepeatRate);
                        int count_prev = (t_prev >= g_Ctx.Style.KeyRepeatDelay) ? static_cast<int>((t_prev - g_Ctx.Style.KeyRepeatDelay) / g_Ctx.Style.KeyRepeatRate) : -1;
                        if (count_curr > count_prev) {
                            g_Ctx.KeyPressed[vk] = true;
                        }
                    }
                }
            }
        }

        uint64_t currentMS = static_cast<uint64_t>(g_Ctx.RealTimeSeconds * 1000.0);

        if (!g_Ctx.DefaultFont) {
            if (SDK::UEngine::GetEngine()) { g_Ctx.DefaultFont = SDK::UEngine::GetEngine()->LargeFont; }
        }

        if (!g_Ctx.StyleInitialized) {
            StyleColorsOcean();
            g_Ctx.StyleInitialized = true;
        }

        g_Ctx.FrameCount++;

        g_Ctx.ActiveIdPreviousFrame = g_Ctx.ActiveId;
        if (!g_Ctx.MouseDown && !g_Ctx.RightMouseDown && !g_Ctx.MiddleMouseDown && g_Ctx.ActiveInputId == 0 && g_Ctx.AssigningHotkey == nullptr && g_Ctx.DraggingSliderId == 0) {
            g_Ctx.ActiveId = 0;
        }

        g_Ctx.MouseClickedThisFrame = g_Ctx.MouseClicked;
        g_Ctx.RightMouseClickedThisFrame = g_Ctx.RightMouseClicked;
        g_Ctx.MiddleMouseClickedThisFrame = g_Ctx.MiddleMouseClicked;

        if (g_Ctx.MouseClicked && !g_Ctx.ActivePopups.empty()) {
            bool clickedInsideAny = false;
            for (int i = (int)g_Ctx.ActivePopups.size() - 1; i >= 0; --i) {
                size_t pid = g_Ctx.ActivePopups[i];
                auto& win = g_Ctx.Windows[pid];
                if (g_Ctx.MousePos.x >= win.Pos.x && g_Ctx.MousePos.x <= win.Pos.x + win.Size.x &&
                    g_Ctx.MousePos.y >= win.Pos.y && g_Ctx.MousePos.y <= win.Pos.y + win.Size.y) {
                    g_Ctx.ActivePopups.resize(i + 1);
                    clickedInsideAny = true;
                    break;
                }
            }
            if (!clickedInsideAny) {
                g_Ctx.ActivePopups.clear();
                g_Ctx.ActiveInputId = 0;
                g_Ctx.MouseClicked = false;
                g_Ctx.MenuBarClickedThisFrame = false;
            }
        }

        if (g_Ctx.ActivePopups.empty()) {
            g_Ctx.MenuBarClickedThisFrame = false;
        }

        for (auto& pair : g_Ctx.Windows) {
            pair.second.DrawList.Clear();
        }
        g_Ctx.TooltipDrawList.Clear();
        g_Ctx.BackgroundDrawList.Clear();
        g_Ctx.ForegroundDrawList.Clear();

        g_Ctx.HoveredWindowId = 0;
        g_Ctx.PopupStack.clear();

        bool popupActive = !g_Ctx.ActivePopups.empty();
        if (!popupActive) {
            for (auto it = g_Ctx.WindowDisplayOrder.rbegin(); it != g_Ctx.WindowDisplayOrder.rend(); ++it) {
                size_t id = *it;
                auto& win = g_Ctx.Windows[id];
                if (g_Ctx.FrameCount - win.LastAccessedFrame <= 1) {
                    if (!(win.CurrentWindowFlags & ShadowWindowFlags_NoMouseInputs)) {
                        if (g_Ctx.MousePos.x >= win.Pos.x && g_Ctx.MousePos.x <= win.Pos.x + win.Size.x &&
                            g_Ctx.MousePos.y >= win.Pos.y && g_Ctx.MousePos.y <= win.Pos.y + win.Size.y) {
                            g_Ctx.HoveredWindowId = id;
                            break;
                        }
                    }
                }
            }
        }

        g_Ctx.FontStack.clear();
        g_Ctx.TextureStack.clear();
        g_Ctx.TextOutlineStack.clear();
        g_Ctx.TextPixelSnapStack.clear();

        while (!g_Ctx.StyleVarStack.empty()) {
            PopStyleVar();
        }
        while (!g_Ctx.StyleColorStack.empty()) {
            PopStyleColor();
        }

        UpdateItemHeight();

        g_Ctx.InActiveTab = true;
        g_Ctx.BeginStack = 0;
        g_Ctx.TabBarStack = 0;
        g_Ctx.TabItemStack = 0;
        g_Ctx.TreeNodeStack = 0;
        g_Ctx.ListBoxStack = 0;
        g_Ctx.IDStack.clear();
        g_Ctx.MenuBarStack = 0;
        g_Ctx.MenuStack = 0;
        g_Ctx.MenuStateStack.clear();

        g_Ctx.TabBarHoverRects = std::move(g_Ctx.TabBarHoverRectsPending);
        g_Ctx.TabBarHoverRectsPending.clear();

        float dx = g_Ctx.MousePos.x - g_Ctx.MousePosPrev.x;
        float dy = g_Ctx.MousePos.y - g_Ctx.MousePosPrev.y;
        if (dx * dx + dy * dy > g_Ctx.Style.MouseStationaryMoveThreshold * g_Ctx.Style.MouseStationaryMoveThreshold) {
            g_Ctx.MouseStationaryStartTime = currentMS;
            g_Ctx.MouseIsStationary = false;
            g_Ctx.MousePosPrev = g_Ctx.MousePos;
        }
        else {
            if (currentMS - g_Ctx.MouseStationaryStartTime >= static_cast<uint64_t>(g_Ctx.Style.MouseStationaryTimeMS)) {
                g_Ctx.MouseIsStationary = true;
            }
        }

        g_Ctx.HoveredIdPreviousFrame = g_Ctx.HoveredIdCurrentFrame;
        g_Ctx.HoveredIdCurrentFrame = 0;
        g_Ctx.LastHoveredIdEval = 0;
        g_Ctx.WidgetCount = 0;

        g_Ctx.HoveredListBoxIdPreviousFrame = g_Ctx.HoveredListBoxIdCurrentFrame;
        g_Ctx.HoveredListBoxIdCurrentFrame = 0;

        if (currentMS > g_Ctx.SharedDelayExpirationTime) {
            g_Ctx.SharedDelayActive = false;
        }

        g_IO.DisplaySize = { Canvas ? static_cast<float>(Canvas->SizeX) : 0.0f, Canvas ? static_cast<float>(Canvas->SizeY) : 0.0f };
        g_IO.DeltaTime = static_cast<float>(g_Ctx.DeltaTime);
    }

    inline bool Begin(std::string_view name, ShadowWindowFlags flags = ShadowWindowFlags_None) {
        std::string_view display; size_t id;
        ParseLabel(name, display, id);

        auto& win = g_Ctx.Windows[id];
        if (win.Id == 0) {
            win.Id = id;
            win.Name = std::string(display);
            g_Ctx.WindowDisplayOrder.push_back(id);
            win.Pos = g_Ctx.WindowPos;
            win.Size = g_Ctx.WindowSize;
            win.CurrentWindowFlags = flags;
        }
        win.LastAccessedFrame = g_Ctx.FrameCount;
        g_Ctx.CurrentWindow = &win;

        bool mouseOverRaw = (g_Ctx.MousePos.x >= win.Pos.x && g_Ctx.MousePos.x <= win.Pos.x + win.Size.x &&
            g_Ctx.MousePos.y >= win.Pos.y && g_Ctx.MousePos.y <= win.Pos.y + win.Size.y);

        bool hasPopupOpen = !g_Ctx.ActivePopups.empty();

        if (!hasPopupOpen && mouseOverRaw && (g_Ctx.MouseClicked || g_Ctx.RightMouseClicked)) {
            if (g_Ctx.HoveredWindowId == id || g_Ctx.HoveredWindowId == 0) {
                g_Ctx.FocusedWindowId = id;
                auto it = std::find(g_Ctx.WindowDisplayOrder.begin(), g_Ctx.WindowDisplayOrder.end(), id);
                if (it != g_Ctx.WindowDisplayOrder.end()) {
                    g_Ctx.WindowDisplayOrder.erase(it);
                    g_Ctx.WindowDisplayOrder.push_back(id);
                }
            }
        }

        if (g_Ctx.HasNextWindowPos) {
            win.Pos = g_Ctx.NextWindowPos;
            g_Ctx.HasNextWindowPos = false;
        }

        g_Ctx.WindowPos = win.Pos;

        if (g_Ctx.HasNextWindowSize) {
            win.Size = g_Ctx.NextWindowSize;
            g_Ctx.HasNextWindowSize = false;
        }

        g_Ctx.WindowSize = win.Size;
        g_Ctx.IsDragging = win.IsDragging;
        g_Ctx.DragOffset = win.DragOffset;
        g_Ctx.IsResizing = win.IsResizing;
        g_Ctx.ResizeStartPos = win.ResizeStartPos;
        g_Ctx.ResizeStartSize = win.ResizeStartSize;
        g_Ctx.IsHoveringResize = win.IsHoveringResize;
        g_Ctx.ScrollY = win.ScrollY;
        g_Ctx.ContentHeight = win.ContentHeight;
        g_Ctx.IsDraggingScrollbar = win.IsDraggingScrollbar;
        g_Ctx.ScrollDragOffset = win.ScrollDragOffset;
        g_Ctx.ContentStartY = win.ContentStartY;
        g_Ctx.IsScrollApplied = win.IsScrollApplied;
        g_Ctx.CurrentScrollbarWidth = win.CurrentScrollbarWidth;
        g_Ctx.CurrentWindowFlags = flags;
        win.CurrentWindowFlags = flags;

        g_Ctx.LastItemMaxX = 0.f;
        g_Ctx.LastItemMin = { 0.f, 0.f };
        g_Ctx.LastItemMax = { 0.f, 0.f };
        g_Ctx.LastItemId = 0;
        g_Ctx.LastItemDisabled = false;

        g_Ctx.BeginStack++;
        g_Ctx.IsScrollApplied = true;

        float min_x = g_Ctx.Style.WindowMinSize.x;
        float min_y = g_Ctx.Style.WindowMinSize.y;
        float max_x = FLT_MAX;
        float max_y = FLT_MAX;

        if (g_Ctx.HasWindowSizeConstraints) {
            min_x = std::max(min_x, g_Ctx.WindowSizeConstraintMin.x);
            min_y = std::max(min_y, g_Ctx.WindowSizeConstraintMin.y);
            max_x = std::max(min_x, g_Ctx.WindowSizeConstraintMax.x);
            max_y = std::max(min_y, g_Ctx.WindowSizeConstraintMax.y);
            g_Ctx.HasWindowSizeConstraints = false;
        }

        g_Ctx.WindowSize.x = std::clamp(g_Ctx.WindowSize.x, min_x, max_x);
        g_Ctx.WindowSize.y = std::clamp(g_Ctx.WindowSize.y, min_y, max_y);

        bool noResize = (flags & ShadowWindowFlags_NoResize) != 0;
        bool noMove = (flags & ShadowWindowFlags_NoMove) != 0;
        bool noScrollbar = (flags & ShadowWindowFlags_NoScrollbar) != 0;
        bool noTitleBar = (flags & ShadowWindowFlags_NoTitleBar) != 0;
        bool noMouseInputs = (flags & ShadowWindowFlags_NoMouseInputs) != 0;

        float titleBarHeight = noTitleBar ? 0.f : std::max(g_Ctx.Style.TitleBarMinHeight, g_Ctx.ItemHeight + g_Ctx.Style.TitleBarPaddingY);
        float menuBarHeight = (flags & ShadowWindowFlags_MenuBar) ? g_Ctx.ItemHeight : 0.f;
        Vec2 wholeWindowSize = g_Ctx.WindowSize;

        bool hoveringWholeWindow = noMouseInputs ? false : IsMouseHovering(g_Ctx.WindowPos, wholeWindowSize);

        float triSize = g_Ctx.Style.ResizeGripSize;
        Vec2 triPos = { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - triSize, g_Ctx.WindowPos.y + g_Ctx.WindowSize.y - triSize };
        g_Ctx.IsHoveringResize = (!noResize && !noMouseInputs) ? IsMouseHoveringRaw(triPos, { triSize, triSize }) : false;

        bool isOtherDragging = (g_Ctx.DraggingSliderId != 0) || g_Ctx.IsDraggingSV || g_Ctx.IsDraggingHue || g_Ctx.IsDraggingAlpha || g_Ctx.DraggingTabId != 0 || g_Ctx.DraggingTabBarScrollId != 0 || g_Ctx.DraggingListBoxScrollId != 0;

        for (const auto& pair : g_Ctx.Windows) {
            if (pair.first != id) {
                if (pair.second.IsDragging || pair.second.IsResizing || pair.second.IsDraggingScrollbar) {
                    isOtherDragging = true;
                    break;
                }
            }
        }

        if (hoveringWholeWindow && g_Ctx.MouseClicked) {
            g_Ctx.ActiveInputId = 0;
        }

        float viewHeight = (g_Ctx.WindowPos.y + g_Ctx.WindowSize.y) - g_Ctx.ContentStartY - g_Ctx.Style.ResizeGripSize - g_Ctx.Style.WindowScrollBottomPadding;
        if (viewHeight < g_Ctx.Style.WindowScrollMinViewHeight) viewHeight = g_Ctx.Style.WindowScrollMinViewHeight;
        float maxScroll = GetScrollMaxY();
        g_Ctx.ScrollY = std::clamp(g_Ctx.ScrollY, 0.f, maxScroll);

        bool hoveringAnyTabBar = false;
        for (auto& [rectPos, rectSize] : g_Ctx.TabBarHoverRects) {
            if (IsMouseHoveringRaw(rectPos, rectSize)) {
                hoveringAnyTabBar = true;
                break;
            }
        }

        bool overListBox = (g_Ctx.HoveredListBoxIdPreviousFrame != 0);

        if (!noMouseInputs && hoveringWholeWindow && g_Ctx.MouseWheel != 0.f && !hasPopupOpen && !hoveringAnyTabBar && !overListBox) {
            g_Ctx.ScrollY -= g_Ctx.MouseWheel * g_Ctx.Style.ScrollSpeed;
            g_Ctx.ScrollY = std::clamp(g_Ctx.ScrollY, 0.f, maxScroll);
        }

        bool hoveringScrollbar = false;
        float scrollbarWidth = g_Ctx.Style.ScrollbarSize;
        float scrollbarMarginRight = g_Ctx.Style.ScrollbarMargin;
        if (!noScrollbar && g_Ctx.ContentHeight > viewHeight) {
            Vec2 trackPos = { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - scrollbarWidth - scrollbarMarginRight, g_Ctx.ContentStartY };
            Vec2 trackSize = { scrollbarWidth, viewHeight };

            float thumbHeight = std::max(g_Ctx.Style.ScrollbarThumbMinSize, (viewHeight / g_Ctx.ContentHeight) * trackSize.y);
            float thumbY = trackPos.y + (g_Ctx.ScrollY / maxScroll) * (trackSize.y - thumbHeight);
            Vec2 thumbPos = { trackPos.x, thumbY };
            Vec2 thumbSize = { scrollbarWidth, thumbHeight };

            bool hoveringThumb = noMouseInputs ? false : IsMouseHoveringRaw(thumbPos, thumbSize);
            bool hoveringTrack = noMouseInputs ? false : IsMouseHoveringRaw(trackPos, trackSize);

            if (hoveringTrack || hoveringThumb) { hoveringScrollbar = true; }

            if (!overListBox && !isOtherDragging) {
                if (!noMouseInputs && g_Ctx.MouseClicked && hoveringThumb) {
                    g_Ctx.IsDraggingScrollbar = true;
                    g_Ctx.ScrollDragOffset = g_Ctx.MousePos.y - thumbPos.y;
                }
                else if (!noMouseInputs && g_Ctx.MouseClicked && hoveringTrack) {
                    if (g_Ctx.MousePos.y < thumbPos.y) g_Ctx.ScrollY -= viewHeight;
                    else g_Ctx.ScrollY += viewHeight;
                    g_Ctx.ScrollY = std::clamp(g_Ctx.ScrollY, 0.f, maxScroll);
                }
            }

            if (g_Ctx.IsDraggingScrollbar) {
                if (g_Ctx.MouseDown) {
                    float newThumbY = g_Ctx.MousePos.y - g_Ctx.ScrollDragOffset;
                    float ratio = (newThumbY - trackPos.y) / std::max(1.f, trackSize.y - thumbHeight);
                    g_Ctx.ScrollY = std::clamp(ratio * maxScroll, 0.f, maxScroll);
                }
                else {
                    g_Ctx.IsDraggingScrollbar = false;
                }
            }
        }
        else {
            g_Ctx.IsDraggingScrollbar = false;
        }

        if (!noMove && !noMouseInputs && !g_Ctx.IsDragging && hoveringWholeWindow && g_Ctx.MouseClicked && !g_Ctx.IsHoveringResize && !isOtherDragging && !hoveringScrollbar) {
            g_Ctx.IsDragging = true;
            g_Ctx.DragOffset.x = g_Ctx.MousePos.x - g_Ctx.WindowPos.x;
            g_Ctx.DragOffset.y = g_Ctx.MousePos.y - g_Ctx.WindowPos.y;
        }
        if (g_Ctx.IsDragging && isOtherDragging) g_Ctx.IsDragging = false;

        if (!noMove && !noMouseInputs && g_Ctx.IsDragging) {
            g_Ctx.WindowPos.x = g_Ctx.MousePos.x - g_Ctx.DragOffset.x;
            g_Ctx.WindowPos.y = g_Ctx.MousePos.y - g_Ctx.DragOffset.y;
            if (!g_Ctx.MouseDown) g_Ctx.IsDragging = false;
        }

        if (!noResize && !noMouseInputs && !g_Ctx.IsResizing && g_Ctx.IsHoveringResize && g_Ctx.MouseClicked && !isOtherDragging) {
            g_Ctx.IsResizing = true;
            g_Ctx.ResizeStartPos = g_Ctx.MousePos;
            g_Ctx.ResizeStartSize = g_Ctx.WindowSize;
        }
        if (g_Ctx.IsResizing && isOtherDragging) g_Ctx.IsResizing = false;

        if (!noResize && !noMouseInputs && g_Ctx.IsResizing) {
            Vec2 delta = { g_Ctx.MousePos.x - g_Ctx.ResizeStartPos.x, g_Ctx.MousePos.y - g_Ctx.ResizeStartPos.y };
            g_Ctx.WindowSize.x = std::clamp(g_Ctx.ResizeStartSize.x + delta.x, min_x, max_x);
            g_Ctx.WindowSize.y = std::clamp(g_Ctx.ResizeStartSize.y + delta.y, min_y, max_y);
            if (!g_Ctx.MouseDown) g_Ctx.IsResizing = false;
        }

        GetWindowDrawList()->AddRectFilled(g_Ctx.WindowPos, g_Ctx.WindowSize, GetColor(GuiCol_WindowBg));

        // 压入主窗口外框物理剪裁区域
        PushClipRect(
            g_Ctx.WindowPos,
            { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x, g_Ctx.WindowPos.y + g_Ctx.WindowSize.y }
        );

        if (!noTitleBar) {
            GetWindowDrawList()->AddRectFilled(g_Ctx.WindowPos, { g_Ctx.WindowSize.x, titleBarHeight }, GetColor(GuiCol_TitleBarBg));

            {
                float titleTextX = g_Ctx.WindowPos.x + g_Ctx.Style.TitleBarTextOffsetX;
                if (flags & ShadowWindowFlags_TextAlignCenter) {
                    float textW = MeasureTextSize(display).x;
                    titleTextX = g_Ctx.WindowPos.x + (g_Ctx.WindowSize.x - textW) * 0.5f;
                }
                else if (flags & ShadowWindowFlags_TextAlignRight) {
                    float textW = MeasureTextSize(display).x;
                    titleTextX = g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - g_Ctx.Style.TitleBarTextOffsetX - textW;
                }
                GetWindowDrawList()->AddText({ titleTextX, g_Ctx.WindowPos.y + g_Ctx.Style.TitleBarTextOffsetY }, GetColor(GuiCol_Text), display);
            }
        }

        if (flags & ShadowWindowFlags_MenuBar) {
            GetWindowDrawList()->AddRectFilled({ g_Ctx.WindowPos.x, g_Ctx.WindowPos.y + titleBarHeight }, { g_Ctx.WindowSize.x, menuBarHeight }, GetColor(GuiCol_FrameBg));
            GetWindowDrawList()->AddLine({ g_Ctx.WindowPos.x, g_Ctx.WindowPos.y + titleBarHeight + menuBarHeight }, { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x, g_Ctx.WindowPos.y + titleBarHeight + menuBarHeight }, GetColor(GuiCol_Border), g_Ctx.Style.MenuBarBorderThickness);
        }


        SetCursorScreenPos({ g_Ctx.WindowPos.x + g_Ctx.Style.WindowPadding.x + g_Ctx.IndentX, g_Ctx.WindowPos.y + titleBarHeight + menuBarHeight + g_Ctx.Style.WindowPadding.y });
        g_Ctx.ContentStartY = g_Ctx.Cursor.y;

        // 压入内容区专属剪裁区域，阻断控件滚动到标题栏上方
        PushClipRect(
            { g_Ctx.WindowPos.x, g_Ctx.ContentStartY },
            { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x, g_Ctx.WindowPos.y + g_Ctx.WindowSize.y }
        );

        SetCursorPosY(GetCursorPosY() - g_Ctx.ScrollY);

        return true;
    }

    inline void BeginTooltip() {
        g_Ctx.InTooltip = true;

        g_Ctx.BackupWindowPos = g_Ctx.WindowPos;
        g_Ctx.BackupWindowSize = g_Ctx.WindowSize;
        g_Ctx.BackupCursor = g_Ctx.Cursor;
        g_Ctx.BackupContentStartY = g_Ctx.ContentStartY;
        g_Ctx.BackupScrollY = g_Ctx.ScrollY;
        g_Ctx.BackupLastItemMaxX = g_Ctx.LastItemMaxX;
        g_Ctx.BackupIsScrollApplied = g_Ctx.IsScrollApplied;
        g_Ctx.BackupCurrentWindowFlags = g_Ctx.CurrentWindowFlags;
        g_Ctx.BackupIndentX = g_Ctx.IndentX;

        g_Ctx.BackupClippingEnabled = g_Ctx.ClippingEnabled;
        g_Ctx.BackupClipMin = g_Ctx.ClipMin;
        g_Ctx.BackupClipMax = g_Ctx.ClipMax;
        g_Ctx.BackupClipStack = g_Ctx.ClipStack;

        g_Ctx.ClippingEnabled = false;
        g_Ctx.ClipStack.clear();

        g_Ctx.WindowPos = { g_Ctx.MousePos.x + g_Ctx.Style.TooltipOffset, g_Ctx.MousePos.y + g_Ctx.Style.TooltipOffset };

        size_t tooltipKey = g_Ctx.HoveredIdCurrentFrame;
        Vec2 bgSize = g_Ctx.TooltipSizeCache[tooltipKey];
        if (bgSize.x < g_Ctx.Style.WindowScrollMinViewHeight) bgSize.x = g_Ctx.Style.TooltipMinSize;
        if (bgSize.y < g_Ctx.Style.WindowScrollMinViewHeight) bgSize.y = g_Ctx.Style.TooltipMinSize;

        GetWindowDrawList()->AddRect({ g_Ctx.WindowPos.x - g_Ctx.Style.PopupBorderInset, g_Ctx.WindowPos.y - g_Ctx.Style.PopupBorderInset }, { bgSize.x + g_Ctx.Style.PopupFillInset, bgSize.y + g_Ctx.Style.PopupFillInset }, GetColor(GuiCol_PopupBorder));
        GetWindowDrawList()->AddRectFilled(g_Ctx.WindowPos, bgSize, GetColor(GuiCol_PopupBg));

        g_Ctx.WindowSize = bgSize;
        g_Ctx.IndentX = 0.f;
        SetCursorScreenPos({ g_Ctx.WindowPos.x + g_Ctx.Style.WindowPadding.x + g_Ctx.IndentX, g_Ctx.WindowPos.y + g_Ctx.Style.WindowPadding.y });
        g_Ctx.ContentStartY = g_Ctx.Cursor.y;
        g_Ctx.ScrollY = 0.f;
        g_Ctx.IsScrollApplied = false;
        g_Ctx.CurrentWindowFlags = ShadowWindowFlags_NoResize | ShadowWindowFlags_NoMove | ShadowWindowFlags_NoScrollbar;

        PushClipRect(g_Ctx.WindowPos, { g_Ctx.WindowPos.x + bgSize.x, g_Ctx.WindowPos.y + bgSize.y });
        g_Ctx.CurrentTooltipSize = { 0.f, 0.f };
    }

    inline void EndTooltip() {
        PopClipRect();

        size_t tooltipKey = g_Ctx.HoveredIdCurrentFrame;
        g_Ctx.TooltipSizeCache[tooltipKey] = g_Ctx.CurrentTooltipSize;

        g_Ctx.WindowPos = g_Ctx.BackupWindowPos;
        g_Ctx.WindowSize = g_Ctx.BackupWindowSize;
        SetCursorScreenPos(g_Ctx.BackupCursor);
        g_Ctx.ContentStartY = g_Ctx.BackupContentStartY;
        g_Ctx.ScrollY = g_Ctx.BackupScrollY;
        g_Ctx.LastItemMaxX = g_Ctx.BackupLastItemMaxX;
        g_Ctx.IsScrollApplied = g_Ctx.BackupIsScrollApplied;
        g_Ctx.CurrentWindowFlags = g_Ctx.BackupCurrentWindowFlags;
        g_Ctx.IndentX = g_Ctx.BackupIndentX;

        g_Ctx.ClippingEnabled = g_Ctx.BackupClippingEnabled;
        g_Ctx.ClipMin = g_Ctx.BackupClipMin;
        g_Ctx.ClipMax = g_Ctx.BackupClipMax;
        g_Ctx.ClipStack = g_Ctx.BackupClipStack;

        g_Ctx.InTooltip = false;
    }

    inline void End() {
        g_Ctx.BeginStack--;

        float actualCursorY = g_Ctx.Cursor.y + (g_Ctx.IsScrollApplied ? g_Ctx.ScrollY : 0.f);
        g_Ctx.ContentHeight = actualCursorY - g_Ctx.ContentStartY;

        // 弹出内容区专用剪裁框，接着再弹出整个主窗口剪裁框
        PopClipRect();
        PopClipRect();

        bool noResize = (g_Ctx.CurrentWindowFlags & ShadowWindowFlags_NoResize) != 0;
        bool noScrollbar = (g_Ctx.CurrentWindowFlags & ShadowWindowFlags_NoScrollbar) != 0;

        float viewHeight = (g_Ctx.WindowPos.y + g_Ctx.WindowSize.y) - g_Ctx.ContentStartY - g_Ctx.Style.ResizeGripSize - g_Ctx.Style.WindowScrollBottomPadding;
        if (viewHeight < g_Ctx.Style.WindowScrollMinViewHeight) viewHeight = g_Ctx.Style.WindowScrollMinViewHeight;

        float maxScroll = GetScrollMaxY();

        g_Ctx.CurrentScrollbarWidth = 0.f;

        if (!noScrollbar && maxScroll > 0.f) {
            g_Ctx.CurrentScrollbarWidth = g_Ctx.Style.ScrollbarSize;

            float scrollbarWidth = g_Ctx.Style.ScrollbarSize;
            float scrollbarMarginRight = g_Ctx.Style.ScrollbarMargin;
            Vec2 trackPos = { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - scrollbarWidth - scrollbarMarginRight, g_Ctx.ContentStartY };
            Vec2 trackSize = { scrollbarWidth, viewHeight };

            GetWindowDrawList()->AddRectFilled(trackPos, trackSize, GetColor(GuiCol_FrameBg));

            float thumbHeight = std::max(g_Ctx.Style.ScrollbarThumbMinSize, (viewHeight / g_Ctx.ContentHeight) * trackSize.y);
            float thumbY = trackPos.y + (g_Ctx.ScrollY / maxScroll) * (trackSize.y - thumbHeight);
            Vec2 thumbPos = { trackPos.x, thumbY };
            Vec2 thumbSize = { scrollbarWidth, thumbHeight };

            bool hoveringThumb = IsMouseHoveringRaw(thumbPos, thumbSize);
            Color thumbColor = g_Ctx.IsDraggingScrollbar ? GetColor(GuiCol_SliderGrab) : (hoveringThumb ? GetColor(GuiCol_FrameBgHovered) : GetColor(GuiCol_Border));
            GetWindowDrawList()->AddRectFilled(thumbPos, thumbSize, thumbColor);
        }

        if (!noResize) {
            float triSize = g_Ctx.Style.ResizeGripSize;
            float x = g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - triSize;
            float y = g_Ctx.WindowPos.y + g_Ctx.WindowSize.y - triSize;

            Color triColor = g_Ctx.IsResizing
                ? GetColor(GuiCol_ResizeGripActive)
                : (g_Ctx.IsHoveringResize ? GetColor(GuiCol_ResizeGripHovered) : GetColor(GuiCol_ResizeGrip));

            float pad = g_Ctx.Style.ResizeGripPad;
            Vec2 p1 = { x + pad, y + triSize - pad };
            Vec2 p2 = { x + triSize - pad, y + triSize - pad };
            Vec2 p3 = { x + triSize - pad, y + pad };

            GetWindowDrawList()->AddTriangleFilled(p1, p2, p3, triColor);
        }

        if (!g_Ctx.MouseDown) g_Ctx.IsResizing = false;

        if (g_Ctx.CurrentWindow) {
            auto& win = *g_Ctx.CurrentWindow;
            win.Pos = g_Ctx.WindowPos;
            win.Size = g_Ctx.WindowSize;
            win.IsDragging = g_Ctx.IsDragging;
            win.DragOffset = g_Ctx.DragOffset;
            win.IsResizing = g_Ctx.IsResizing;
            win.ResizeStartPos = g_Ctx.ResizeStartPos;
            win.ResizeStartSize = g_Ctx.ResizeStartSize;
            win.IsHoveringResize = g_Ctx.IsHoveringResize;
            win.ScrollY = g_Ctx.ScrollY;
            win.ContentHeight = g_Ctx.ContentHeight;
            win.IsDraggingScrollbar = g_Ctx.IsDraggingScrollbar;
            win.ScrollDragOffset = g_Ctx.ScrollDragOffset;
            win.ContentStartY = g_Ctx.ContentStartY;
            win.IsScrollApplied = g_Ctx.IsScrollApplied;
            win.CurrentScrollbarWidth = g_Ctx.CurrentScrollbarWidth;
        }

        g_Ctx.CurrentWindow = nullptr;
    }

    inline bool BeginTabBar(std::string_view name, ShadowTabBarFlags flags = ShadowTabBarFlags_None) {
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.TabBarStack++;
        g_Ctx.CurrentTabBarFlags = flags;
        g_Ctx.CurrentTabBarId = id;

        if (!g_Ctx.MouseDown && g_Ctx.DraggingTabBarId == id) {
            g_Ctx.DraggingTabId = 0;
            g_Ctx.DraggingTabBarId = 0;
        }

        // 暂时撤销滚动：TabBar 作为固定在顶部的头组件，不应当跟随页面内容滚动
        SetCursorPosY(GetCursorPosY() + g_Ctx.ScrollY);

        g_Ctx.TabCursor = g_Ctx.Cursor;
        g_Ctx.TabBarOrigin = g_Ctx.Cursor;

        bool fittingScroll = (flags & ShadowTabBarFlags_FittingPolicyScroll) != 0;
        bool noScrollbar = (flags & ShadowTabBarFlags_NoScrollbar) != 0;

        float prevContentWidth = g_Ctx.TabBarContentWidth;
        float viewWidth = g_Ctx.WindowSize.x - (g_Ctx.Style.WindowPadding.x * 2.f);
        g_Ctx.TabBarViewWidth = viewWidth;
        bool needsScrollbar = fittingScroll && !noScrollbar && (prevContentWidth > viewWidth);
        g_Ctx.TabBarNeedsScrollbar = needsScrollbar;

        {
            auto& order = g_Ctx.TabOrderMap[id];
            auto& layout = g_Ctx.TabBarLayoutX[id];
            layout.clear();
            float accum = 0.f;
            for (size_t oid : order) {
                layout[oid] = accum;
                float w = 0.f;
                auto itw = g_Ctx.TabWidthCache.find(oid);
                if (itw != g_Ctx.TabWidthCache.end()) w = itw->second;
                accum += w + g_Ctx.Style.TabBarTabSpacing;
            }
        }

        g_Ctx.TabBarContentWidthAccum = 0.f;

        g_Ctx.TabBarDisplayCache[id].clear();
        g_Ctx.TabAppearedThisFrame[id].clear();

        SetCursorPosY(GetCursorPosY() + g_Ctx.ItemHeight + g_Ctx.Style.ItemSpacing.y);

        float scrollbarReserve = needsScrollbar ? (g_Ctx.Style.ScrollbarSize + g_Ctx.Style.TabBarScrollbarReserve) : 0.f;
        SetCursorPosY(GetCursorPosY() + scrollbarReserve);

        GetWindowDrawList()->AddRectFilled({ g_Ctx.WindowPos.x, g_Ctx.Cursor.y }, { g_Ctx.WindowSize.x, g_Ctx.Style.TabBarSeparatorHeight }, GetColor(GuiCol_Separator));
        SetCursorPosY(GetCursorPosY() + g_Ctx.Style.TabBarSeparatorHeight + g_Ctx.Style.WindowPadding.y);

        g_Ctx.ContentStartY = g_Ctx.Cursor.y;

        // 【新增】压入 TabBar 专属的内容剪裁框，阻断内部控件越界
        PushClipRect(
            { g_Ctx.WindowPos.x, g_Ctx.ContentStartY },
            { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x, g_Ctx.WindowPos.y + g_Ctx.WindowSize.y }
        );

        // 重新应用滚动，使 TabBar 内部的子控件开始滚动
        SetCursorPosY(GetCursorPosY() - g_Ctx.ScrollY);
        g_Ctx.IsScrollApplied = true;

        return true;
    }

    inline void EndTabBar() {
        size_t tabBarId = g_Ctx.CurrentTabBarId;
        bool fittingScroll = (g_Ctx.CurrentTabBarFlags & ShadowTabBarFlags_FittingPolicyScroll) != 0;
        bool reorderable = (g_Ctx.CurrentTabBarFlags & ShadowTabBarFlags_Reorderable) != 0;
        bool noScrollbar = (g_Ctx.CurrentTabBarFlags & ShadowTabBarFlags_NoScrollbar) != 0;

        // 【新增】在绘制 TabBar 自身的水平滚动条和拖拽头部前，先弹出它专属的内容剪裁框
        PopClipRect();

        g_Ctx.TabBarContentWidth = g_Ctx.TabBarContentWidthAccum;

        float viewWidth = g_Ctx.TabBarViewWidth;
        float maxScrollX = GetScrollMaxX();
        float& scrollX = g_Ctx.TabBarScrollX[tabBarId];
        scrollX = std::clamp(scrollX, 0.f, maxScrollX);

        Vec2 tabRowPos = g_Ctx.TabBarOrigin;
        Vec2 tabRowSize = { viewWidth, g_Ctx.ItemHeight };
        bool hoveringTabRow = IsMouseHovering(tabRowPos, tabRowSize);
        bool overListBox = (g_Ctx.HoveredListBoxIdPreviousFrame != 0);

        if (reorderable && g_Ctx.DraggingTabId != 0 && g_Ctx.DraggingTabBarId == tabBarId) {
            if (g_Ctx.MouseDown) {
                g_Ctx.DraggingTabCurrentX = g_Ctx.MousePos.x - g_Ctx.DraggingTabGrabOffsetX;

                auto& order = g_Ctx.TabOrderMap[tabBarId];
                auto& layout = g_Ctx.TabBarLayoutX[tabBarId];

                auto itSelf = std::find(order.begin(), order.end(), g_Ctx.DraggingTabId);
                if (itSelf != order.end()) {
                    size_t selfIdx = static_cast<size_t>(itSelf - order.begin());

                    float selfWidth = 0.f;
                    auto itw = g_Ctx.TabWidthCache.find(g_Ctx.DraggingTabId);
                    if (itw != g_Ctx.TabWidthCache.end()) selfWidth = itw->second;

                    float draggedLogicalX = (g_Ctx.DraggingTabCurrentX - g_Ctx.TabBarOrigin.x) + scrollX;
                    float draggedCenterX = draggedLogicalX + selfWidth * 0.5f;

                    if (selfIdx > 0) {
                        size_t leftId = order[selfIdx - 1];
                        auto itL = layout.find(leftId);
                        if (itL != layout.end()) {
                            float leftWidth = 0.f;
                            auto itlw = g_Ctx.TabWidthCache.find(leftId);
                            if (itlw != g_Ctx.TabWidthCache.end()) leftWidth = itlw->second;
                            float leftCenter = itL->second + leftWidth * 0.5f;
                            if (draggedCenterX < leftCenter) {
                                std::swap(order[selfIdx - 1], order[selfIdx]);
                            }
                        }
                    }

                    itSelf = std::find(order.begin(), order.end(), g_Ctx.DraggingTabId);
                    selfIdx = static_cast<size_t>(itSelf - order.begin());
                    if (selfIdx + 1 < order.size()) {
                        size_t rightId = order[selfIdx + 1];
                        auto itR = layout.find(rightId);
                        if (itR != layout.end()) {
                            float rightWidth = 0.f;
                            auto itrw = g_Ctx.TabWidthCache.find(rightId);
                            if (itrw != g_Ctx.TabWidthCache.end()) rightWidth = itrw->second;
                            float rightCenter = itR->second + rightWidth * 0.5f;
                            if (draggedCenterX > rightCenter) {
                                std::swap(order[selfIdx], order[selfIdx + 1]);
                            }
                        }
                    }
                }
            }
            else {
                g_Ctx.DraggingTabId = 0;
                g_Ctx.DraggingTabBarId = 0;
            }
        }

        if (reorderable && g_Ctx.DraggingTabId != 0 && g_Ctx.DraggingTabBarId == tabBarId) {
            for (const auto& tabInfo : g_Ctx.TabBarDisplayCache[tabBarId]) {
                if (tabInfo.id == g_Ctx.DraggingTabId) {
                    Vec2 clipMin = { g_Ctx.TabBarOrigin.x - 1.f, g_Ctx.TabBarOrigin.y - 1.f };
                    Vec2 clipMax = { g_Ctx.TabBarOrigin.x + g_Ctx.TabBarViewWidth + 1.f, g_Ctx.TabBarOrigin.y + tabInfo.size.y + 1.f };

                    Color bgColor = GetColor(GuiCol_TabActive);
                    Color textColor = GetColor(GuiCol_TextHighlight);

                    if (tabInfo.font) {
                        if (tabInfo.noSDF) PushFontNoSDF(tabInfo.font, tabInfo.fontScale);
                        else PushFont(tabInfo.font, tabInfo.fontScale);
                    }

                    PushClipRect(clipMin, clipMax);
                    GetWindowDrawList()->AddRectFilled(tabInfo.pos, tabInfo.size, bgColor);
                    GetWindowDrawList()->AddText({ tabInfo.pos.x + g_Ctx.Style.TabExtraWidth / 2.f, tabInfo.pos.y + g_Ctx.Style.FramePadding.y }, textColor, tabInfo.display);
                    PopClipRect();

                    if (tabInfo.font) { PopFont(); }
                    break;
                }
            }
        }

        float scrollBarTop = 0.f, scrollBarBottom = 0.f;
        if (fittingScroll && !noScrollbar && g_Ctx.TabBarNeedsScrollbar) {
            float barHeight = g_Ctx.Style.ScrollbarSize;
            Vec2 trackPos = { tabRowPos.x, tabRowPos.y + g_Ctx.ItemHeight + 2.f };
            Vec2 trackSize = { viewWidth, barHeight };

            GetWindowDrawList()->AddRectFilled(trackPos, trackSize, GetColor(GuiCol_FrameBg));

            float thumbWidth = std::max(g_Ctx.Style.ScrollbarThumbMinSize, (viewWidth / g_Ctx.TabBarContentWidth) * trackSize.x);
            float thumbX = trackPos.x + (maxScrollX > 0.f ? (scrollX / maxScrollX) * (trackSize.x - thumbWidth) : 0.f);
            Vec2 thumbPos = { thumbX, trackPos.y };
            Vec2 thumbSize = { thumbWidth, barHeight };

            bool hoveringThumb = IsMouseHoveringRaw(thumbPos, thumbSize);
            bool hoveringTrack = IsMouseHoveringRaw(trackPos, trackSize);

            if (!overListBox) {
                if (g_Ctx.MouseClicked && hoveringThumb) {
                    g_Ctx.DraggingTabBarScrollId = tabBarId;
                    g_Ctx.TabBarScrollDragOffset = g_Ctx.MousePos.x - thumbPos.x;
                    g_Ctx.MouseClicked = false;
                }
                else if (g_Ctx.MouseClicked && hoveringTrack) {
                    if (g_Ctx.MousePos.x < thumbPos.x) scrollX -= viewWidth * 0.5f;
                    else scrollX += viewWidth * 0.5f;
                    scrollX = std::clamp(scrollX, 0.f, maxScrollX);
                    g_Ctx.MouseClicked = false;
                }
            }

            if (g_Ctx.DraggingTabBarScrollId == tabBarId) {
                if (g_Ctx.MouseDown) {
                    float newThumbX = g_Ctx.MousePos.x - g_Ctx.TabBarScrollDragOffset;
                    float ratio = (newThumbX - trackPos.x) / std::max(1.f, trackSize.x - thumbWidth);
                    scrollX = std::clamp(ratio * maxScrollX, 0.f, maxScrollX);
                }
                else {
                    g_Ctx.DraggingTabBarScrollId = 0;
                }
            }

            Color thumbColor = (g_Ctx.DraggingTabBarScrollId == tabBarId)
                ? GetColor(GuiCol_SliderGrab)
                : (hoveringThumb ? GetColor(GuiCol_FrameBgHovered) : GetColor(GuiCol_Border));
            GetWindowDrawList()->AddRectFilled(thumbPos, thumbSize, thumbColor);

            bool hoveringForWheel = hoveringTabRow || hoveringTrack;

            if (hoveringForWheel && g_Ctx.MouseWheel != 0.f && !overListBox) {
                scrollX -= g_Ctx.MouseWheel * g_Ctx.Style.TabBarScrollSpeed;
                scrollX = std::clamp(scrollX, 0.f, maxScrollX);
                g_Ctx.MouseWheel = 0.f;
            }

            scrollBarTop = trackPos.y;
            scrollBarBottom = trackPos.y + trackSize.y;
        }
        else if (fittingScroll) {
            if (hoveringTabRow && g_Ctx.MouseWheel != 0.f && !overListBox) {
                scrollX -= g_Ctx.MouseWheel * g_Ctx.Style.TabBarScrollSpeed;
                scrollX = std::clamp(scrollX, 0.f, maxScrollX);
                g_Ctx.MouseWheel = 0.f;
            }
            g_Ctx.DraggingTabBarScrollId = 0;
        }

        float unionTop = tabRowPos.y;
        float unionBottom = (scrollBarBottom > 0) ? scrollBarBottom : (tabRowPos.y + g_Ctx.ItemHeight);
        Vec2 fullTabBarRectPos = { tabRowPos.x, unionTop };
        Vec2 fullTabBarRectSize = { viewWidth, unionBottom - unionTop };
        g_Ctx.TabBarHoverRectsPending.push_back({ fullTabBarRectPos, fullTabBarRectSize });

        g_Ctx.TabBarStack--;
    }

    inline bool BeginTabItem(std::string_view name) {
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.TabItemStack++;

        size_t tabBarId = g_Ctx.CurrentTabBarId;
        bool reorderable = (g_Ctx.CurrentTabBarFlags & ShadowTabBarFlags_Reorderable) != 0;
        bool fittingScroll = (g_Ctx.CurrentTabBarFlags & ShadowTabBarFlags_FittingPolicyScroll) != 0;

        auto& order = g_Ctx.TabOrderMap[tabBarId];
        if (std::find(order.begin(), order.end(), id) == order.end()) {
            order.push_back(id);
        }
        g_Ctx.TabAppearedThisFrame[tabBarId].push_back(id);

        Vec2 tabSize = MeasureTextSize(display);
        tabSize.x += g_Ctx.Style.TabExtraWidth; tabSize.y = g_Ctx.ItemHeight;

        g_Ctx.TabWidthCache[id] = tabSize.x;

        float offsetX;
        auto& layout = g_Ctx.TabBarLayoutX[tabBarId];
        auto itLayout = layout.find(id);
        if (itLayout != layout.end()) {
            offsetX = itLayout->second;
        }
        else {
            float maxEnd = 0.f;
            for (auto& [oid, x] : layout) {
                float w = 0.f;
                auto itw = g_Ctx.TabWidthCache.find(oid);
                if (itw != g_Ctx.TabWidthCache.end()) w = itw->second;
                maxEnd = std::max(maxEnd, x + w + 5.f);
            }
            offsetX = maxEnd;
            layout[id] = offsetX;
        }

        float scrollX = fittingScroll ? g_Ctx.TabBarScrollX[tabBarId] : 0.f;

        bool isDraggingSelf = reorderable && g_Ctx.DraggingTabId == id && g_Ctx.DraggingTabBarId == tabBarId;

        Vec2 tabPos;
        if (isDraggingSelf) {
            tabPos = { g_Ctx.DraggingTabCurrentX, g_Ctx.TabBarOrigin.y };
        }
        else {
            tabPos = { g_Ctx.TabBarOrigin.x + offsetX - scrollX, g_Ctx.TabBarOrigin.y };
        }

        // 【新增】暂时弹出 BeginTabBar 压入的专属内容剪裁框，回到不受限的层级以便绘制 Header
        PopClipRect();

        SDK::UFont* currentFont = g_Ctx.DefaultFont;
        float currentScale = 1.0f;
        bool currentNoSDF = false;
        if (!g_Ctx.FontStack.empty()) {
            currentFont = g_Ctx.FontStack.back().Font;
            currentScale = g_Ctx.FontStack.back().Scale;
            currentNoSDF = g_Ctx.FontStack.back().NoSDF;
        }

        g_Ctx.TabBarDisplayCache[tabBarId].push_back({ id, tabPos, tabSize, std::string(display), currentFont, currentScale, currentNoSDF });

        g_Ctx.TabBarContentWidthAccum += tabSize.x + g_Ctx.Style.TabBarTabSpacing;

        Vec2 clipMin = { g_Ctx.TabBarOrigin.x - 1.f, g_Ctx.TabBarOrigin.y - 1.f };
        Vec2 clipMax = { g_Ctx.TabBarOrigin.x + g_Ctx.TabBarViewWidth + 1.f, g_Ctx.TabBarOrigin.y + tabSize.y + 1.f };
        bool tabVisible = !(tabPos.x + tabSize.x < g_Ctx.TabBarOrigin.x || tabPos.x > g_Ctx.TabBarOrigin.x + g_Ctx.TabBarViewWidth);

        bool hovered = tabVisible && IsMouseHovering(tabPos, tabSize) && IsRectVisible(tabPos, tabSize);

        size_t& currentActiveTabId = g_Ctx.ActiveTabIdMap[tabBarId];

        if (hovered && g_Ctx.MouseClicked) {
            if (currentActiveTabId != id) {
                g_Ctx.FocusedSliderId = 0;
                g_Ctx.ActiveInputId = 0;
                g_Ctx.ScrollY = 0.f;
            }
            currentActiveTabId = id;

            if (reorderable) {
                g_Ctx.DraggingTabId = id;
                g_Ctx.DraggingTabBarId = tabBarId;
                g_Ctx.DraggingTabGrabOffsetX = g_Ctx.MousePos.x - tabPos.x;
                g_Ctx.DraggingTabCurrentX = tabPos.x;
                isDraggingSelf = true;
            }
            g_Ctx.MouseClicked = false;
        }

        if (currentActiveTabId == 0) currentActiveTabId = id;

        bool isActive = (currentActiveTabId == id);
        g_Ctx.InActiveTab = isActive;

        if (!isDraggingSelf) {
            Color bgColor = isActive ? GetColor(GuiCol_TabActive) : (hovered ? GetColor(GuiCol_TabHovered) : GetColor(GuiCol_Tab));
            Color textColor = isActive ? GetColor(GuiCol_TextHighlight) : GetColor(GuiCol_TextDisabled);

            if (tabVisible) {
                PushClipRect(clipMin, clipMax);
                GetWindowDrawList()->AddRectFilled(tabPos, tabSize, bgColor);
                GetWindowDrawList()->AddText({ tabPos.x + g_Ctx.Style.TabExtraWidth / 2.f, tabPos.y + g_Ctx.Style.FramePadding.y }, textColor, display);
                PopClipRect();
            }
        }

        // 【新增】Header 画完了，恢复压入 BeginTabBar 的内容剪裁框
        PushClipRect(
            { g_Ctx.WindowPos.x, g_Ctx.ContentStartY },
            { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x, g_Ctx.WindowPos.y + g_Ctx.WindowSize.y }
        );

        if (isActive) {
            float activeScrollbarWidth = g_Ctx.CurrentScrollbarWidth;

            PushClipRect(
                { g_Ctx.WindowPos.x, g_Ctx.ContentStartY },
                { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - activeScrollbarWidth, g_Ctx.WindowPos.y + g_Ctx.WindowSize.y - g_Ctx.Style.ResizeGripSize }
            );
        }

        return isActive;
    }

    inline void EndTabItem() {
        if (g_Ctx.InActiveTab) {
            PopClipRect(); // 当前页结束，弹出页面内部专用的剪裁区域
        }
        g_Ctx.TabItemStack--;
        g_Ctx.InActiveTab = true;
    }

    inline void Render() {
        CheckAndDrawErrors();

        auto ExecCmds = [](const std::vector<ShadowDrawCmd>& cmds) {
            for (const auto& cmd : cmds) {
                switch (cmd.type) {
                case ShadowDrawCmdType::Line:
                    InternalDrawLine(cmd.pos, cmd.size, cmd.color, cmd.thickness, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, cmd.texture);
                    break;
                case ShadowDrawCmdType::Rect:
                    InternalDrawRect(cmd.pos, cmd.size, cmd.color, cmd.thickness, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, cmd.texture);
                    break;
                case ShadowDrawCmdType::RectFilled:
                case ShadowDrawCmdType::Texture:
                    InternalDrawRectFilled(cmd.pos, cmd.size, cmd.color, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, cmd.texture);
                    break;
                case ShadowDrawCmdType::Text:
                    InternalDrawText(cmd.text, cmd.pos, cmd.color, cmd.font, cmd.fontScale, cmd.textShadowColor, cmd.textOutlineColor, cmd.textOutline, cmd.noSDF);
                    break;
                case ShadowDrawCmdType::TriangleFilled:
                    InternalDrawTriangleFilled(cmd.p1, cmd.p2, cmd.p3, cmd.color, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, cmd.texture);
                    break;
                case ShadowDrawCmdType::Triangle:
                    InternalDrawTriangle(cmd.p1, cmd.p2, cmd.p3, cmd.color, cmd.thickness, cmd.clippingEnabled, cmd.clipMin, cmd.clipMax, cmd.texture);
                    break;
                }
            }
            };

        ExecCmds(g_Ctx.BackgroundDrawList.CmdBuffer);

        for (size_t id : g_Ctx.WindowDisplayOrder) {
            auto it = g_Ctx.Windows.find(id);
            if (it != g_Ctx.Windows.end()) {
                ExecCmds(it->second.DrawList.CmdBuffer);
            }
        }

        // 依序迭代所有已被激活打开的弹窗，越深的层级越晚渲染，从而永远叠在最上层
        for (size_t id : g_Ctx.ActivePopups) {
            auto it = g_Ctx.Windows.find(id);
            if (it != g_Ctx.Windows.end()) {
                ExecCmds(it->second.DrawList.CmdBuffer);
            }
        }

        ExecCmds(g_Ctx.TooltipDrawList.CmdBuffer);
        ExecCmds(g_Ctx.ForegroundDrawList.CmdBuffer);

        g_Ctx.MouseClicked = false;
        g_Ctx.RightMouseClicked = false;
        g_Ctx.MiddleMouseClicked = false;
        memset(g_Ctx.KeyPressed, 0, sizeof(g_Ctx.KeyPressed));
        g_Ctx.InputChars.clear();
        g_Ctx.MouseWheel = 0.f;
    }

    inline bool Combo(std::string_view name, int* current_item, const std::vector<std::string>& items, ShadowComboFlags flags = ShadowComboFlags_None, Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return false;
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;
        PushID(name);

        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;
        bool disabled = IsDisabled();
        bool noText = (flags & ShadowComboFlags_NoText) != 0;
        bool noRightAlign = (flags & ShadowComboFlags_NoRightAlign) != 0;
        bool fitText = (flags & ShadowComboFlags_FitText) != 0;

        float textWidth = 0.f;
        if (!noText) {
            textWidth = MeasureTextSize(display).x;
        }

        float controlOffsetX = GetControlOffsetX();
        float rightMargin = GetRightMargin();

        std::string currentText = (*current_item >= 0 && *current_item < static_cast<int>(items.size())) ? items[*current_item] : "Unknown";
        float triSize = itemHeight * g_Ctx.Style.ComboArrowSizeRatio;

        float boxWidth;
        if (size_arg.x > 0.f) {
            boxWidth = size_arg.x;
        }
        else {
            if (fitText) {
                boxWidth = g_Ctx.Style.FramePadding.x + MeasureTextSize(currentText).x + g_Ctx.Style.FramePadding.x + triSize + g_Ctx.Style.FramePadding.x;
            }
            else {
                if (noRightAlign) {
                    float startX = g_Ctx.Cursor.x + (noText ? 0.f : textWidth + g_Ctx.Style.LabelSpacing);
                    boxWidth = std::max(g_Ctx.Style.ComboMinWidthNoRightAlign, g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - rightMargin - startX);
                }
                else {
                    boxWidth = std::max(g_Ctx.Style.ComboMinWidth, g_Ctx.WindowSize.x - controlOffsetX - rightMargin);
                }
            }
        }

        Vec2 pos = g_Ctx.Cursor;
        Vec2 boxPos;
        if (noRightAlign) {
            boxPos = { pos.x + (noText ? 0.f : textWidth + g_Ctx.Style.LabelSpacing), pos.y };
        }
        else {
            if (fitText) {
                boxPos = { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - rightMargin - boxWidth, pos.y };
            }
            else {
                boxPos = { g_Ctx.WindowPos.x + controlOffsetX, pos.y };
            }
        }

        Vec2 boxSize = { boxWidth, itemHeight };
        Vec2 min = { std::min(pos.x, boxPos.x), pos.y };
        Vec2 max = { boxPos.x + boxSize.x, pos.y + itemHeight };

        ItemSize(min, max);
        if (!ItemAdd(min, max, id, disabled)) {
            PopID();
            return false;
        }

        Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);

        if (!noText) {
            GetWindowDrawList()->AddText({ pos.x, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, display);
        }

        bool hovered = !disabled && IsMouseHovering(boxPos, boxSize);
        if (hovered && g_Ctx.MouseDown) {
            g_Ctx.ActiveId = id;
        }
        bool toggled = false;

        if (hovered && g_Ctx.MouseClicked) {
            g_Ctx.IsDragging = false;

            if (IsPopupOpen("##ComboPopup")) {
                CloseCurrentPopup();
            }
            else {
                OpenPopup("##ComboPopup");
                SetNextWindowPos({ boxPos.x, boxPos.y + boxSize.y });
                SetNextWindowSize({ boxWidth, items.size() * g_Ctx.ItemHeight });
            }
            g_Ctx.MouseClicked = false;
            toggled = true;
        }

        Color bgColor = disabled ? GetColor(GuiCol_ControlDisabled) : (hovered ? GetColor(GuiCol_FrameBgHovered) : GetColor(GuiCol_FrameBg));
        GetWindowDrawList()->AddRectFilled(boxPos, boxSize, bgColor);
        GetWindowDrawList()->AddText({ boxPos.x + g_Ctx.Style.FramePadding.x, boxPos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, currentText);

        Vec2 triPos = { boxPos.x + boxSize.x - g_Ctx.Style.FramePadding.x - triSize, boxPos.y + boxSize.y / 2.f - triSize / 2.f };
        if (IsRectVisible(triPos, { triSize, triSize })) {
            Color triCol = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
            Vec2 p1 = { triPos.x, triPos.y + triSize * 0.25f };
            Vec2 p2 = { triPos.x + triSize, triPos.y + triSize * 0.25f };
            Vec2 p3 = { triPos.x + triSize * 0.5f, triPos.y + triSize * 0.75f };
            GetWindowDrawList()->AddTriangleFilled(p1, p2, p3, triCol);
        }

        PushStyleVar(GuiStyleVar_WindowPadding, { 0.f, 0.f });

        if (BeginPopup("##ComboPopup", ShadowWindowFlags_NoMove)) {
            for (size_t i = 0; i < items.size(); ++i) {
                Vec2 itemPos = g_Ctx.Cursor;
                bool itemHovered = IsMouseHoveringRaw(itemPos, { boxWidth, g_Ctx.ItemHeight });
                bool isCurrentItem = (*current_item == static_cast<int>(i));

                if (itemHovered) {
                    GetWindowDrawList()->AddRectFilled(itemPos, { boxWidth, g_Ctx.ItemHeight }, GetColor(GuiCol_FrameBgHovered));
                    if (g_Ctx.MouseClicked) {
                        *current_item = static_cast<int>(i);
                        CloseCurrentPopup();
                        g_Ctx.MouseClicked = false;
                        toggled = true;
                    }
                }
                else if (isCurrentItem) {
                    GetWindowDrawList()->AddRectFilled(itemPos, { boxWidth, g_Ctx.ItemHeight }, GetColor(GuiCol_DropdownActive));
                }

                Color textCol = isCurrentItem ? GetColor(GuiCol_TextHighlight) : GetColor(GuiCol_Text);
                GetWindowDrawList()->AddText({ itemPos.x + g_Ctx.Style.FramePadding.x, itemPos.y + g_Ctx.Style.FramePadding.y }, textCol, items[i]);

                SetCursorPosY(GetCursorPosY() + g_Ctx.ItemHeight);
            }
            g_Ctx.LastItemMaxX = g_Ctx.WindowPos.x + boxWidth;
        }
        EndPopup();
        PopStyleVar();

        PopID();
        return toggled;
    }

    inline bool Checkbox(std::string_view name, bool* value, Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return false;
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;

        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;
        Vec2 boxSize = { size_arg.x > 0.f ? size_arg.x : itemHeight, itemHeight };
        float textWidth = MeasureTextSize(display).x;
        Vec2 interactSize = { boxSize.x + g_Ctx.Style.LabelSpacing + textWidth, itemHeight };

        bool disabled = IsDisabled();
        Vec2 pos = g_Ctx.Cursor;
        Vec2 min = pos;
        Vec2 max = { pos.x + interactSize.x, pos.y + itemHeight };

        ItemSize(min, max);
        if (!ItemAdd(min, max, id, disabled)) {
            return false;
        }

        bool hovered = !disabled && IsMouseHovering(pos, interactSize);

        if (hovered && g_Ctx.MouseDown) {
            g_Ctx.ActiveId = id;
        }

        if (hovered && g_Ctx.MouseClicked) {
            *value = !(*value);
        }

        Color bgColor = disabled ? GetColor(GuiCol_ControlDisabled) : (hovered ? GetColor(GuiCol_FrameBgHovered) : GetColor(GuiCol_FrameBg));
        GetWindowDrawList()->AddRectFilled(pos, boxSize, bgColor);

        if (*value) {
            float checkPad = boxSize.x * g_Ctx.Style.CheckboxCheckPaddingRatio;
            Color checkCol = GetColor(GuiCol_CheckMark);
            GetWindowDrawList()->AddRectFilled({ pos.x + checkPad, pos.y + checkPad }, { boxSize.x - checkPad * 2.f, boxSize.y - checkPad * 2.f }, checkCol);
        }

        Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
        GetWindowDrawList()->AddText({ pos.x + boxSize.x + g_Ctx.Style.LabelSpacing, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, display);

        return *value;
    }

    inline bool Switch(std::string_view name, bool* value, Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return false;
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;

        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;
        float padding = g_Ctx.Style.SwitchPadding;
        float knobSize = itemHeight - padding * 2.f;
        float width = size_arg.x > 0.f ? size_arg.x : (knobSize * 2.f + padding * 2.f);
        Vec2 boxSize = { width, itemHeight };
        float textWidth = MeasureTextSize(display).x;
        Vec2 interactSize = { textWidth + g_Ctx.Style.LabelSpacing + boxSize.x, itemHeight };

        bool disabled = IsDisabled();
        Vec2 pos = g_Ctx.Cursor;
        Vec2 min = pos;
        Vec2 max = { pos.x + interactSize.x, pos.y + itemHeight };

        ItemSize(min, max);
        if (!ItemAdd(min, max, id, disabled)) {
            return false;
        }

        bool hovered = !disabled && IsMouseHovering(pos, interactSize);
        if (hovered && g_Ctx.MouseDown) { g_Ctx.ActiveId = id; }
        if (hovered && g_Ctx.MouseClicked) { *value = !(*value); }

        Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
        GetWindowDrawList()->AddText({ pos.x, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, display);

        Vec2 boxPos = { pos.x + textWidth + g_Ctx.Style.LabelSpacing, pos.y };

        Color bgColor;
        if (disabled) {
            bgColor = GetColor(GuiCol_ControlDisabled);
        }
        else {
            if (*value) {
                bgColor = hovered ? GetColor(GuiCol_SwitchBgActiveHovered) : GetColor(GuiCol_SwitchBgActive);
            }
            else {
                bgColor = hovered ? GetColor(GuiCol_SwitchBgHovered) : GetColor(GuiCol_SwitchBg);
            }
        }

        GetWindowDrawList()->AddRectFilled(boxPos, boxSize, bgColor);

        Color knobColor = GetColor(GuiCol_SwitchKnob);

        float knobX = *value ? (boxPos.x + width - padding - knobSize) : (boxPos.x + padding);
        GetWindowDrawList()->AddRectFilled({ knobX, boxPos.y + padding }, { knobSize, knobSize }, knobColor);

        return *value;
    }

    inline bool Selectable(std::string_view name, bool* p_selected = nullptr, Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return false;
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;

        float rightMargin = GetRightMargin();
        float width = size_arg.x > 0.f ? size_arg.x : std::max(10.f, g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - rightMargin - g_Ctx.Cursor.x);
        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;
        Vec2 size = { width, itemHeight };

        bool disabled = IsDisabled();
        Vec2 pos = g_Ctx.Cursor;
        Vec2 min = pos;
        Vec2 max = { pos.x + size.x, pos.y + size.y };

        ItemSize(min, max);
        if (!ItemAdd(min, max, id, disabled)) {
            return false;
        }

        bool hovered = !disabled && IsMouseHovering(pos, size);
        if (hovered && g_Ctx.MouseDown) { g_Ctx.ActiveId = id; }
        bool clicked = hovered && g_Ctx.MouseClicked;

        if (clicked && p_selected) {
            *p_selected = !*p_selected;
        }

        bool selected = p_selected ? *p_selected : false;

        Color bgColor = GetColor(GuiCol_Transparent);
        if (disabled) {
            if (selected) bgColor = GetColor(GuiCol_ControlDisabled);
        }
        else {
            if (hovered && selected) bgColor = GetColor(GuiCol_TabHovered);
            else if (hovered) bgColor = GetColor(GuiCol_FrameBgHovered);
            else if (selected) bgColor = GetColor(GuiCol_TabActive);
        }

        if (bgColor.a > 0.0f) {
            if (g_Ctx.InPopup && !g_Ctx.PopupStack.empty() && size_arg.x <= 0.f) {
                g_Ctx.PopupStack.back().RightAlignCmds.push_back({ GetWindowDrawList()->GetCmdBuffer().size(), RightAlignCmdType::RectBackground });
            }
            GetWindowDrawList()->AddRectFilled(pos, size, bgColor);
        }

        Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
        GetWindowDrawList()->AddText({ pos.x + g_Ctx.Style.FramePadding.x, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, display);

        return clicked;
    }

    inline bool Selectable(std::string_view name, bool selected, Vec2 size_arg = { 0.f, 0.f }) {
        bool temp = selected;
        return Selectable(name, &temp, size_arg);
    }

    inline bool InputTextWithHint(std::string_view name, std::string_view hint, std::string& text, ShadowInputTextFlags flags = 0, Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return false;
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;

        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;
        bool disabled = IsDisabled();
        bool noName = (flags & ShadowInputTextFlags_NoName) != 0;

        float controlOffsetX = GetControlOffsetX();
        float rightMargin = GetRightMargin();

        Vec2 pos = g_Ctx.Cursor;
        Vec2 boxPos;
        float boxWidth;

        if (noName) {
            boxPos = pos;
            boxWidth = std::max(g_Ctx.Style.InputTextMinWidth, g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - rightMargin - boxPos.x);
        }
        else {
            boxPos = { g_Ctx.WindowPos.x + controlOffsetX, pos.y };
            boxWidth = std::max(g_Ctx.Style.InputTextMinWidth, g_Ctx.WindowSize.x - controlOffsetX - rightMargin);
        }

        if (size_arg.x > 0.f) boxWidth = size_arg.x;
        Vec2 boxSize = { boxWidth, itemHeight };
        Vec2 min = { std::min(pos.x, boxPos.x), pos.y };
        Vec2 max = { boxPos.x + boxSize.x, pos.y + itemHeight };

        ItemSize(min, max);
        if (!ItemAdd(min, max, id, disabled)) {
            return false;
        }

        if (!noName) {
            Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
            GetWindowDrawList()->AddText({ pos.x, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, display);
        }

        bool changed = InputTextEx(id, boxPos, boxSize, text, flags, false, hint);
        return changed;
    }

    inline bool InputText(std::string_view name, std::string& text, ShadowInputTextFlags flags = 0, Vec2 size_arg = { 0.f, 0.f }) {
        return InputTextWithHint(name, "", text, flags, size_arg);
    }

    inline bool InputFloat(std::string_view name, float* v, float step = 0.0f, float step_fast = 0.0f, std::string_view format = "{:.3f}", ShadowInputTextFlags flags = 0, Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return false;
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;
        PushID(name);

        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;
        bool disabled = IsDisabled();
        bool noName = (flags & ShadowInputTextFlags_NoName) != 0;

        float controlOffsetX = GetControlOffsetX();
        float rightMargin = GetRightMargin();

        Vec2 pos = g_Ctx.Cursor;
        Vec2 boxPos;
        float boxWidth;

        if (noName) {
            boxPos = pos;
            boxWidth = std::max(g_Ctx.Style.InputTextMinWidth, g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - rightMargin - boxPos.x);
        }
        else {
            boxPos = { g_Ctx.WindowPos.x + controlOffsetX, pos.y };
            boxWidth = std::max(g_Ctx.Style.InputTextMinWidth, g_Ctx.WindowSize.x - controlOffsetX - rightMargin);
        }

        if (size_arg.x > 0.f) boxWidth = size_arg.x;
        Vec2 boxSize = { boxWidth, itemHeight };
        Vec2 min = { std::min(pos.x, boxPos.x), pos.y };
        Vec2 max = { boxPos.x + boxSize.x, pos.y + itemHeight };

        ItemSize(min, max);
        if (!ItemAdd(min, max, id, disabled)) {
            PopID();
            return false;
        }

        if (!noName) {
            Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
            GetWindowDrawList()->AddText({ pos.x, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, display);
        }

        size_t inputId = GetID("##InputFloatText");

        if (g_Ctx.ActiveInputId != inputId) {
            if ((flags & ShadowInputTextFlags_DisplayEmptyRefVal) && std::abs(*v) < g_Ctx.Style.InputFloatEmptyThreshold) {
                g_Ctx.InputBuffers[inputId] = "";
            }
            else {
                g_Ctx.InputBuffers[inputId] = std::vformat(format, std::make_format_args(*v));
            }
        }

        ShadowInputTextFlags effectiveFlags = flags | ShadowInputTextFlags_CharsScientific;

        bool changed = InputTextEx(inputId, boxPos, boxSize, g_Ctx.InputBuffers[inputId], effectiveFlags);
        bool valueChanged = false;

        if (changed && !disabled) {
            std::string& str = g_Ctx.InputBuffers[inputId];
            if (str.empty() && (flags & ShadowInputTextFlags_ParseEmptyRefVal)) {
                *v = 0.0f;
                valueChanged = true;
            }
            else if (!str.empty()) {
                float parsed;
                auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), parsed);
                if (ec == std::errc()) {
                    *v = parsed;
                    valueChanged = true;
                }
            }
        }

        if (g_Ctx.ActiveInputId == inputId) {
            g_Ctx.ActiveId = id;
        }

        PopID();
        return valueChanged;
    }

    inline bool Button(std::string_view name, Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return false;
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;

        Vec2 size = MeasureTextSize(display);
        size.x += g_Ctx.Style.FramePadding.x * 2.f;
        size.y = g_Ctx.ItemHeight;

        if (size_arg.x > 0.f) size.x = size_arg.x;
        if (size_arg.y > 0.f) size.y = size_arg.y;

        bool disabled = IsDisabled();
        Vec2 pos = g_Ctx.Cursor;
        Vec2 min = pos;
        Vec2 max = { pos.x + size.x, pos.y + size.y };

        ItemSize(min, max);
        if (!ItemAdd(min, max, id, disabled)) {
            return false;
        }

        bool hovered = !disabled && IsMouseHovering(pos, size);
        bool clicked = hovered && g_Ctx.MouseClicked;

        if (hovered && g_Ctx.MouseDown) {
            g_Ctx.ActiveId = id;
        }

        Color bgColor = disabled ? GetColor(GuiCol_ControlDisabled) : (hovered ? GetColor(GuiCol_ButtonHovered) : GetColor(GuiCol_Button));
        Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);

        GetWindowDrawList()->AddRectFilled(pos, size, bgColor);
        GetWindowDrawList()->AddText({ pos.x + g_Ctx.Style.FramePadding.x, pos.y + g_Ctx.Style.FramePadding.y + (size.y - g_Ctx.ItemHeight) * 0.5f }, textColor, display);

        return clicked;
    }

    inline void Image(SDK::UTexture* texture, Vec2 size, Color color = { 1.f, 1.f, 1.f, 1.f }) {
        if (!g_Ctx.InActiveTab) return;
        g_Ctx.WidgetCount++;

        bool disabled = IsDisabled();
        Vec2 pos = g_Ctx.Cursor;
        Vec2 min = pos;
        Vec2 max = { pos.x + size.x, pos.y + size.y };

        ItemSize(min, max);
        if (!ItemAdd(min, max, ++g_Ctx.WidgetCount, disabled)) {
            return;
        }

        Color drawColor = GetColor(color);

        GetWindowDrawList()->AddTexture(pos, size, drawColor, texture);
    }

    inline bool ImageButton(std::string_view name, SDK::UTexture* texture, Vec2 size) {
        if (!g_Ctx.InActiveTab) return false;
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;

        Vec2 btnSize = { size.x > 0.f ? size.x : g_Ctx.ItemHeight, size.y > 0.f ? size.y : g_Ctx.ItemHeight };
        bool disabled = IsDisabled();
        Vec2 pos = g_Ctx.Cursor;
        Vec2 min = pos;
        Vec2 max = { pos.x + btnSize.x, pos.y + btnSize.y };

        ItemSize(min, max);
        if (!ItemAdd(min, max, id, disabled)) {
            return false;
        }

        bool hovered = !disabled && IsMouseHovering(pos, btnSize);
        bool clicked = hovered && g_Ctx.MouseClicked;

        if (hovered && g_Ctx.MouseDown) {
            g_Ctx.ActiveId = id;
        }

        Color bgColor = disabled ? GetColor(GuiCol_ControlDisabled) : (hovered ? GetColor(GuiCol_ButtonHovered) : GetColor(GuiCol_Button));
        Color tintColor = GetColor({ 1.f, 1.f, 1.f, 1.f });

        GetWindowDrawList()->AddRectFilled(pos, btnSize, bgColor);
        GetWindowDrawList()->AddTexture(pos, btnSize, tintColor, texture);

        return clicked;
    }

    inline void Slider(std::string_view name, float* value, float min_val, float max_val, float step = 0.f, ShadowSliderFlags flags = ShadowSliderFlags_None, Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return;
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;
        PushID(name);

        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;
        bool disabled = IsDisabled();
        bool noText = (flags & ShadowSliderFlags_NoText) != 0;
        bool noRightAlign = (flags & ShadowSliderFlags_NoRightAlign) != 0;

        float textWidth = 0.f;
        if (!noText) {
            textWidth = MeasureTextSize(display).x;
        }

        size_t sliderInputId = GetID("##SliderInput");

        int prec = g_Ctx.Style.SliderDefaultPrecision;
        if (step > 0.f) {
            prec = 0;
            float temp = step;
            while (temp < 0.999f && prec < 5) {
                temp *= 10.0f;
                prec++;
            }
        }

        if (g_Ctx.ActiveInputId != sliderInputId) {
            g_Ctx.InputBuffers[sliderInputId] = std::format("{:.{}f}", *value, prec);
        }

        auto it_width = g_Ctx.SliderInputWidthCache.find(sliderInputId);
        if (it_width == g_Ctx.SliderInputWidthCache.end()) {
            std::string minStr = std::format("{:.{}f}", min_val, prec);
            std::string maxStr = std::format("{:.{}f}", max_val, prec);
            float wMin = MeasureTextSize(minStr).x;
            float wMax = MeasureTextSize(maxStr).x;
            g_Ctx.SliderInputWidthCache[sliderInputId] = std::max(wMin, wMax) + g_Ctx.Style.FramePadding.x * 2.f + g_Ctx.Style.SliderInputExtraWidth;
        }
        float valBoxWidth = g_Ctx.SliderInputWidthCache[sliderInputId];

        float controlOffsetX = GetControlOffsetX();
        float rightMargin = GetRightMargin();

        Vec2 pos = g_Ctx.Cursor;
        Vec2 sliderPos;
        float sliderWidth;

        if (noRightAlign) {
            sliderPos = { pos.x + (noText ? 0.f : textWidth + 10.f), pos.y };
            sliderWidth = std::max(g_Ctx.Style.SliderMinWidth, g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - rightMargin - sliderPos.x - valBoxWidth - g_Ctx.Style.ItemSpacing.x);
        }
        else {
            sliderPos = { g_Ctx.WindowPos.x + controlOffsetX, pos.y };
            sliderWidth = std::max(g_Ctx.Style.SliderMinWidth, g_Ctx.WindowSize.x - controlOffsetX - valBoxWidth - g_Ctx.Style.ItemSpacing.x - rightMargin);
        }

        if (size_arg.x > 0.f) sliderWidth = size_arg.x;

        Vec2 size = { sliderWidth, itemHeight };
        Vec2 valBoxPos = { sliderPos.x + sliderWidth + g_Ctx.Style.ItemSpacing.x, sliderPos.y };
        Vec2 valBoxSize = { valBoxWidth, itemHeight };
        Vec2 min = { std::min(pos.x, sliderPos.x), pos.y };
        Vec2 max = { valBoxPos.x + valBoxSize.x, pos.y + itemHeight };

        ItemSize(min, max);
        if (!ItemAdd(min, max, id, disabled)) {
            PopID();
            return;
        }

        Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
        if (!noText) {
            GetWindowDrawList()->AddText({ pos.x, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, display);
        }

        bool hovered = !disabled && IsMouseHovering(sliderPos, size);

        if (!disabled && g_Ctx.MouseClicked) {
            if (hovered) { g_Ctx.DraggingSliderId = id; g_Ctx.FocusedSliderId = id; g_Ctx.ActiveId = id; }
            else if (g_Ctx.FocusedSliderId == id) g_Ctx.FocusedSliderId = 0;
        }

        if (!g_Ctx.MouseDown && g_Ctx.DraggingSliderId == id) g_Ctx.DraggingSliderId = 0;

        if (!disabled && g_Ctx.DraggingSliderId == id) {
            g_Ctx.ActiveId = id;
            float ratio = std::clamp((g_Ctx.MousePos.x - sliderPos.x) / sliderWidth, 0.f, 1.f);
            float newVal = min_val + ratio * (max_val - min_val);
            if (step > 0.f) newVal = std::round(newVal / step) * step;
            *value = std::clamp(newVal, min_val, max_val);
        }

        if (!disabled && g_Ctx.FocusedSliderId == id) {
            if (g_Ctx.KeyPressed[VK_LEFT]) {
                float modifyVal = *value - (step > 0.f ? step : (max_val - min_val) * g_Ctx.Style.SliderKeyboardStepRatio);
                if (step > 0.f) modifyVal = std::round(modifyVal / step) * step;
                *value = std::clamp(modifyVal, min_val, max_val);
            }
            if (g_Ctx.KeyPressed[VK_RIGHT]) {
                float modifyVal = *value + (step > 0.f ? step : (max_val - min_val) * g_Ctx.Style.SliderKeyboardStepRatio);
                if (step > 0.f) modifyVal = std::round(modifyVal / step) * step;
                *value = std::clamp(modifyVal, min_val, max_val);
            }
        }

        Color bgColor = disabled ? GetColor(GuiCol_ControlDisabled) : (hovered ? GetColor(GuiCol_FrameBgHovered) : GetColor(GuiCol_FrameBg));
        GetWindowDrawList()->AddRectFilled(sliderPos, size, bgColor);

        float fillWidth = std::clamp((*value - min_val) / (max_val - min_val), 0.f, 1.f) * sliderWidth;
        Color grabCol = GetColor(GuiCol_SliderGrab);
        GetWindowDrawList()->AddRectFilled(sliderPos, { fillWidth, size.y }, grabCol);

        float knobWidth = g_Ctx.Style.SliderKnobWidth;
        float knobX = sliderPos.x + fillWidth - knobWidth * 0.5f;
        knobX = std::clamp(knobX, sliderPos.x, sliderPos.x + sliderWidth - knobWidth);

        Color knobCol = GetColor(GuiCol_SliderKnob);
        GetWindowDrawList()->AddRectFilled({ knobX, sliderPos.y }, { knobWidth, size.y }, knobCol);

        if (g_Ctx.FocusedSliderId == id) {
            Color border = GetColor(GuiCol_Border);
            GetWindowDrawList()->AddRect(sliderPos, size, border, g_Ctx.Style.SliderFocusBorderThickness);
        }

        bool changed = InputTextEx(sliderInputId, valBoxPos, valBoxSize, g_Ctx.InputBuffers[sliderInputId], ShadowInputTextFlags_CharsDecimal | ShadowInputTextFlags_AlignCenter);

        if (changed && !disabled) {
            const std::string& str = g_Ctx.InputBuffers[sliderInputId];
            float newVal;
            auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), newVal);

            if (ec == std::errc() && ptr > str.data()) {
                if (step > 0.f) newVal = std::round(newVal / step) * step;
                *value = std::clamp(newVal, min_val, max_val);
            }
        }

        if (g_Ctx.ActiveInputId == sliderInputId) {
            g_Ctx.ActiveId = id;
        }

        PopID();
    }

    inline void ColorPicker(std::string_view name, float* r, float* g, float* b, float* a, ShadowColorPickerFlags flags = ShadowColorPickerFlags_None, Vec2 size_arg = { 0.f, 0.f }) {
        if (!g_Ctx.InActiveTab) return;
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;
        PushID(name);

        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;
        bool disabled = IsDisabled();
        bool noText = (flags & ShadowColorPickerFlags_NoText) != 0;
        bool noRightAlign = (flags & ShadowColorPickerFlags_NoRightAlign) != 0;

        float textWidth = 0.f;
        if (!noText) {
            textWidth = MeasureTextSize(display).x;
        }

        Vec2 pos = g_Ctx.Cursor;
        Vec2 boxSize = { size_arg.x > 0.f ? size_arg.x : itemHeight, itemHeight };
        Vec2 boxPos;

        if (noRightAlign) {
            boxPos = { pos.x + (noText ? 0.f : textWidth + 10.f), pos.y };
        }
        else {
            float rightMargin = GetRightMargin();
            boxPos = { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - rightMargin - boxSize.x, pos.y };
        }

        Vec2 min = { std::min(pos.x, boxPos.x), pos.y };
        Vec2 max = { boxPos.x + boxSize.x, pos.y + itemHeight };

        ItemSize(min, max);
        if (!ItemAdd(min, max, id, disabled)) {
            PopID();
            return;
        }

        if (!noText) {
            Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);
            GetWindowDrawList()->AddText({ pos.x, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, display);
        }

        bool hovered = !disabled && IsMouseHovering(boxPos, boxSize);

        if (hovered && g_Ctx.MouseDown) {
            g_Ctx.ActiveId = id;
        }

        if (hovered && g_Ctx.MouseClicked) {
            g_Ctx.IsDragging = false;

            if (IsPopupOpen("##ColorPickerPopup")) {
                CloseCurrentPopup();
            }
            else {
                OpenPopup("##ColorPickerPopup");

                g_Ctx.ColorPickerR = r; g_Ctx.ColorPickerG = g; g_Ctx.ColorPickerB = b; g_Ctx.ColorPickerA = a;
                RGBtoHSV(*r, *g, *b, g_Ctx.ColorPickerH, g_Ctx.ColorPickerS, g_Ctx.ColorPickerV);

                float padding = g_Ctx.Style.CPPadding;
                float svSize = g_Ctx.Style.CPSVSize;
                float hueWidth = g_Ctx.Style.CPHueWidth;
                float alphaWidth = g_Ctx.Style.CPAlphaWidth;
                float spacing = g_Ctx.Style.CPSpacing;

                float hexBoxHeight = std::max(g_Ctx.Style.ColorPickerHexBoxMinHeight, g_Ctx.ItemHeight + g_Ctx.Style.ColorPickerHexBoxExtraHeight);
                float popupWidth = padding * 2.f + svSize + spacing * 2.f + hueWidth + alphaWidth;
                float popupHeight = padding * 2.f + svSize + spacing + hexBoxHeight;

                SetNextWindowPos({ boxPos.x + boxSize.x + g_Ctx.Style.ItemSpacing.x, boxPos.y + itemHeight + g_Ctx.Style.PopupHeightExtra });
                SetNextWindowSize({ popupWidth, popupHeight });
            }
            g_Ctx.MouseClicked = false;
        }

        float globalAlpha = GetColor(GuiCol_ColorPickerLight).a;

        Color cbLight = GetColor(GuiCol_CheckerboardLight);
        Color cbDark = GetColor(GuiCol_CheckerboardDark);

        float currentA = a ? *a : 1.0f;
        int checkerSize = g_Ctx.Style.ColorPickerCheckerSize;

        for (int y = 0; y < boxSize.y; y += checkerSize) {
            for (int x = 0; x < boxSize.x; x += checkerSize) {
                bool isWhite = ((x / checkerSize) + (y / checkerSize)) % 2 == 0;
                Color bgCol = isWhite ? cbLight : cbDark;

                float finalR = *r * currentA + bgCol.r * (1.f - currentA);
                float finalG = *g * currentA + bgCol.g * (1.f - currentA);
                float finalB = *b * currentA + bgCol.b * (1.f - currentA);

                float drawX = boxPos.x + x;
                float drawY = boxPos.y + y;
                float w = std::min((float)checkerSize, boxSize.x - x);
                float h = std::min((float)checkerSize, boxSize.y - y);

                GetWindowDrawList()->AddRectFilled({ drawX, drawY }, { w, h }, { finalR, finalG, finalB, globalAlpha });
            }
        }

        if (hovered || IsPopupOpen("##ColorPickerPopup")) {
            Color border = GetColor(GuiCol_Border);
            GetWindowDrawList()->AddRect(boxPos, boxSize, border);
        }

        PushStyleVar(GuiStyleVar_WindowPadding, { 0.f, 0.f });

        if (BeginPopup("##ColorPickerPopup")) {
            float padding = g_Ctx.Style.CPPadding;
            float svSize = g_Ctx.Style.CPSVSize;
            float hueWidth = g_Ctx.Style.CPHueWidth;
            float alphaWidth = g_Ctx.Style.CPAlphaWidth;
            float spacing = g_Ctx.Style.CPSpacing;

            float hexBoxHeight = std::max(g_Ctx.Style.ColorPickerHexBoxMinHeight, g_Ctx.ItemHeight + g_Ctx.Style.ColorPickerHexBoxExtraHeight);
            float popupWidth = padding * 2.f + svSize + spacing * 2.f + hueWidth + alphaWidth;
            float popupHeight = padding * 2.f + svSize + spacing + hexBoxHeight;

            Vec2 popupPos = g_Ctx.WindowPos;
            Vec2 svPos = { popupPos.x + padding, popupPos.y + padding };
            Vec2 huePos = { svPos.x + svSize + spacing, svPos.y };
            Vec2 alphaPos = { huePos.x + hueWidth + spacing, svPos.y };
            Vec2 hexPos = { svPos.x, svPos.y + svSize + spacing };
            Vec2 hexSize = { popupWidth - padding * 2.f, hexBoxHeight };

            bool svHovered = IsMouseHoveringRaw(svPos, { svSize, svSize });
            bool hueHovered = IsMouseHoveringRaw(huePos, { hueWidth, svSize });
            bool alphaHovered = IsMouseHoveringRaw(alphaPos, { alphaWidth, svSize });

            if (g_Ctx.MouseDown) {
                if (!g_Ctx.IsDraggingSV && !g_Ctx.IsDraggingHue && !g_Ctx.IsDraggingAlpha) {
                    if (svHovered) g_Ctx.IsDraggingSV = true;
                    else if (hueHovered) g_Ctx.IsDraggingHue = true;
                    else if (alphaHovered) g_Ctx.IsDraggingAlpha = true;
                }

                if (g_Ctx.IsDraggingSV) {
                    g_Ctx.ColorPickerS = std::clamp((g_Ctx.MousePos.x - svPos.x) / svSize, 0.f, 1.f);
                    g_Ctx.ColorPickerV = 1.0f - std::clamp((g_Ctx.MousePos.y - svPos.y) / svSize, 0.f, 1.f);
                    HSVtoRGB(g_Ctx.ColorPickerH, g_Ctx.ColorPickerS, g_Ctx.ColorPickerV, *r, *g, *b);
                }
                else if (g_Ctx.IsDraggingHue) {
                    g_Ctx.ColorPickerH = std::clamp((g_Ctx.MousePos.y - huePos.y) / svSize, 0.f, 1.f);
                    HSVtoRGB(g_Ctx.ColorPickerH, g_Ctx.ColorPickerS, g_Ctx.ColorPickerV, *r, *g, *b);
                }
                else if (g_Ctx.IsDraggingAlpha) {
                    if (a) {
                        *a = 1.0f - std::clamp((g_Ctx.MousePos.y - alphaPos.y) / svSize, 0.f, 1.f);
                    }
                }
            }
            else {
                g_Ctx.IsDraggingSV = false;
                g_Ctx.IsDraggingHue = false;
                g_Ctx.IsDraggingAlpha = false;
            }

            float pickerAlpha = GetColor(GuiCol_ColorPickerLight).a;
            Color shadowCol = GetColor(GuiCol_ColorPickerShadow);

            int svSteps = std::max(2, static_cast<int>(svSize));
            float svStepSize = svSize / svSteps;

            for (int i = 0; i < svSteps; ++i) {
                float s = (float)i / (svSteps - 1);
                float pr, pg, pb;
                HSVtoRGB(g_Ctx.ColorPickerH, s, 1.0f, pr, pg, pb);
                GetWindowDrawList()->AddRectFilled({ svPos.x + i * svStepSize, svPos.y }, { svStepSize, svSize }, { pr, pg, pb, pickerAlpha });
            }
            for (int j = 0; j < svSteps; ++j) {
                float v = 1.0f - (float)j / (svSteps - 1);
                float drawAlpha = 1.0f - v;
                GetWindowDrawList()->AddRectFilled({ svPos.x, svPos.y + j * svStepSize }, { svSize, svStepSize }, { shadowCol.r, shadowCol.g, shadowCol.b, drawAlpha * shadowCol.a });
            }

            int hueSteps = std::max(2, static_cast<int>(svSize));
            float hueStepSize = svSize / hueSteps;
            for (int i = 0; i < hueSteps; ++i) {
                float h = (float)i / (hueSteps - 1);
                float pr, pg, pb;
                HSVtoRGB(h, 1.f, 1.f, pr, pg, pb);
                GetWindowDrawList()->AddRectFilled({ huePos.x, huePos.y + i * hueStepSize }, { hueWidth, hueStepSize }, { pr, pg, pb, pickerAlpha });
            }

            int alphaSteps = std::max(2, static_cast<int>(svSize));
            float alphaStepSize = svSize / alphaSteps;

            for (int i = 0; i < alphaSteps; ++i) {
                float drawAlpha = 1.0f - (float)i / (alphaSteps - 1);
                float drawY = alphaPos.y + i * alphaStepSize;

                for (int x = 0; x < alphaWidth; x += checkerSize) {
                    float drawX = alphaPos.x + x;
                    float w = std::min((float)checkerSize, alphaWidth - x);

                    int checkY = static_cast<int>((drawY - alphaPos.y) / checkerSize);
                    int checkX = x / checkerSize;
                    bool isWhite = (checkX + checkY) % 2 == 0;
                    Color bgCol = isWhite ? cbLight : cbDark;

                    float finalR = *r * drawAlpha + bgCol.r * (1.f - drawAlpha);
                    float finalG = *g * drawAlpha + bgCol.g * (1.f - drawAlpha);
                    float finalB = *b * drawAlpha + bgCol.b * (1.f - drawAlpha);

                    GetWindowDrawList()->AddRectFilled({ drawX, drawY }, { w, alphaStepSize }, { finalR, finalG, finalB, pickerAlpha });
                }
            }

            size_t hexId = GetID("##HexInput");
            if (g_Ctx.ActiveInputId != hexId) {
                uint8_t r8 = static_cast<uint8_t>(*r * 255.f);
                uint8_t g8 = static_cast<uint8_t>(*g * 255.f);
                uint8_t b8 = static_cast<uint8_t>(*b * 255.f);
                uint8_t a8 = a ? static_cast<uint8_t>(*a * 255.f) : 255;
                g_Ctx.InputBuffers[hexId] = std::format("{:02X}{:02X}{:02X}{:02X}", r8, g8, b8, a8);
            }

            bool hexChanged = InputTextEx(hexId, hexPos, hexSize, g_Ctx.InputBuffers[hexId], ShadowInputTextFlags_CharsHexadecimal | ShadowInputTextFlags_CharsUppercase, true);
            if (hexChanged) {
                ApplyHexInput(hexId);
            }

            // 绘制 SV 选择器的 Cursor
            Vec2 cursorSV = { svPos.x + g_Ctx.ColorPickerS * svSize, svPos.y + (1.f - g_Ctx.ColorPickerV) * svSize };
            float cursorOuterHalf = g_Ctx.Style.ColorPickerCursorSize * 0.5f;
            float cursorInnerHalf = g_Ctx.Style.ColorPickerCursorInnerSize * 0.5f;
            float cursorCenterHalf = g_Ctx.Style.ColorPickerCursorCenterSize * 0.5f;
            GetWindowDrawList()->AddRect({ cursorSV.x - cursorOuterHalf, cursorSV.y - cursorOuterHalf }, { g_Ctx.Style.ColorPickerCursorSize, g_Ctx.Style.ColorPickerCursorSize }, GetColor(GuiCol_ColorPickerDark));
            GetWindowDrawList()->AddRect({ cursorSV.x - cursorInnerHalf, cursorSV.y - cursorInnerHalf }, { g_Ctx.Style.ColorPickerCursorInnerSize, g_Ctx.Style.ColorPickerCursorInnerSize }, GetColor(GuiCol_ColorPickerLight));
            GetWindowDrawList()->AddRectFilled({ cursorSV.x - cursorCenterHalf, cursorSV.y - cursorCenterHalf }, { g_Ctx.Style.ColorPickerCursorCenterSize, g_Ctx.Style.ColorPickerCursorCenterSize }, { *r, *g, *b, pickerAlpha });

            // 绘制 Hue 选择器的 Cursor
            Vec2 cursorHue = { huePos.x - cursorCenterHalf, huePos.y + g_Ctx.ColorPickerH * svSize - cursorCenterHalf };
            GetWindowDrawList()->AddRect(cursorHue, { hueWidth + g_Ctx.Style.ColorPickerCursorCenterSize, g_Ctx.Style.ColorPickerCursorCenterSize }, GetColor(GuiCol_ColorPickerDark));
            GetWindowDrawList()->AddRect({ cursorHue.x + g_Ctx.Style.SliderFocusBorderThickness, cursorHue.y + g_Ctx.Style.SliderFocusBorderThickness }, { hueWidth + g_Ctx.Style.ColorPickerCursorCenterSize - g_Ctx.Style.SliderFocusBorderThickness * 2.f, g_Ctx.Style.ColorPickerCursorCenterSize - g_Ctx.Style.SliderFocusBorderThickness * 2.f }, GetColor(GuiCol_ColorPickerLight));

            if (a) {
                Vec2 cursorAlpha = { alphaPos.x - cursorCenterHalf, alphaPos.y + (1.f - *a) * svSize - cursorCenterHalf };
                GetWindowDrawList()->AddRect(cursorAlpha, { alphaWidth + g_Ctx.Style.ColorPickerCursorCenterSize, g_Ctx.Style.ColorPickerCursorCenterSize }, GetColor(GuiCol_ColorPickerDark));
                GetWindowDrawList()->AddRect({ cursorAlpha.x + g_Ctx.Style.SliderFocusBorderThickness, cursorAlpha.y + g_Ctx.Style.SliderFocusBorderThickness }, { alphaWidth + g_Ctx.Style.ColorPickerCursorCenterSize - g_Ctx.Style.SliderFocusBorderThickness * 2.f, g_Ctx.Style.ColorPickerCursorCenterSize - g_Ctx.Style.SliderFocusBorderThickness * 2.f }, GetColor(GuiCol_ColorPickerLight));
            }

            SetCursorPosY(GetCursorPosY() + popupHeight);
            g_Ctx.LastItemMaxX = g_Ctx.WindowPos.x + popupWidth;
        }
        EndPopup();
        PopStyleVar();

        PopID();
    }

    inline void ColorPicker(std::string_view name, Color* color, ShadowColorPickerFlags flags = ShadowColorPickerFlags_None, Vec2 size_arg = { 0.f, 0.f }) {
        if (color) {
            ColorPicker(name, &color->r, &color->g, &color->b, &color->a, flags, size_arg);
        }
    }

    inline bool HotKey(std::string_view name, int* hotkey, ShadowHotkeyFlags flags = ShadowHotkeyFlags_None, Vec2 size_arg = { 0.f, 0.f }) {
        RegisterHotkey(hotkey, nullptr, nullptr);

        if (!g_Ctx.InActiveTab) return false;
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;

        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;
        bool disabled = IsDisabled();
        bool noText = (flags & ShadowHotkeyFlags_NoText) != 0;
        bool noRightAlign = (flags & ShadowHotkeyFlags_NoRightAlign) != 0;

        float textWidth = 0.f;
        if (!noText) {
            textWidth = MeasureTextSize(display).x;
        }

        bool isAssigning = (g_Ctx.AssigningHotkey == hotkey);
        std::string keyName = isAssigning ? "[Press Key]" : std::format("[{}]", GetKeyName(*hotkey));

        Vec2 pos = g_Ctx.Cursor;
        Vec2 btnSize = { MeasureTextSize(keyName).x + g_Ctx.Style.FramePadding.x * 2.f, itemHeight };
        if (size_arg.x > 0.f) btnSize.x = size_arg.x;
        Vec2 btnPos;

        if (noRightAlign) {
            btnPos = { pos.x + (noText ? 0.f : textWidth + g_Ctx.Style.LabelSpacing), pos.y };
        }
        else {
            float rightMargin = GetRightMargin();
            btnPos = { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - rightMargin - btnSize.x, pos.y };
        }

        Vec2 min = { std::min(pos.x, btnPos.x), pos.y };
        Vec2 max = { btnPos.x + btnSize.x, pos.y + itemHeight };

        ItemSize(min, max);
        if (!ItemAdd(min, max, id, disabled)) {
            return false;
        }

        Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);

        if (!noText) {
            GetWindowDrawList()->AddText({ pos.x, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, display);
        }

        bool btnHovered = !disabled && IsMouseHovering(btnPos, btnSize);
        if (btnHovered && g_Ctx.MouseDown) {
            g_Ctx.ActiveId = id;
        }
        if (btnHovered && g_Ctx.MouseClicked) {
            g_Ctx.IsDragging = false;
            g_Ctx.AssigningHotkey = hotkey;
            g_Ctx.MouseClicked = false;
        }

        Color bgColor = disabled ? GetColor(GuiCol_ControlDisabled) : (btnHovered ? GetColor(GuiCol_ButtonHovered) : GetColor(GuiCol_Button));
        GetWindowDrawList()->AddRectFilled(btnPos, btnSize, bgColor);
        GetWindowDrawList()->AddText({ btnPos.x + g_Ctx.Style.FramePadding.x, btnPos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, keyName);

        return *hotkey != 0 && g_Ctx.KeyStates[*hotkey];
    }

    inline void HotKey(std::string_view name, int* hotkey, bool* is_active, HotkeyMode* hotkey_mode, ShadowHotkeyFlags flags = ShadowHotkeyFlags_None, Vec2 size_arg = { 0.f, 0.f }) {
        RegisterHotkey(hotkey, hotkey_mode, is_active);

        if (!g_Ctx.InActiveTab) return;
        std::string_view display; size_t id; ParseLabel(name, display, id);
        g_Ctx.WidgetCount++;
        PushID(name);

        float itemHeight = size_arg.y > 0.f ? size_arg.y : g_Ctx.ItemHeight;
        bool disabled = IsDisabled();
        bool noText = (flags & ShadowHotkeyFlags_NoText) != 0;
        bool noRightAlign = (flags & ShadowHotkeyFlags_NoRightAlign) != 0;
        bool noStateDisplay = (flags & ShadowHotkeyFlags_NoStateDisplay) != 0;

        float textWidth = 0.f;
        if (!noText) {
            textWidth = MeasureTextSize(display).x;
        }

        bool isAssigning = (g_Ctx.AssigningHotkey == hotkey);
        std::vector<std::string> modeStrs = { "None", "Hold On", "Toggle On", "Hold Off", "Always On" };

        std::string keyName = isAssigning ? "[Press Key]" : std::format("[{}]", GetKeyName(*hotkey));
        Vec2 btnSize = { MeasureTextSize(keyName).x + g_Ctx.Style.FramePadding.x * 2.f, itemHeight };
        if (size_arg.x > 0.f) btnSize.x = size_arg.x;

        float dotSize = std::max(g_Ctx.Style.HotkeyDotSizeMin, itemHeight * g_Ctx.Style.HotkeyDotSizeRatio);
        float dotOffset = (itemHeight - dotSize) / 2.f;

        Vec2 pos = g_Ctx.Cursor;
        Vec2 btnPos;
        if (noRightAlign) {
            btnPos = { pos.x + (noText ? 0.f : textWidth + g_Ctx.Style.LabelSpacing), pos.y };
        }
        else {
            float rightMargin = GetRightMargin();
            if (noStateDisplay) {
                btnPos = { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - rightMargin - btnSize.x, pos.y };
            }
            else {
                btnPos = { g_Ctx.WindowPos.x + g_Ctx.WindowSize.x - rightMargin - dotSize - g_Ctx.Style.ItemSpacing.x - btnSize.x, pos.y };
            }
        }

        float maxX = noStateDisplay ? (btnPos.x + btnSize.x) : (btnPos.x + btnSize.x + g_Ctx.Style.ItemSpacing.x + dotSize);
        Vec2 min = { std::min(pos.x, btnPos.x), pos.y };
        Vec2 max = { maxX, pos.y + itemHeight };

        ItemSize(min, max);
        if (!ItemAdd(min, max, id, disabled)) {
            PopID();
            return;
        }

        Color textColor = disabled ? GetColor(GuiCol_TextDisabled) : GetColor(GuiCol_Text);

        if (!noText) {
            GetWindowDrawList()->AddText({ pos.x, pos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, display);
        }

        bool btnHovered = !disabled && IsMouseHovering(btnPos, btnSize);
        if (btnHovered && g_Ctx.MouseDown) {
            g_Ctx.ActiveId = id;
        }

        if (btnHovered) {
            if (g_Ctx.MouseClicked) {
                g_Ctx.IsDragging = false;
                g_Ctx.AssigningHotkey = hotkey;
                g_Ctx.MouseClicked = false;
            }
            else if (g_Ctx.RightMouseClicked) {
                g_Ctx.IsDragging = false;

                if (IsPopupOpen("##HotkeyModePopup")) {
                    CloseCurrentPopup();
                }
                else {
                    OpenPopup("##HotkeyModePopup");
                    SetNextWindowPos({ btnPos.x, btnPos.y + btnSize.y });
                    SetNextWindowSize({ g_Ctx.Style.HotkeyModePopupWidth, modeStrs.size() * g_Ctx.ItemHeight });
                }
                g_Ctx.RightMouseClicked = false;
            }
        }

        Color bgColor = disabled ? GetColor(GuiCol_ControlDisabled) : (btnHovered ? GetColor(GuiCol_ButtonHovered) : GetColor(GuiCol_Button));
        GetWindowDrawList()->AddRectFilled(btnPos, btnSize, bgColor);
        GetWindowDrawList()->AddText({ btnPos.x + g_Ctx.Style.FramePadding.x, btnPos.y + g_Ctx.Style.FramePadding.y + (itemHeight - g_Ctx.ItemHeight) * 0.5f }, textColor, keyName);

        PushStyleVar(GuiStyleVar_WindowPadding, { 0.f, 0.f });

        // 限制其拖拽
        if (BeginPopup("##HotkeyModePopup", ShadowWindowFlags_NoMove)) {
            for (size_t i = 0; i < modeStrs.size(); ++i) {
                Vec2 itemPos = g_Ctx.Cursor;
                bool itemHovered = IsMouseHoveringRaw(itemPos, { g_Ctx.Style.HotkeyModePopupWidth, g_Ctx.ItemHeight });
                bool isCurrentItem = (*hotkey_mode == static_cast<HotkeyMode>(i));

                if (itemHovered) {
                    GetWindowDrawList()->AddRectFilled(itemPos, { g_Ctx.Style.HotkeyModePopupWidth, g_Ctx.ItemHeight }, GetColor(GuiCol_FrameBgHovered));
                    if (g_Ctx.MouseClicked) {
                        *hotkey_mode = static_cast<HotkeyMode>(i);
                        CloseCurrentPopup();
                        g_Ctx.MouseClicked = false;
                    }
                }
                else if (isCurrentItem) {
                    GetWindowDrawList()->AddRectFilled(itemPos, { g_Ctx.Style.HotkeyModePopupWidth, g_Ctx.ItemHeight }, GetColor(GuiCol_DropdownActive));
                }

                Color textCol = isCurrentItem ? GetColor(GuiCol_TextHighlight) : GetColor(GuiCol_Text);
                GetWindowDrawList()->AddText({ itemPos.x + g_Ctx.Style.FramePadding.x, itemPos.y + g_Ctx.Style.FramePadding.y }, textCol, modeStrs[i]);

                SetCursorPosY(GetCursorPosY() + g_Ctx.ItemHeight);
            }
            g_Ctx.LastItemMaxX = g_Ctx.WindowPos.x + g_Ctx.Style.HotkeyModePopupWidth;
        }
        EndPopup();
        PopStyleVar();

        switch (*hotkey_mode) {
        case HotkeyMode::None:      *is_active = false; break;
        case HotkeyMode::HoldOn:    *is_active = (*hotkey != 0) && g_Ctx.KeyStates[*hotkey]; break;
        case HotkeyMode::HoldOff:   *is_active = (*hotkey != 0) && !g_Ctx.KeyStates[*hotkey]; break;
        case HotkeyMode::ToggleOn:  *is_active = (*hotkey != 0) && g_Ctx.HotkeyToggles[*hotkey]; break;
        case HotkeyMode::AlwaysOn:  *is_active = true; break;
        }

        if (!noStateDisplay) {
            Color indicatorColor = GetColor(*is_active ? GuiCol_ActiveIndicator : GuiCol_InactiveIndicator);
            GetWindowDrawList()->AddRectFilled({ btnPos.x + btnSize.x + g_Ctx.Style.ItemSpacing.x, pos.y + dotOffset }, { dotSize, dotSize }, indicatorColor);
        }

        PopID();
    }
}