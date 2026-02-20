#include "../Minimal-D3D12-Hook-ImGui/Main/mdx12_api.h"
#include "SDK_Headers.hpp"
#include "Aimbot.h"
#include "Configs.h"
#include "ESP.h"
#include "Util.h"
#include <chrono>

namespace g_Aimbot {
    static bool bIsAutoFiring = false;

    void DrawStatusText(TargetInfo& best) {
        SDK::APlayerController* LocalPC = g_Util::GetLocalPC();
        if (!LocalPC || !best.bIsValid || !best.Character) return;

        SDK::FVector2D ScreenPos;
        if (LocalPC->ProjectWorldLocationToScreen(best.BestComponentLocation, &ScreenPos, false)) {
            ImColor textColor = best.bIsTriggering ? ImColor(255, 0, 0) : (best.bIsLocked ? ImColor(0, 255, 0) : ImColor(255, 255, 0));

            char buf[128];
            _snprintf_s(buf, sizeof(buf), "BONE: %d | FOV: %.2f | SPEED: %.0f%%", best.BestBoneIndex, best.FovDistance, g_Config::AimbotSmooth);
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(ScreenPos.X, ScreenPos.Y - 40), textColor, buf);

            if (best.bIsTriggering) {
                ImGui::GetBackgroundDrawList()->AddText(ImVec2(ScreenPos.X, ScreenPos.Y - 60), ImColor(255, 0, 0), "!!! TRIGGER ACTIVE !!!");
                ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(ScreenPos.X, ScreenPos.Y), 10.0f, ImColor(255, 0, 0), 12, 2.0f);
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
                ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(sPos.X, sPos.Y), 0.5f, g_Util::GetU32Color(g_Config::AimPointsColor));

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
        if (!LocalPC || !LocalPC->Pawn || !LocalPC->PlayerCameraManager) return Best;

        SDK::FVector CamLoc; SDK::FRotator CamRot;
        LocalPC->GetPlayerViewPoint(&CamLoc, &CamRot);

        auto& Actors = World->PersistentLevel->Actors;
        if (Actors.Num() <= 0) return Best;
        for (int i = 0; i < Actors.Num(); i++) {
            SDK::AActor* Actor = Actors[i];
            if (!Actor || !Actor->Class || Actor == LocalPC->Pawn || Actor->bHidden || !Actor->IsA(SDK::APrimalCharacter::StaticClass())) continue;

            SDK::APrimalCharacter* Char = (SDK::APrimalCharacter*)Actor;
            if (!Char->Mesh) continue;
            if (Char->IsDead() || g_ESP::GetRelation(Char, (SDK::APrimalCharacter*)LocalPC->Pawn) == g_ESP::RelationType::Team) continue;

            int BoneCount = Char->Mesh->GetNumBones();
            for (int j = 0; j < BoneCount; j++) {
                SDK::FVector BoneLoc = Char->Mesh->GetSocketLocation(Char->Mesh->GetBoneName(j));
                if (BoneLoc.IsZero()) continue;

                float Angle = GetAngleDistance(CamLoc, BoneLoc, CamRot);
                if (Angle < g_Config::AimbotFOV) {
                    if (LocalPC->LineOfSightTo(Char, { 0,0,0 }, false)) {
                        if (Angle < Best.FovDistance) {
                            Best.Character = Char;
                            Best.BestComponentLocation = BoneLoc;
                            Best.FovDistance = Angle;
                            Best.BestBoneIndex = j;
                            Best.bIsValid = true;
                            Best.bIsLocked = (Angle < 0.5f);
                        }
                    }
                }
            }
        }
        return Best;
    }

    void Tick() {
        if (!g_Config::bAimbotEnabled && !g_Config::bTriggerbotEnabled) return;

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (!World || !World->PersistentLevel) {
            if (bIsAutoFiring) bIsAutoFiring = false;
            return;
        }

        SDK::APlayerController* LocalPC = g_Util::GetLocalPC();
        if (!LocalPC || !LocalPC->Pawn || !LocalPC->PlayerCameraManager) return;

        SDK::AShooterPlayerController* ShooterPC = (SDK::AShooterPlayerController*)LocalPC;
        if (!ShooterPC) return;

        SDK::AShooterCharacter* MyChar = (SDK::AShooterCharacter*)ShooterPC->Pawn;
        if (!MyChar) return;

        SDK::AShooterWeapon* MyWeapon = MyChar->CurrentWeapon;
        if (!MyWeapon || MyWeapon->GetAmmoReloadState() != SDK::EWeaponAmmoReloadState::Ready || MyWeapon->GetCurrentAmmo() <= 0) return;

        TargetInfo Best = GetBestTarget();

        if (g_Config::bDrawAimPoints && Best.bIsValid) {
            VisualizeTargetBones(Best.Character, LocalPC);
        }

        if (g_Config::bAimbotEnabled && Best.Character && Best.bIsValid) {
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

        if (g_Config::bTriggerbotEnabled && Best.Character && Best.bIsValid) {
            if (MyChar && MyChar->CurrentWeapon) {
                if (Best.Character && Best.bIsValid && Best.bIsLocked) {
                    g_Util::MimicMouseClick(true);
                    bIsAutoFiring = true;
                    Best.bIsTriggering = true;
                }
                else {
                    if (bIsAutoFiring && ShooterPC) {
                        g_Util::MimicMouseClick(false);
                        bIsAutoFiring = false;
                    }
                }
            }
        }
        else if (bIsAutoFiring) {
            if (LocalPC && LocalPC->IsA(SDK::AShooterPlayerController::StaticClass())) {
                g_Util::MimicMouseClick(false);
            }
            bIsAutoFiring = false;
        }

        DrawStatusText(Best);
    }
}