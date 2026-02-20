#include "../Minimal-D3D12-Hook-ImGui/Main/mdx12_api.h"
#include "SDK_Headers.hpp"
#include "Aimbot.h"
#include "Configs.h"
#include "ESP.h"
#include "Util.h"
#include <chrono>
#include <float.h>

namespace g_Aimbot {
    static bool bIsAutoFiring = false;

    void DrawStatusText(TargetInfo& best) {
        SDK::APlayerController* LocalPC = g_Util::GetLocalPC();
        if (!LocalPC || !best.bIsValid || !best.Character) return;

        SDK::FVector2D ScreenPos;
        if (LocalPC->ProjectWorldLocationToScreen(best.BestComponentLocation, &ScreenPos, false)) {
            ImColor textColor = best.bIsTriggering ? ImColor(255, 0, 0) : (best.bIsLocked ? ImColor(0, 255, 0) : ImColor(255, 255, 0));

            char buf[128];
            _snprintf_s(buf, sizeof(buf), "HP: %.0f | DIST: %.0fm | LCK: %s",
                best.Health, best.Distance / 100.0f, best.bIsLocked ? "YES" : "NO");

            ImGui::GetBackgroundDrawList()->AddText(ImVec2(ScreenPos.X, ScreenPos.Y - 40), textColor, buf);

            if (best.bIsTriggering) {
                ImGui::GetBackgroundDrawList()->AddText(ImVec2(ScreenPos.X, ScreenPos.Y - 60), ImColor(255, 0, 0), ">>> SHOOTING <<<");
            }
        }
    }

    void VisualizeTargetBones(SDK::APrimalCharacter* Char, SDK::APlayerController* PC) {
        if (!Char || !Char->Mesh || !PC) return;

        int BoneCount = Char->Mesh->GetNumBones();
        SDK::FVector2D lastPos = { 0, 0 };

        for (int i = 0; i < BoneCount; i++) {
            SDK::FVector BoneLoc = Char->Mesh->GetSocketLocation(Char->Mesh->GetBoneName(i));
            SDK::FVector2D sPos;

            if (PC->ProjectWorldLocationToScreen(BoneLoc, &sPos, false)) {
                // 绘制骨骼点
                ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(sPos.X, sPos.Y), 1.2f, g_Util::GetU32Color(g_Config::AimPointsColor));

                // 绘制连接线 (保持颜色独立)
                if (lastPos.X != 0 && lastPos.Y != 0) {
                    ImGui::GetBackgroundDrawList()->AddLine(ImVec2(lastPos.X, lastPos.Y), ImVec2(sPos.X, sPos.Y), g_Util::GetU32Color(g_Config::AimSkeletonColor));
                }
                lastPos = sPos;
            }
        }
    }

    float GetAngleDistance(SDK::FVector CamLoc, SDK::FVector TargetLoc, SDK::FRotator CamRot) {
        SDK::FVector Diff = { TargetLoc.X - CamLoc.X, TargetLoc.Y - CamLoc.Y, TargetLoc.Z - CamLoc.Z };
        SDK::FVector DirToTarget = SDK::UKismetMathLibrary::Normal(Diff, 0.0001f);
        SDK::FVector CamForward = SDK::UKismetMathLibrary::GetForwardVector(CamRot);
        float Dot = SDK::UKismetMathLibrary::Dot_VectorVector(DirToTarget, CamForward);
        Dot = SDK::UKismetMathLibrary::FClamp(Dot, -1.0f, 1.0f);
        return SDK::UKismetMathLibrary::DegAcos(Dot);
    }

    TargetInfo GetBestTarget() {
        TargetInfo Best;
        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (!World || !World->PersistentLevel) return Best;

        SDK::APlayerController* LocalPC = g_Util::GetLocalPC();
        if (!LocalPC || !LocalPC->Pawn) return Best;

        SDK::FVector CamLoc; SDK::FRotator CamRot;
        LocalPC->GetPlayerViewPoint(&CamLoc, &CamRot);

        auto& Actors = World->PersistentLevel->Actors;

        SDK::APrimalCharacter* BestChar = nullptr;
        float MinDistance = FLT_MAX;
        float MinHealth = FLT_MAX;
        float MinAngle = FLT_MAX;

        for (int i = 0; i < Actors.Num(); i++) {
            SDK::AActor* Actor = Actors[i];
            if (!Actor || Actor == LocalPC->Pawn || Actor->bHidden || !Actor->IsA(SDK::APrimalCharacter::StaticClass())) continue;

            SDK::APrimalCharacter* Char = (SDK::APrimalCharacter*)Actor;
            if (Char->IsDead() || g_ESP::GetRelation(Char, (SDK::APrimalCharacter*)LocalPC->Pawn) == g_ESP::RelationType::Team) continue;

            SDK::FVector ActorLoc = Char->K2_GetActorLocation();
            float Dist = SDK::UKismetMathLibrary::Vector_Distance(CamLoc, ActorLoc);
            float Angle = GetAngleDistance(CamLoc, ActorLoc, CamRot);

            // FOV 过滤
            if (Angle > g_Config::AimbotFOV) continue;

            // 视线检查
            if (!LocalPC->LineOfSightTo(Char, { 0,0,0 }, false)) continue;

            // 核心逻辑：距离最近 > 血量最低 > 准星最近
            bool bIsBetter = false;
            if (Dist < MinDistance - 150.0f) {
                bIsBetter = true;
            }
            else if (Dist <= MinDistance + 150.0f) {
                if (Char->GetHealth() < MinHealth - 1.0f) {
                    bIsBetter = true;
                }
                else if (abs(Char->GetHealth() - MinHealth) < 1.0f) {
                    if (Angle < MinAngle) bIsBetter = true;
                }
            }

            if (bIsBetter) {
                MinDistance = Dist;
                MinHealth = Char->GetHealth();
                MinAngle = Angle;
                BestChar = Char;
            }
        }

        // 处理最终目标
        if (BestChar && BestChar->Mesh) {
            SDK::USkeletalMeshComponent* Mesh = BestChar->Mesh;
            int BoneCount = Mesh->GetNumBones();
            SDK::FVector MinB = { FLT_MAX, FLT_MAX, FLT_MAX };
            SDK::FVector MaxB = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

            // 构建骨骼 AABB
            for (int j = 0; j < BoneCount; j++) {
                SDK::FVector BoneLoc = Mesh->GetSocketLocation(Mesh->GetBoneName(j));
                if (BoneLoc.IsZero()) continue;
                MinB.X = min(MinB.X, BoneLoc.X); MinB.Y = min(MinB.Y, BoneLoc.Y); MinB.Z = min(MinB.Z, BoneLoc.Z);
                MaxB.X = max(MaxB.X, BoneLoc.X); MaxB.Y = max(MaxB.Y, BoneLoc.Y); MaxB.Z = max(MaxB.Z, BoneLoc.Z);
            }

            Best.Character = BestChar;
            Best.BestComponentLocation = (MinB + MaxB) / 2.0f;

            // 重新计算中心点相对于相机的真实角度，用于触发判定
            Best.FovDistance = GetAngleDistance(CamLoc, Best.BestComponentLocation, CamRot);
            Best.Distance = MinDistance;
            Best.Health = MinHealth;
            Best.bIsValid = true;

            // Triggerbot 判定逻辑优化：
            // 角度阈值随距离动态调整，防止远距离无法触发或近距离太难对准
            float TriggerThreshold = (MinDistance > 5000.0f) ? 0.8f : 1.8f;
            Best.bIsLocked = (Best.FovDistance < TriggerThreshold);
        }

        return Best;
    }

    void Tick() {
        if (!g_Config::bAimbotEnabled && !g_Config::bTriggerbotEnabled) return;

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (!World || !World->PersistentLevel) return;

        SDK::APlayerController* LocalPC = g_Util::GetLocalPC();
        if (!LocalPC || !LocalPC->Pawn) return;

        SDK::AShooterPlayerController* ShooterPC = (SDK::AShooterPlayerController*)LocalPC;
        SDK::AShooterCharacter* MyChar = (SDK::AShooterCharacter*)ShooterPC->Pawn;
        if (!MyChar) return;

        // 武器与弹药状态检查
        SDK::AShooterWeapon* MyWeapon = MyChar->CurrentWeapon;
        if (!MyWeapon || MyWeapon->GetAmmoReloadState() != SDK::EWeaponAmmoReloadState::Ready || MyWeapon->GetCurrentAmmo() <= 0) {
            if (bIsAutoFiring) {
                g_Util::MimicMouseClick(false);
                bIsAutoFiring = false;
            }
            return;
        }

        TargetInfo Best = GetBestTarget();

        // 渲染调试信息
        if (g_Config::bDrawAimPoints && Best.bIsValid) {
            VisualizeTargetBones(Best.Character, LocalPC);
        }

        // Aimbot 锁定逻辑
        if (g_Config::bAimbotEnabled && Best.bIsValid) {
            SDK::FVector CamLoc; SDK::FRotator CamRot;
            LocalPC->GetPlayerViewPoint(&CamLoc, &CamRot);
            SDK::FRotator TargetRot = SDK::UKismetMathLibrary::FindLookAtRotation(CamLoc, Best.BestComponentLocation);

            if (g_Config::AimbotSmooth >= 100.0f) {
                LocalPC->SetControlRotation(TargetRot);
            }
            else {
                SDK::FRotator CurrentRot = LocalPC->ControlRotation;
                float Alpha = SDK::UKismetMathLibrary::FClamp(g_Config::AimbotSmooth / 100.0f, 0.0f, 1.0f);
                SDK::FRotator InterpRot = SDK::UKismetMathLibrary::RLerp(CurrentRot, TargetRot, Alpha, true);
                LocalPC->SetControlRotation(InterpRot);
            }
        }

        // Triggerbot 开火逻辑
        if (g_Config::bTriggerbotEnabled && Best.bIsValid) {
            if (Best.bIsLocked) {
                g_Util::MimicMouseClick(true);
                bIsAutoFiring = true;
                Best.bIsTriggering = true;
            }
            else {
                if (bIsAutoFiring) {
                    g_Util::MimicMouseClick(false);
                    bIsAutoFiring = false;
                }
            }
        }
        else if (bIsAutoFiring) {
            g_Util::MimicMouseClick(false);
            bIsAutoFiring = false;
        }

        DrawStatusText(Best);
    }
}