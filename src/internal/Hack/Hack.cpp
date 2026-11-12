#pragma once
#include "../Config/Configs.h"
#include "../Util/Util.h"
#include "../Log/LogManager.h"
#include "../CheatData/CheatCache.hpp"
#include "../../XorStr.h"
#include "Hack.h"

namespace g_Hack {
    static int32_t g_CurrentNoteIndex = 0;
    static const int32_t g_MaxNoteIndex = 1000;

    void DumpServerInfo() {
        SDK::UWorld* World = CheatCache::Tick::World;
        if (!World || !World->NetDriver || !World->NetDriver->ServerConnection) {
            g_LogManager::AddLog(255, 50, 55, 255, _XOR_("当前不在服务器内").str());
            return;
        }

        SDK::UNetConnection* ServerConnection = World->NetDriver->ServerConnection;

        // g_LogManager::AddLog(255, 200, 255, 255, std::format("[{}] [{}]", ServerConnection->GetFirstIP(), ServerConnection->GetPort()));
    }

    void Suicide(SDK::UWorld* World) {
        if (!World) return;
        SDK::AShooterPlayerController* PC = CheatCache::Tick::LocalSPC;
        if (!PC || !PC->Character) return;

        SDK::AShooterCharacter* TargetHuman = nullptr;

        // 情况 1：当前控制的是人类本身
        if (PC->Character->IsA(SDK::AShooterCharacter::StaticClass()) && !PC->Character->IsA(SDK::APrimalDinoCharacter::StaticClass())) {
            TargetHuman = static_cast<SDK::AShooterCharacter*>(PC->Character);
        }
        // 情况 2：当前控制的是恐龙，从 Rider 弱指针中提取人类
        else if (PC->Character->IsA(SDK::APrimalDinoCharacter::StaticClass())) {
            SDK::APrimalDinoCharacter* Dino = static_cast<SDK::APrimalDinoCharacter*>(PC->Character);

            // 检查弱指针是否有效，并转换为强指针（裸指针）
            if (Dino) {
                TargetHuman = Dino->Rider.Get();
            }
        }

        // 统一执行自杀逻辑
        if (TargetHuman) {
            TargetHuman->BPSuicide();
        }
    }

    void UnlockExplorerNotes(SDK::UWorld* World) {
        if (!World) return;

        SDK::AShooterPlayerController* PC = CheatCache::Tick::LocalSPC;
        if (!PC)  return;

        if (g_CurrentNoteIndex <= g_MaxNoteIndex) {
            PC->UnlockExplorerNote(g_CurrentNoteIndex, true, true);
            g_CurrentNoteIndex++;
        }
        else {
            g_CurrentNoteIndex = 0;
        }
    }

    void AutoFeed(SDK::UWorld* World) {
        if (!World) return;

        SDK::AShooterPlayerController* PC = CheatCache::Tick::LocalSPC;
        if (!PC)  return;

        SDK::APrimalDinoCharacter* TargetDino = nullptr;

        if (PC->Pawn && PC->Pawn->IsA(SDK::APrimalDinoCharacter::StaticClass())) {
            TargetDino = static_cast<SDK::APrimalDinoCharacter*>(PC->Pawn);
        }

        else if (PC->Character && PC->Character->IsA(SDK::AShooterCharacter::StaticClass())) {
            SDK::AShooterCharacter* MyHuman = static_cast<SDK::AShooterCharacter*>(PC->Character);

            if (MyHuman) {
                TargetDino = MyHuman->GetRidingDino();
            }
        }

        // 只有找到恐龙时才执行喂食逻辑
        if (TargetDino) {
            if (
                strstr(TargetDino->MyDinoSettingsCDO->DinoFoodTypeName.ToString().c_str(), _XOR_("食肉动物").crypt()) || 
                strstr(TargetDino->MyDinoSettingsCDO->DinoFoodTypeName.ToString().c_str(), _XOR_("Carnivore").crypt())
                ) {

                g_Util::ProcessDinoFeed(PC, TargetDino);
            }
        }
    }

    void SuperFlyer(SDK::UWorld* World) {
        if (!World) return;

        SDK::AShooterPlayerController* PC = CheatCache::Tick::LocalSPC;
        if (!PC)  return;

        SDK::APrimalDinoCharacter* TargetDino = nullptr;

        if (PC->Pawn && PC->Pawn->IsA(SDK::APrimalDinoCharacter::StaticClass())) {
            TargetDino = static_cast<SDK::APrimalDinoCharacter*>(PC->Pawn);

            // 静态变量用于保存状态与备份数据
            static bool bIsBackedUp = false;
            static bool orig_bFlyerDinoAllowBackwardsFlight = false;
            static bool orig_bFlyerDinoAllowStrafing = false;

            // 当功能关闭时，如果之前已备份过，则恢复一次原始属性
            if (!g_Config::bSuperFlyer) {
                if (bIsBackedUp) {
                    TargetDino->bFlyerDinoAllowBackwardsFlight = orig_bFlyerDinoAllowBackwardsFlight;
                    TargetDino->bFlyerDinoAllowStrafing = orig_bFlyerDinoAllowStrafing;
                    bIsBackedUp = false;
                }
                return;
            }

            // 在开启功能前先备份原始状态
            if (!bIsBackedUp) {
                orig_bFlyerDinoAllowBackwardsFlight = TargetDino->bFlyerDinoAllowBackwardsFlight;
                orig_bFlyerDinoAllowStrafing = TargetDino->bFlyerDinoAllowStrafing;
                bIsBackedUp = true;
            }

            // 允许恐龙左右飞，倒飞
            TargetDino->bFlyerDinoAllowBackwardsFlight = true;
            TargetDino->bFlyerDinoAllowStrafing = true;
        }
    }

    void ForceTurn(SDK::UMovementComponent* rcx, float DeltaTime) {
        SDK::AShooterPlayerController* LocalPC = CheatCache::PhysicsRotation::LocalSPC;
        if (!LocalPC || !LocalPC->Pawn || !LocalPC->PlayerCameraManager) return;

        // 转换为 UCharacterMovementComponent 以访问其属性
        SDK::UCharacterMovementComponent* moveComp = static_cast<SDK::UCharacterMovementComponent*>(rcx);
        if (!moveComp) return;
        if (!moveComp->UpdatedComponent) return;

        // 只为玩家骑乘的恐龙启用
        SDK::APrimalDinoCharacter* DinoCharacter = static_cast<SDK::APrimalDinoCharacter*>(LocalPC->Pawn);
        if (!DinoCharacter || !DinoCharacter->IsA(SDK::APrimalDinoCharacter::StaticClass())) return;

        // 由于此函数是共用的，所以必须过滤掉除player与riding外的movement。最好的方法是每帧检查并hook虚函数
        // 可以增加一个过滤，只为恐龙启用
        if (DinoCharacter->CharacterMovement != rcx) {
            return;
        }

        // 静态变量用于保存状态与备份数据
        static bool bIsBackedUp = false;
        static bool orig_bOrientRotationToMovement = false;
        static bool orig_bUseControllerDesiredRotation = false;

        // 当功能关闭时，如果之前已备份过，则恢复一次原始属性
        if (!g_Config::bForceTurn) {
            if (bIsBackedUp) {
                moveComp->bOrientRotationToMovement = orig_bOrientRotationToMovement;
                moveComp->bUseControllerDesiredRotation = orig_bUseControllerDesiredRotation;
                bIsBackedUp = false;
            }
            return;
        }

        // 在开启功能前先备份原始状态
        if (!bIsBackedUp) {
            orig_bOrientRotationToMovement = moveComp->bOrientRotationToMovement;
            orig_bUseControllerDesiredRotation = moveComp->bUseControllerDesiredRotation;
            bIsBackedUp = true;
        }

        // 禁用所有自动旋转机制
        moveComp->bOrientRotationToMovement = false;
        moveComp->bUseControllerDesiredRotation = false;

        SDK::FRotator rot = LocalPC->PlayerCameraManager->GetCameraRotation();

        // 保存当前速度
        SDK::FVector oldVelocity = moveComp->Velocity;

        // 执行旋转（强制更新世界旋转和相对旋转）
        moveComp->UpdatedComponent->K2_SetWorldRotation(rot, false, nullptr, false);
        moveComp->UpdatedComponent->RelativeRotation = rot;

        // 恢复速度（消除旋转引起的任何速度变化）
        moveComp->Velocity = oldVelocity;
    }

    void DamageLog(SDK::AActor* _this, float DamageAmount, SDK::FDamageEvent* DamageEvent, SDK::AController* Instigator, SDK::AActor* DamageCauser) {
        SDK::AShooterPlayerController* PC = CheatCache::Tick::LocalSPC;
        if (!PC)  return;

        SDK::APawn* LPawn = PC->Pawn;
        if (!LPawn || DamageCauser != LPawn || _this == LPawn) return;

        std::string targetName = "";
        std::string causerName = "";
        float rhp = 0.f;

        // 1. 处理受击者 (Target)
        if (_this->IsA(SDK::AShooterCharacter::StaticClass())) {
            rhp = static_cast<SDK::AShooterCharacter*>(_this)->GetHealth() - DamageAmount;
            targetName = static_cast<SDK::AShooterCharacter*>(_this)->PlayerName.ToString();
        }
        else if (_this->IsA(SDK::APrimalDinoCharacter::StaticClass())) {
            rhp = static_cast<SDK::APrimalDinoCharacter*>(_this)->GetHealth() - DamageAmount;
            targetName = static_cast<SDK::APrimalDinoCharacter*>(_this)->DescriptiveName.ToString();
        }
        else if (_this->IsA(SDK::APrimalStructure::StaticClass())) {
            rhp = static_cast<SDK::APrimalStructure*>(_this)->Health - DamageAmount;
            targetName = static_cast<SDK::APrimalStructure*>(_this)->GetDescriptiveName().ToString();
        }
        else {
            return;
            // 这种情况一般是树之类的，不确定还有没有其他类型的实体
            // targetName = _this->Class->GetName();
        }

        if (rhp < 0) rhp = 0;

        // 2. 处理攻击者 (Causer)
        if (DamageCauser->IsA(SDK::AShooterCharacter::StaticClass())) {
            causerName = static_cast<SDK::AShooterCharacter*>(DamageCauser)->PlayerName.ToString();
        }
        else if (DamageCauser->IsA(SDK::APrimalDinoCharacter::StaticClass())) {
            causerName = static_cast<SDK::APrimalDinoCharacter*>(DamageCauser)->DescriptiveName.ToString();
        }
        else if (DamageCauser->IsA(SDK::APrimalStructure::StaticClass())) {
            causerName = static_cast<SDK::APrimalStructure*>(DamageCauser)->GetDescriptiveName().ToString();
        }
        else {
            causerName = DamageCauser->Class->GetName();
        }

        // 3. 打印日志
        std::string logMsg = std::format("{} -> {} -> {:.0f} damage ({:.0f} remaining)",
            causerName,
            targetName,
            DamageAmount,
            rhp
        );

        g_LogManager::AddLog(
            g_Config::LogDamageColor[0] * 255,
            g_Config::LogDamageColor[1] * 255,
            g_Config::LogDamageColor[2] * 255,
            g_Config::LogDamageColor[3] * 255,
            logMsg
        );
    }

    void AutoSwapBrokenEquipment()
    {
        static int globalTick = 0;
        globalTick++;

        if (globalTick % 3 != 0) return;

        SDK::AShooterPlayerController* PC = CheatCache::Tick::LocalSPC;
        if (!PC || !PC->Pawn) return;

        SDK::APrimalCharacter* Character = static_cast<SDK::APrimalCharacter*>(PC->Pawn);
        if (!Character || !Character->MyInventoryComponent) return;

        SDK::UPrimalInventoryComponent* Inv = Character->MyInventoryComponent;
        auto& EquippedItems = Inv->EquippedItems;
        auto& InventoryItems = Inv->InventoryItems;

        if (InventoryItems.Num() == 0) return;

        static int lastSwapTick[25] = { 0 };
        const float ALERT_DURABILITY = g_Config::ArmorRange;

        //获取当前穿着的装备
        SDK::UPrimalItem* WornArmor[20] = { nullptr };
        for (int i = 0; i < EquippedItems.Num(); i++)
        {
            auto item = EquippedItems[i];
            if (!item) continue;

            int typeIndex = (int)item->MyEquipmentType;
            if (typeIndex >= 0 && typeIndex < 20)
            {
                WornArmor[typeIndex] = item;
            }
        }

        int targetSlots[] = { 0, 1, 2, 3, 4, 8 };
        bool bSlotNeedsReplace[25] = { false }; // 记录需换甲
        bool bAnyNeedsReplace = false;          // 是否需要换

        for (int i = 0; i < 6; i++)
        {
            int typeIndex = targetSlots[i];

            // 如果在冷却中，直接跳过，本帧不处理它
            if (globalTick - lastSwapTick[typeIndex] < 60) continue;

            auto CurrentEquippedItem = WornArmor[typeIndex];

            if (CurrentEquippedItem == nullptr)
            {
                // 空槽位，需要穿
                bSlotNeedsReplace[typeIndex] = true;
                bAnyNeedsReplace = true;
            }
            else
            {
                // 有装备，判断是否快碎了
                if (!CurrentEquippedItem->bIsItemSkin && CurrentEquippedItem->bUseItemDurability)
                {
                    if (CurrentEquippedItem->ItemDurability <= ALERT_DURABILITY)
                    {
                        bSlotNeedsReplace[typeIndex] = true;
                        bAnyNeedsReplace = true;
                    }
                }
            }
        }

        //如果没有需要换的 跳过
        if (!bAnyNeedsReplace) return;

        float bestDurabilityForSlot[25] = { -1.0f };         // 找到的最高耐久
        SDK::FItemNetID* bestItemNetIdForSlot[25] = { nullptr }; // 记录最高耐久装备的ID

        // 只遍历一次大背包
        for (int j = 0; j < InventoryItems.Num(); j++)
        {
            auto CandItem = InventoryItems[j];
            if (!CandItem) continue;

            int typeIndex = (int)CandItem->MyEquipmentType;

            if (typeIndex >= 0 && typeIndex < 25 && bSlotNeedsReplace[typeIndex])
            {
                if (CandItem->bIsItemSkin || !CandItem->bUseItemDurability) continue;
                if (CandItem->bIsBlueprint || CandItem->bIsEngram) continue;

                float candDur = CandItem->ItemDurability;
                if (candDur > ALERT_DURABILITY && candDur > bestDurabilityForSlot[typeIndex])
                {
                    bestDurabilityForSlot[typeIndex] = candDur;
                    bestItemNetIdForSlot[typeIndex] = &CandItem->ItemID;
                }
            }
        }

        for (int i = 0; i < 6; i++)
        {
            int typeIndex = targetSlots[i];

            if (bSlotNeedsReplace[typeIndex])
            {
                if (bestItemNetIdForSlot[typeIndex] != nullptr)
                {
                    PC->ServerEquipPawnItem(*bestItemNetIdForSlot[typeIndex]);
                    lastSwapTick[typeIndex] = globalTick; 
                }
                else
                {
                    lastSwapTick[typeIndex] = globalTick;
                }
            }
        }
    }

    void OutBody()
    {
        SDK::AShooterPlayerController* LocalPC = CheatCache::Tick::LocalSPC;
        if (!LocalPC) return;

        SDK::ACharacter* Character = LocalPC->Character;
        if (!Character) return;

        SDK::UCharacterMovementComponent* MovementComp = Character->CharacterMovement;
        if (!MovementComp) return;

        static bool bLastGhostState = false;
        bool bCurrentGhostState = g_Config::bOutBodyChecked && g_MDX12::g_MenuState::g_isOutBodyActive;

        static SDK::FVector SavedRealLocation = { 0.0f, 0.0f, 0.0f };

        if (bCurrentGhostState == true && bLastGhostState == false)
        {
            SavedRealLocation = Character->K2_GetActorLocation();

            Character->SetReplicateMovement(false);
            Character->SetActorEnableCollision(false);

            MovementComp->MaxFlySpeed = 10000.f;
            MovementComp->MaxAcceleration = 5000.f;
            MovementComp->BrakingDecelerationFlying = 9999.f;
            MovementComp->SetMovementMode(SDK::EMovementMode::MOVE_Flying, 0);

            bLastGhostState = true;
        }

        else if (bCurrentGhostState == false && bLastGhostState == true)
        {
            Character->K2_SetActorLocation(SavedRealLocation, false, nullptr, true);

            MovementComp->SetMovementMode(SDK::EMovementMode::MOVE_Walking, 0);
            Character->SetActorEnableCollision(true);

            Character->SetReplicateMovement(true);

            bLastGhostState = false;
        }

        if (bCurrentGhostState == true && bLastGhostState == true)
        {
            if (MovementComp->MovementMode != SDK::EMovementMode::MOVE_Flying)
            {
                MovementComp->SetMovementMode(SDK::EMovementMode::MOVE_Flying, 0);
            }
        }
    }
}