#pragma once
#include "../../external/Minimal-D3D12-Hook-ImGui/Main/mdx12_api.h"
#include "../../external/Minimal-D3D12-Hook-ImGui/MinHook/include/MinHook.h"
#include "../../external/AOBScan/AOBScan.hpp"
#include "../../external/CppSDK/SDK.hpp"
#include "../../external/Shadow-Gui/src/Shadow.h"
#include "../Config/Configs.h"
#include "../../XorStr.h"
#include <cmath>
#include <algorithm>
#include <string>
#include <sstream>
#include <numbers>
#include <cwctype> 
#include <shellapi.h> 

namespace g_Util {
    static const float inv255 = 1.0f / 255.0f; 

    __forceinline const char* GetKeyName(UINT vk) {
        if (vk >= 'A' && vk <= 'Z') {
            static char buf[2] = { 0 };
            buf[0] = (char)vk;
            return buf;
        }
        if (vk >= '0' && vk <= '9') {
            static char buf[2] = { 0 };
            buf[0] = (char)vk;
            return buf;
        }
        switch (vk) {
        case VK_F1: return "F1"; case VK_F2: return "F2"; case VK_F3: return "F3"; case VK_F4: return "F4";
        case VK_F5: return "F5"; case VK_F6: return "F6"; case VK_F7: return "F7"; case VK_F8: return "F8";
        case VK_F9: return "F9"; case VK_F10: return "F10"; case VK_F11: return "F11"; case VK_F12: return "F12";
        case VK_INSERT: return "INSERT"; case VK_DELETE: return "DELETE"; case VK_HOME: return "HOME";
        case VK_END: return "END"; case VK_PRIOR: return "PAGE UP"; case VK_NEXT: return "PAGE DOWN";
        case VK_TAB: return "TAB"; case VK_LSHIFT: return "LSHIFT"; case VK_RSHIFT: return "RSHIFT";
        case VK_LCONTROL: return "LCTRL"; case VK_RCONTROL: return "RCTRL"; case VK_LMENU: return "LALT";
        case VK_RMENU: return "RALT"; case VK_CAPITAL: return "CAPSLOCK"; case VK_SPACE: return "SPACE";
        case VK_RETURN: return "ENTER"; case VK_ESCAPE: return "ESCAPE";
        default: return "UNKNOWN";
        }
    }

    __forceinline std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return std::tolower(c);
            });
        return s;
    }

    __forceinline ImU32 GetU32Color(float color[4]) {
        return ImGui::ColorConvertFloat4ToU32(*(ImVec4*)color);
    }

    __forceinline ImU32 ToImColor(float r, float g, float b, float a) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(r * inv255, g * inv255, b * inv255, a * inv255));
    }

    __forceinline ImU32 GetHealthColor(float healthPercent) {
        healthPercent = fmaxf(0.0f, fminf(healthPercent, 1.0f));

        float r = fminf(1.0f, 2.0f - 2.0f * healthPercent);
        float g = fminf(1.0f, 2.0f * healthPercent);

        return ToImColor(r * 255.0f, g * 255.0f, 0.0f, 255.0f);
    }

    __forceinline bool IsCookedMeat(SDK::UPrimalItem* Item) {
        if (!Item || !Item->Class) return false;
        std::string name = Item->Class->GetName();
        return strstr(name.c_str(), _XOR_("PrimalItemConsumable_CookedMeat").crypt()) != nullptr;
    }

    __forceinline bool IsRawMeat(SDK::UPrimalItem* Item) {
        if (!Item || !Item->Class) return false;
        std::string name = Item->Class->GetName();
        return strstr(name.c_str(), _XOR_("PrimalItemConsumable_RawMeat").crypt()) != nullptr;
    }

    __forceinline bool IsEntityMatch(std::string displayName, std::string filter) {
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

    __forceinline static std::vector<std::string> SplitFilterTokens(const std::string& filter) {
        std::vector<std::string> tokens;
        std::stringstream ss(filter);
        std::string token;
        while (std::getline(ss, token, ',')) {
            size_t start = token.find_first_not_of(" \t");
            size_t end = token.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos) {
                tokens.push_back(token.substr(start, end - start + 1));
            }
            else if (start != std::string::npos) {
                tokens.push_back(token.substr(start));
            }
        }
        return tokens;
    }

    __forceinline static bool IsEntityMatchMulti(const std::string& displayName, const char* filterBuf) {
        std::string filter = filterBuf;
        if (filter.empty()) return true;

        std::vector<std::string> tokens = SplitFilterTokens(filter);
        if (tokens.empty()) return true;

        for (const std::string& token : tokens) {
            if (token.empty()) continue;
            if (g_Util::IsEntityMatch(displayName, token.c_str())) return true;
        }
        return false;
    }

    __forceinline bool IsStructureMatch(const std::string& structureName, const std::string& filter) {
        if (filter.empty()) return true;

        std::string lowerName = structureName;
        std::string lowerFilter = filter;

        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

        return lowerName.find(lowerFilter) != std::string::npos;
    }

    __forceinline static bool IsStructureMatchMulti(const std::string& structureName, const char* filterBuf) {
        std::string filter = filterBuf;
        if (filter.empty()) return true;

        std::vector<std::string> tokens = SplitFilterTokens(filter);
        if (tokens.empty()) return true;

        for (const std::string& token : tokens) {
            if (token.empty()) continue;
            if (g_Util::IsStructureMatch(structureName, token.c_str())) return true;
        }
        return false;
    }

    __forceinline SDK::APlayerController* GetLocalPC() {
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

    __forceinline float GetAngleDistance(SDK::FVector CamLoc, SDK::FVector TargetLoc, SDK::FRotator CamRot) {
        SDK::FVector Diff = { TargetLoc.X - CamLoc.X, TargetLoc.Y - CamLoc.Y, TargetLoc.Z - CamLoc.Z };
        SDK::FVector DirToTarget = SDK::UKismetMathLibrary::Normal(Diff, 0.0001f);
        SDK::FVector CamForward = SDK::UKismetMathLibrary::GetForwardVector(CamRot);
        float Dot = SDK::UKismetMathLibrary::Dot_VectorVector(DirToTarget, CamForward);
        Dot = SDK::UKismetMathLibrary::FClamp(Dot, -1.0f, 1.0f);
        return SDK::UKismetMathLibrary::DegAcos(Dot);
    }

    __forceinline void MimicMouseClick(bool bPress) {
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = bPress ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));
    }

    __forceinline void ProcessDinoFeed(SDK::AShooterPlayerController* PC, SDK::APrimalDinoCharacter* Dino) {
        if (!Dino || Dino->IsDead()) return;

        float curHP = Dino->GetHealth();
        float maxHP = Dino->GetMaxHealth();
        if (maxHP <= 0.0f || curHP >= (maxHP - 1.0f)) return;

        SDK::UPrimalInventoryComponent* Inv = Dino->MyInventoryComponent;
        if (!Inv) return;

        SDK::TArray<SDK::UPrimalItem*>& Items = Inv->InventoryItems;
        SDK::UPrimalItem* BestMeat = nullptr;

        // 寻找食物：优先熟肉，其次生肉
        for (int i = 0; i < Items.Num(); i++) {
            SDK::UPrimalItem* Item = Items[i];
            if (!Item || !Item->Class) continue;

            if (IsCookedMeat(Item)) {
                BestMeat = Item;
                break; // 熟肉回血多，找到直接跳出
            }
            if (IsRawMeat(Item)) {
                BestMeat = Item; // 先记下生肉，继续看有没有熟肉
            }
        }

        if (BestMeat) {
            // 使用针对龙的 RPC
            PC->ServerRequestInventoryUseItem(Inv, BestMeat->ItemID, BestMeat->SlotIndex);
        }
    }

    __forceinline static std::string IntToStr(int v)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), _XOR_("%d").crypt(), v);
        return buf; // NRVO / SSO，比 std::to_string 开销低
    }

    __forceinline static float ApproachAlpha(float cur, float target, float deltaSeconds, float fadeTime)
    {
        if (fadeTime <= 0.0f) return target;
        float diff = target - cur;
        float maxStep = deltaSeconds / fadeTime;
        if (fabsf(diff) <= maxStep) return target;
        return cur + (diff > 0.0f ? maxStep : -maxStep);
    }

    __forceinline static ImU32 ResolveDroppedItemColor(const std::string& className, float itemRating, int quantity)
    {
        const char* cn = className.c_str();

        if (strstr(cn, _XOR_("PrimalItem_WeaponEmptyCryopod").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemCryopodColor);
        if (strstr(cn, _XOR_("Egg").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemEggColor);
        if (quantity >= 1000)
            return g_Util::GetU32Color(g_Config::DroppedItemPiledColor);
        if (strstr(cn, _XOR_("PrimalItemResource_FungalWood").crypt()) || strstr(cn, _XOR_("PrimalItemResource_Wood").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemWoodColor);
        if (strstr(cn, _XOR_("PrimalItemResource_Thatch").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemThatchColor);
        if (strstr(cn, _XOR_("PrimalItemResource_Hide").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemHideColor);
        if (strstr(cn, _XOR_("PrimalItemResource_Pelt").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemPeltColor);
        if (strstr(cn, _XOR_("PrimalItemResource_Keratin").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemKeratinColor);
        if (strstr(cn, _XOR_("PrimalItemResource_Chitin").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemChitinColor);
        if (strstr(cn, _XOR_("PrimalItemResource_CorruptedPolymer").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemCorruptedPolymerColor);
        if (strstr(cn, _XOR_("PrimalItemResource_Polymer_Organic").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemPolymer_OrganicColor);
        if (strstr(cn, _XOR_("PrimalItemResource_Polymer").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemPolymerColor);
        if (strstr(cn, _XOR_("PrimalItemResource_ScrapMetalIngot").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_MetalIngot").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_ScrapMetal").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_Metal").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemMetalColor);
        if (strstr(cn, _XOR_("PrimalItemResource_Stone").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemStoneColor);
        if (strstr(cn, _XOR_("PrimalItemResource_Crystal").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemCrystalColor);
        if (strstr(cn, _XOR_("PrimalItemResource_Gem_Fertile").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_Gem_BioLum").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_Gem_Element").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_BlueSap").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_RedSap").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemGemColor);
        if (strstr(cn, _XOR_("PrimalItemResource_Silicon").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_BlackPearl").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemPearlColor);
        if (strstr(cn, _XOR_("PrimalItemConsumable_SpoiledMeat").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemSpoiledMeatColor);
        if (strstr(cn, _XOR_("PrimalItemConsumable_RawMeat").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_RawPrimeMeat").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_RawMutton").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_RawPrimeMeat_Fish").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_RawMeat_Fish").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedMeat").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedPrimeMeat").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedLambChop").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedPrimeMeat_Fish").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedMeat_Fish").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedMeat_Jerky").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedPrimeMeat_Jerky").crypt()))
            return g_Util::GetU32Color(g_Config::DroppedItemMeatColor);

        // 按品质评级上色
        if (itemRating >= 10.0f) return g_Util::ToImColor(0, 255, 255, 255);
        if (itemRating >= 7.0f) return g_Util::ToImColor(255, 255, 0, 255);
        if (itemRating >= 4.5f) return g_Util::ToImColor(160, 32, 240, 255);
        if (itemRating >= 2.5f) return g_Util::ToImColor(0, 191, 255, 255);
        if (itemRating >= 1.25f) return g_Util::ToImColor(50, 205, 50, 255);

        return g_Util::GetU32Color(g_Config::DroppedItemNameColor);
    }

    __forceinline bool IsDroppedItemAllowed(const std::string& className, int quantity)
    {
        const char* cn = className.c_str();

        if (strstr(cn, _XOR_("PrimalItem_WeaponEmptyCryopod").crypt()))
            return g_Config::bDroppedItemCryopod;
        if (strstr(cn, _XOR_("Egg").crypt()))
            return g_Config::bDroppedItemEgg;
        if (quantity >= 1000)
            return g_Config::bDroppedItemPiled;
        if (strstr(cn, _XOR_("PrimalItemResource_FungalWood").crypt()) || strstr(cn, _XOR_("PrimalItemResource_Wood").crypt()))
            return g_Config::bDroppedItemWood;
        if (strstr(cn, _XOR_("PrimalItemResource_Thatch").crypt()))
            return g_Config::bDroppedItemThatch;
        if (strstr(cn, _XOR_("PrimalItemResource_Hide").crypt()))
            return g_Config::bDroppedItemHide;
        if (strstr(cn, _XOR_("PrimalItemResource_Pelt").crypt()))
            return g_Config::bDroppedItemPelt;
        if (strstr(cn, _XOR_("PrimalItemResource_Keratin").crypt()))
            return g_Config::bDroppedItemKeratin;
        if (strstr(cn, _XOR_("PrimalItemResource_Chitin").crypt()))
            return g_Config::bDroppedItemChitin;
        if (strstr(cn, _XOR_("PrimalItemResource_CorruptedPolymer").crypt()))
            return g_Config::bDroppedItemCorruptedPolymer;
        if (strstr(cn, _XOR_("PrimalItemResource_Polymer_Organic").crypt()))
            return g_Config::bDroppedItemPolymer_Organic;
        if (strstr(cn, _XOR_("PrimalItemResource_Polymer").crypt()))
            return g_Config::bDroppedItemPolymer;
        if (strstr(cn, _XOR_("PrimalItemResource_ScrapMetalIngot").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_MetalIngot").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_ScrapMetal").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_Metal").crypt()))
            return g_Config::bDroppedItemMetal;
        if (strstr(cn, _XOR_("PrimalItemResource_Stone").crypt()))
            return g_Config::bDroppedItemStone;
        if (strstr(cn, _XOR_("PrimalItemResource_Crystal").crypt()))
            return g_Config::bDroppedItemCrystal;
        if (strstr(cn, _XOR_("PrimalItemResource_Gem_Fertile").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_Gem_BioLum").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_Gem_Element").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_BlueSap").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_RedSap").crypt()))
            return g_Config::bDroppedItemGem;
        if (strstr(cn, _XOR_("PrimalItemResource_Silicon").crypt()) ||
            strstr(cn, _XOR_("PrimalItemResource_BlackPearl").crypt()))
            return g_Config::bDroppedItemPearl;
        if (strstr(cn, _XOR_("PrimalItemConsumable_SpoiledMeat").crypt()))
            return g_Config::bDroppedItemSpoiledMeat;
        if (strstr(cn, _XOR_("PrimalItemConsumable_RawMeat").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_RawPrimeMeat").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_RawMutton").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_RawPrimeMeat_Fish").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_RawMeat_Fish").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedMeat").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedPrimeMeat").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedLambChop").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedPrimeMeat_Fish").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedMeat_Fish").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedMeat_Jerky").crypt()) ||
            strstr(cn, _XOR_("PrimalItemConsumable_CookedPrimeMeat_Jerky").crypt()))
            return g_Config::bDroppedItemMeat;

        // 如果没有被上述特定分类命中，默认允许绘制（例如武器、装备等普通物品）
        return true;
    }

    __forceinline void* FindUFunction(std::string FuncName) {
        void* Address = nullptr;
        SDK::UObject* Obj = nullptr;
        SDK::UFunction* Func = nullptr;

        Obj = SDK::UObject::FindObject(FuncName);
        if (Obj && Obj->IsA(SDK::UFunction::StaticClass())) {
            Func = static_cast<SDK::UFunction*>(Obj);
        }

        if (Func && Func->ExecFunction) {
            Address = reinterpret_cast<void*>(Func->ExecFunction);
        }

        return Address;
    }

    __forceinline void PotatoGraphics(SDK::UObject* WorldContextObject, SDK::UGameViewportClient* GameViewportClient) {
        if (!WorldContextObject) return;
        if (!GameViewportClient) return;

        // 禁用后处理
        // [2] -> (ByteOffset: +0x0, Bit: 2)
        // GameViewportClient->EngineShowFlags[0] &= ~(1 << 2);

        // 地面材质降低
        // [61] -> (ByteOffset: +0x7, Bit: 5)
        // GameViewportClient->EngineShowFlags[0x7] &= ~(1 << 5);

        // 删掉远景滤镜
        // [129] -> (ByteOffset: +0x10, Bit: 1)
        // GameViewportClient->EngineShowFlags[0x10] &= ~(1 << 1);

        // 禁用天空和反射
        // [139] -> (ByteOffset: +0x11, Bit: 3)
        // GameViewportClient->EngineShowFlags[0x11] &= ~(1 << 3);

        // 解决水面闪烁
        // [220] -> (ByteOffset: +0x1B, Bit: 4)
        // GameViewportClient->EngineShowFlags[0x1B] &= ~(1 << 4);

        // 辅助宏：通过 KismetSystemLibrary 执行虚幻引擎控制台指令
        auto Exec = [WorldContextObject](const wchar_t* Cmd) {
            SDK::UKismetSystemLibrary::ExecuteConsoleCommand(WorldContextObject, SDK::FString(Cmd), nullptr);
            };

        Exec(_XOR_(L"r.TemporalAA.Upsampling 1").crypt());     // 开启 TAA 时空抗锯齿超采样 (提高边缘平滑度与画面重建质量)
        Exec(_XOR_(L"r.AntiAliasingMethod 2").crypt());        // 将抗锯齿算法切换为 TAA (0 = 关闭, 1 = FXAA, 2 = TAA, 3 = TSR)
        Exec(_XOR_(L"r.SkeletalMeshLODBias 2").crypt());       // 骨骼网格体 (如角色、生物模型) 细节等级偏移，强行降低模型面数以提升性能
        Exec(_XOR_(L"r.MaterialQualityLevel 0").crypt());      // 将全局材质质量设为最低级 (0 = Low, 1 = Medium, 2 = High)，简化复杂材质计算
        Exec(_XOR_(L"r.SubsurfaceScattering 0").crypt());      // 禁用次表面散射 (SSS) 特效，关闭皮肤、树叶等透光与漫射计算
        Exec(_XOR_(L"r.ParticleLODBias 5").crypt());           // 粒子系统细节等级偏移，大幅简化或远距离隐藏烟雾、火花等粒子特效
        Exec(_XOR_(L"p.ClothPhysics 0").crypt());              // 禁用服装与布料物理模拟 (如斗篷、装备披风)，降低 CPU 物理计算负担
        Exec(_XOR_(L"r.DisableDistortion 1").crypt());         // 禁用画面扭曲效果 (如爆炸热浪、水下波纹等折射特效)

        // 1. 阴影
        Exec(_XOR_(L"r.ShadowQuality 0").crypt());                     // 关闭通用阴影质量级别 (0 = Off/Lowest)
        Exec(_XOR_(L"r.Shadow.MaxResolution 0").crypt());              // 阴影贴图最大分辨率设为 0，禁用阴影纹理生成
        Exec(_XOR_(L"r.Shadow.DistanceScale 0").crypt());              // 阴影渲染渲染距离归零，超出 0 距离不再渲染阴影
        Exec(_XOR_(L"r.Shadow.CSM.MaxCascades 0").crypt());            // 级联阴影 (CSM) 层级设为 0，关闭级联动态阴影
        Exec(_XOR_(L"r.Shadow.Virtual.Enable 0").crypt());             // 禁用 UE5 虚拟阴影贴图 (Virtual Shadow Maps / VSM)
        Exec(_XOR_(L"r.DistanceFieldShadowing 0").crypt());            // 禁用网格体距离场阴影 (Distance Field Shadows)
        Exec(_XOR_(L"r.ContactShadows 0").crypt());                    // 禁用屏幕空间接触阴影 (Contact Shadows)
        Exec(_XOR_(L"grass.DisableDynamicShadows 1").crypt());         // 强制禁用草地与细小植被的动态投射阴影

        // 2. 光照
        Exec(_XOR_(L"r.LightFunctionQuality 0").crypt());              // 禁用灯光函数 (Light Function) 材质特效
        Exec(_XOR_(L"r.DynamicGlobalIlluminationMethod 0").crypt());   // 关掉水会闪烁
        Exec(_XOR_(L"r.LightShaftQuality 0").crypt());                 // 禁用光轴与体积光斑 (丁达尔效应/Light Shafts)

        Exec(_XOR_(L"r.LightMaxDrawDistanceScale 1").crypt());         //  不要光源最大绘制距离倍率

        Exec(_XOR_(L"r.SkylightIntensityMultiplier 5").crypt());       // 天光
        Exec(_XOR_(L"r.Lumen.ScreenProbeGather.ScreenTraces 0").crypt()); // 禁用 Lumen 屏幕空间光线追踪探测
        Exec(_XOR_(L"r.Lumen.ScreenProbeGather.RadianceCache.ProbeResolution 16").crypt()); // 将 Radiance Cache 探针分辨率降至最低 (16)

        // 3. 反射
        Exec(_XOR_(L"r.ReflectionEnvironment 0").crypt());             // 反射环境
        Exec(_XOR_(L"r.SSR.Quality 0").crypt());                       // 禁用屏幕空间反射 (Screen Space Reflections)
        Exec(_XOR_(L"r.Lumen.Reflections.Allow 0").crypt());           // 禁用 Lumen 动态高精度反射
        Exec(_XOR_(L"r.Lumen.Reflections.Contrast 0").crypt());        // 将 Lumen 反射对比度归零，降低计算复杂度
        Exec(_XOR_(L"r.MinRoughnessOverride 1").crypt());              // 强制所有材质粗糙度设为最大值 1.0 (消除光滑高光与反射)

        // 4. 植被
        Exec(_XOR_(L"foliage.DensityScale 0").crypt());                // 植被生成密度归零 (隐藏大部分渲染植被)
        Exec(_XOR_(L"foliage.LODDistanceScale 0").crypt());            // 植被 LOD 距离比例归零 (强制使用最低面数模型)
        Exec(_XOR_(L"grass.DensityScale 0").crypt());                  // 草地生成密度归零 (隐藏地表小草)
        Exec(_XOR_(L"grass.SizeScale 0").crypt());                     // 草地渲染尺寸归零
        Exec(_XOR_(L"r.Foliage.WPODisableMultiplier 1").crypt());      // 禁用植被的世界位置偏移 (WPO / 风吹风动动画)
        Exec(_XOR_(L"r.Foliage.AutoBoundsWPODisableMax 1").crypt());   // 自动关闭远距离植被的动画计算
        Exec(_XOR_(L"r.Nanite.MaxPixelsPerEdge 10").crypt());          // 极简化 Nanite 几何体 (增加每条边的像素网格大小以降低几何面数)
        Exec(_XOR_(L"wp.Runtime.HLOD.ForceDisable 1").crypt());        // 强制禁用世界分区 (World Partition) 远景 HLOD 生成

        // 5. 后处理
        Exec(_XOR_(L"r.BloomQuality 0").crypt());                      // 禁用辉光/泛光 (Bloom) 特效
        Exec(_XOR_(L"r.DepthOfFieldQuality 0").crypt());               // 禁用景深 (Depth of Field) 模糊效果
        Exec(_XOR_(L"r.MotionBlurQuality 0").crypt());                 // 禁用运动模糊 (Motion Blur) 采样质量
        Exec(_XOR_(L"r.MotionBlur.Amount 0").crypt());                 // 将运动模糊强度设为 0
        Exec(_XOR_(L"r.AmbientOcclusionLevels 0").crypt());            // 关闭环境光遮蔽 (AO / SSAO) 级别
        Exec(_XOR_(L"r.AOOverwriteSceneColor 0").crypt());             // 为 1 会导致色彩搞砸
        Exec(_XOR_(L"r.LensFlareQuality 0").crypt());                  // 禁用镜头光晕 (Lens Flare) 特效
        Exec(_XOR_(L"fx.EnableNiagaraSpriteRendering 0").crypt());     // 禁用 Niagara 粒子系统的 Sprite 纹理渲染
        Exec(_XOR_(L"ark.MaxActiveDestroyedMeshGeoCollectionCount 0").crypt()); // 禁用破碎网格体 (Geometry Collection) 物理碎片生成
        Exec(_XOR_(L"r.EyeAdaptationQuality 0").crypt());              // 不要禁用曝光采样质量

        // 6. 大气
        Exec(_XOR_(L"r.SkyAtmosphere 0").crypt());                     // 天空大气渲染
        Exec(_XOR_(L"r.Fog 0").crypt());                               // 禁用通用场景雾效 (Height Fog)
        Exec(_XOR_(L"r.VolumetricFog 0").crypt());                     // 禁用体积雾 (Volumetric Fog) 高消耗渲染
        Exec(_XOR_(L"r.VolumetricCloud 0").crypt());                   // 禁用 UE5 3D 动态体积云 (Volumetric Cloud)

        // 7. 材质
        Exec(_XOR_(L"r.MipMapLODBias 0").crypt());                     // 纹理 MipMap 偏移 (设为 0 保持正常贴图，若需贴图极糊可改大如 3~5)
        Exec(_XOR_(L"r.EmitterSpawnRateScale 0").crypt());             // 传统级联粒子 (Cascade Particle) 生成速率归零
        Exec(_XOR_(L"r.ParticleLODBias 10").crypt());                  // 强制粒子系统使用最低细节级别 (LOD)
        Exec(_XOR_(L"r.TranslucencyVolumeBlur 0").crypt());            // 禁用半透明体积模糊计算
        Exec(_XOR_(L"r.VT.EnableFeedback 0").crypt());                 // 禁用虚拟纹理 (Virtual Texture) 采样反馈机制

        // 已经被flags关了
        // 8. 水体系统
        // Exec(_XOR_(L"r.Water.SingleLayer 0").crypt());                 // 关闭单层水体 (Single Layer Water) 渲染管线
        // Exec(_XOR_(L"r.Water.SingleLayer.Reflection 0").crypt());      // 关闭水面反射计算
    }

    __forceinline bool Welcome(SDK::UWorld* World, SDK::UCanvas* Canvas)
    {
        if (g_Config::bInitWelcome) return true;

        // 只播放一次
        static bool bFinished = false;
        if (bFinished) {
            g_Config::bInitWelcome = true;
            return true;
        }

        if (!World || !Canvas || Canvas->SizeX <= 0 || Canvas->SizeY <= 0)
            return bFinished;

        // 获取 DeltaTime
        float dt = SDK::UGameplayStatics::GetWorldDeltaSeconds(World);
        static float CurrentTime = 0.0f;
        CurrentTime += dt;

        // 时间阶段定义 (秒)
        constexpr float DelayDuration = 1.f;        // 初始等待秒
        constexpr float FadeInDuration = 0.8f;      // 半透黑幕与钟表淡入
        constexpr float CoreAnimDuration = 2.5f;    // 钟表核心旋转展示
        constexpr float FadeOutDuration = 0.8f;     // 平滑淡出恢复
        constexpr float TotalDuration = DelayDuration + FadeInDuration + CoreAnimDuration + FadeOutDuration;

        // 延迟阶段：静默等待
        if (CurrentTime < DelayDuration)
            return false;

        // 动画整体结束
        if (CurrentTime >= TotalDuration)
        {
            bFinished = true;
            g_Config::bInitWelcome = true;
            return true;
        }

        Shadow::ShadowDrawList* DrawList = Shadow::GetBackgroundDrawList();
        if (!DrawList)
            return false;

        const float ScreenW = static_cast<float>(Canvas->SizeX);
        const float ScreenH = static_cast<float>(Canvas->SizeY);
        const Shadow::Vec2 Center = { ScreenW * 0.5f, ScreenH * 0.5f };

        // 1. 缓动函数
        auto EaseInOutCubic = [](float t) -> float {
            t = std::clamp(t, 0.0f, 1.0f);
            return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
            };
        auto EaseOutExpo = [](float t) -> float {
            t = std::clamp(t, 0.0f, 1.0f);
            return (t >= 1.0f) ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
            };

        // 2. 动画阶段与透明度计算
        constexpr float MaxBgAlpha = 0.5f; // 半透明背景最大不透明度 (0.0 完全透明 ~ 1.0 完全不透明)

        float BgAlpha = 0.0f;
        float ContentAlpha = 0.0f;
        float ActiveTime = CurrentTime - DelayDuration; // 扣除延迟后的实际动画时间

        if (ActiveTime < FadeInDuration)
        {
            // 淡入阶段
            float t = ActiveTime / FadeInDuration;
            BgAlpha = EaseInOutCubic(t) * MaxBgAlpha;
            ContentAlpha = EaseOutExpo(t);
        }
        else if (ActiveTime < (FadeInDuration + CoreAnimDuration))
        {
            // 核心展示阶段
            BgAlpha = MaxBgAlpha;
            ContentAlpha = 1.0f;
        }
        else
        {
            // 淡出阶段
            float t = (ActiveTime - (FadeInDuration + CoreAnimDuration)) / FadeOutDuration;
            BgAlpha = (1.0f - EaseInOutCubic(t)) * MaxBgAlpha;
            ContentAlpha = 1.0f - EaseInOutCubic(t);
        }

        BgAlpha = std::clamp(BgAlpha, 0.0f, MaxBgAlpha);
        ContentAlpha = std::clamp(ContentAlpha, 0.0f, 1.0f);

        // 颜色工具
        auto MakeCol = [](float r, float g, float b, float a) -> Shadow::Color {
            return Shadow::Color(
                std::clamp(r, 0.0f, 1.0f),
                std::clamp(g, 0.0f, 1.0f),
                std::clamp(b, 0.0f, 1.0f),
                std::clamp(a, 0.0f, 1.0f)
            );
            };

        // 配色定义：
        Shadow::Color TranslucentBg = MakeCol(0.f, 0.01f, 0.02f, BgAlpha);             // 半透明深邃暗底
        Shadow::Color CyanGlow = MakeCol(0.00f, 0.88f, 1.00f, ContentAlpha * 0.90f);     // 科技青
        Shadow::Color PurpleGlow = MakeCol(0.60f, 0.35f, 1.00f, ContentAlpha * 0.80f);   // 霓虹紫
        Shadow::Color PureWhite = MakeCol(1.00f, 1.00f, 1.00f, ContentAlpha * 0.95f);    // 亮白刻度
        Shadow::Color FaintRing = MakeCol(0.00f, 0.88f, 1.00f, ContentAlpha * 0.20f);    // 微弱外轨道底圈

        // 3. 绘制全屏半透明遮罩
        DrawList->AddRectFilled({ 0.0f, 0.0f }, { ScreenW, ScreenH }, TranslucentBg);

        if (ContentAlpha <= 0.005f)
            return false;

        // 4. 几何辅助绘制
        auto DrawRing = [&](Shadow::Vec2 center, float radius, float startAngle, float sweepAngle, int segments, Shadow::Color col, float thickness) {
            float step = sweepAngle / static_cast<float>(segments);
            for (int i = 0; i < segments; ++i)
            {
                float a1 = startAngle + i * step;
                float a2 = startAngle + (i + 1) * step;
                Shadow::Vec2 p1 = { center.x + radius * std::cos(a1), center.y + radius * std::sin(a1) };
                Shadow::Vec2 p2 = { center.x + radius * std::cos(a2), center.y + radius * std::sin(a2) };
                DrawList->AddLine(p1, p2, col, thickness);
            }
            };

        constexpr float PI = std::numbers::pi_v<float>;
        const float AnimTime = ActiveTime;

        // 5. 纯钟表科技动画
        // [1] 底层微弱完整细圆环轨道
        DrawRing(Center, 68.0f, 0.0f, 2.0f * PI, 48, FaintRing, 1.0f);

        // [2] 顺时针旋转三段式圆弧
        float InnerRadius = 42.0f + 2.0f * std::sin(AnimTime * 3.0f);
        float RotSpeed1 = AnimTime * 1.6f;
        for (int i = 0; i < 3; ++i)
        {
            float baseAngle = RotSpeed1 + i * (2.0f * PI / 3.0f);
            DrawRing(Center, InnerRadius, baseAngle, PI * 0.45f, 24, CyanGlow, 2.0f);
        }

        // [3] 逆时针旋转 16 周期钟表刻度环
        float OuterRadius = 68.0f;
        float RotSpeed2 = -AnimTime * 0.7f;
        constexpr int NumTicks = 16;
        for (int i = 0; i < NumTicks; ++i)
        {
            float angle = RotSpeed2 + i * (2.0f * PI / NumTicks);
            float r1 = OuterRadius;
            float r2 = OuterRadius + ((i % 4 == 0) ? 8.0f : 4.0f); // 主刻度较长
            Shadow::Vec2 p1 = { Center.x + r1 * std::cos(angle), Center.y + r1 * std::sin(angle) };
            Shadow::Vec2 p2 = { Center.x + r2 * std::cos(angle), Center.y + r2 * std::sin(angle) };
            DrawList->AddLine(p1, p2, (i % 4 == 0) ? PureWhite : PurpleGlow, (i % 4 == 0) ? 2.0f : 1.2f);
        }

        // [4] 中心呼吸菱形与流光核心
        float CorePulse = 6.0f + 2.0f * std::sin(AnimTime * 4.0f);
        Shadow::Vec2 Top = { Center.x, Center.y - CorePulse };
        Shadow::Vec2 Bottom = { Center.x, Center.y + CorePulse };
        Shadow::Vec2 Left = { Center.x - CorePulse, Center.y };
        Shadow::Vec2 Right = { Center.x + CorePulse, Center.y };

        DrawList->AddLine(Top, Right, PureWhite, 1.8f);
        DrawList->AddLine(Right, Bottom, PureWhite, 1.8f);
        DrawList->AddLine(Bottom, Left, PureWhite, 1.8f);
        DrawList->AddLine(Left, Top, PureWhite, 1.8f);
        DrawList->AddRectFilled({ Center.x - 1.5f, Center.y - 1.5f }, { 3.0f, 3.0f }, CyanGlow);

        return false;
    }

    __forceinline SDK::AShooterGameState* GetAShooterGameState() {
        SDK::AShooterGameState* GS = nullptr;
        SDK::UWorld* World = SDK::UWorld::GetWorld();

        if (World && World->GameState && World->GameState->IsA(SDK::AShooterGameState::StaticClass())) {
            GS = static_cast<SDK::AShooterGameState*>(World->GameState);
        }

        return GS;
    }

    __forceinline float GetClientPing() {
        float ping = 0.f;

        SDK::APlayerController* LocalPC = g_Util::GetLocalPC();
        if (LocalPC && LocalPC->PlayerState) {
            ping = LocalPC->PlayerState->GetExactPing();
        }

        return ping;
    }

    __forceinline float GetServerFPS() {
        SDK::AShooterGameState* GS = g_Util::GetAShooterGameState();
        if (GS) {
            return GS->ServerFramerate;
        }

        return 0.f;
    }

    __forceinline SDK::UFont* GetOpenSansRegular12() {
        static SDK::UFont* OpenSansRegular12 = nullptr;

        if (!OpenSansRegular12) {
            SDK::UObject* _Font = SDK::UObject::FindObject(_XOR_("Font OpenSansRegular12.OpenSansRegular12").str());
            if (_Font && _Font->IsA(SDK::UFont::StaticClass())) OpenSansRegular12 = (SDK::UFont*)_Font;
        }

        return OpenSansRegular12;
    }

    __forceinline SDK::UFont* GetSansationBold18() {
        static SDK::UFont* SansationBold18 = nullptr;

        if (!SansationBold18) {
            SDK::UObject* _Font = SDK::UObject::FindObject(_XOR_("Font SansationBold18.SansationBold18").str());
            if (_Font && _Font->IsA(SDK::UFont::StaticClass())) SansationBold18 = (SDK::UFont*)_Font;
        }

        return SansationBold18;
    }

    __forceinline float GetFPS() {
        static float time_accumulator = 0.f; // 累积时间 (秒)
        static int   frame_count = 0;        // 累积渲染帧数
        static float last_fps = 0.f;         // 上一次计算并缓存的 FPS

        float dt = Shadow::GetIO().DeltaTime;

        // 累加时间和帧数
        time_accumulator += dt;
        frame_count++;

        // 达到 500ms (0.5秒) 更新一次
        if (time_accumulator >= 0.5f) {
            last_fps = (time_accumulator > 0.f) ? (static_cast<float>(frame_count) / time_accumulator) : 0.f;

            // 重置计数器
            time_accumulator = 0.f;
            frame_count = 0;
        }

        return last_fps;
    }

    __forceinline void DropOrUseItem() {
        SDK::AShooterPlayerController* LocalPC = static_cast<SDK::AShooterPlayerController*>(g_Util::GetLocalPC());
        if (LocalPC && LocalPC->Pawn && LocalPC->Pawn->IsA(SDK::APrimalCharacter::StaticClass())) {
            SDK::APrimalCharacter* LocalCharacter = static_cast<SDK::APrimalCharacter*>(LocalPC->Pawn);

            if (LocalCharacter) {
                SDK::UPrimalInventoryComponent* LocalInventory = LocalCharacter->MyInventoryComponent;

                if (LocalInventory) {
                    // 执行丢弃逻辑
                    if (g_Config::bDropItem) {
                        LocalPC->ServerDropFromRemoteInventory(LocalInventory, g_Config::dropItemID);
                        // 重置状态
                        g_Config::bDropItem = false;
                    }

                    // 执行使用逻辑
                    if (g_Config::bUseItem) {
                        // 执行操作
                        LocalPC->ServerRequestInventoryUseItem(LocalInventory, g_Config::useItemID, g_Config::useItemSlotID);

                        // 重置状态
                        g_Config::bUseItem = false;
                        // SDK::FItemNetID 通常是结构体，不需要像字符串一样手动清理，下次会被覆盖
                    }
                }
            }
        }
    }

    __forceinline void ADD_MH(std::string pattern, bool* hookOK, void* hk, void* o) {
        AOB::Result ok = AOB::Scan(pattern);

        if (ok && ok.size() == 1) {
            void* targetAddr = ok[0];

            if (MH_CreateHook(targetAddr, hk, reinterpret_cast<LPVOID*>(o)) == MH_OK) {
                MH_EnableHook(targetAddr);
                *hookOK = true;
            }
        }
    }

    __forceinline void ADD_MH(void* targetAddr, bool* hookOK, void* hk, void* o) {
        if (targetAddr) {
            if (MH_CreateHook(targetAddr, hk, reinterpret_cast<LPVOID*>(o)) == MH_OK) {
                MH_EnableHook(targetAddr);
                *hookOK = true;
            }
        }
    }

    static __forceinline bool HasCommandLineArg(std::wstring targetArg) noexcept {
        struct CmdCache {
            int argc = 0;
            LPWSTR* argv = nullptr;

            CmdCache() noexcept {
                argv = CommandLineToArgvW(GetCommandLineW(), &argc);
            }
        };

        static const CmdCache cache;

        if (cache.argv == nullptr) {
            return false;
        }

        const size_t targetLen = targetArg.size();

        for (int i = 0; i < cache.argc; ++i) {
            const wchar_t* currentArg = cache.argv[i];
            if (currentArg == nullptr) continue;

            if (wcslen(currentArg) != targetLen) continue;

            bool isMatch = true;
            for (size_t j = 0; j < targetLen; ++j) {
                if (towlower(targetArg[j]) != towlower(currentArg[j])) {
                    isMatch = false;
                    break;
                }
            }

            if (isMatch) {
                return true;
            }
        }

        return false;
    }
}