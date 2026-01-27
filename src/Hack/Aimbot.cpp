#include "../Minimal-D3D12-Hook-ImGui-1.0.2/Main/mdx12_api.h"
#include "SDK_Headers.hpp"

#include "Aimbot.h"
#include "Configs.h"
#include "ESP.h"
#include <chrono>
#include <random>

namespace g_Aimbot {
    SDK::APrimalCharacter* CurrentTarget = nullptr;
    auto LastTriggerTime = std::chrono::steady_clock::now();

    float GetRandomDelay(float BaseMS, float RandomPercent) {
        if (RandomPercent <= 0) return BaseMS;
        static std::default_random_engine e;
        std::uniform_real_distribution<float> d(1.0f - (RandomPercent / 100.0f), 1.0f + (RandomPercent / 100.0f));
        return BaseMS * d(e);
    }

    TargetInfo GetBestTarget() {
        TargetInfo Best;
        SDK::UWorld* World = SDK::UWorld::GetWorld();
        SDK::APlayerController* LocalPC = g_ESP::GetLocalPC();

        if (!World || !LocalPC || !LocalPC->Pawn) {
            return Best;
        }

        if (!LocalPC->PlayerCameraManager) {
            return Best;
        }

        SDK::FVector CameraLoc;
        SDK::FRotator CameraRot;
        LocalPC->GetPlayerViewPoint(&CameraLoc, &CameraRot);

        SDK::TArray<SDK::AActor*> Actors = World->PersistentLevel->Actors;
        for (int i = 0; i < Actors.Num(); i++) {
            SDK::AActor* Actor = Actors[i];
            if (!Actor || Actor == LocalPC->Pawn || Actor->bHidden) continue;

            if (Actor->IsA(SDK::APrimalCharacter::StaticClass())) {
                SDK::APrimalCharacter* Char = (SDK::APrimalCharacter*)Actor;
                if (!Char || Char->IsDead()) continue;
                if (g_ESP::GetRelation(Char, (SDK::APrimalCharacter*)LocalPC->Pawn) == g_ESP::RelationType::Team)
                    continue;

                SDK::ACharacter* CharacterBase = (SDK::ACharacter*)Char;

                if (!CharacterBase || !CharacterBase->Mesh) continue;

                if (!LocalPC->LineOfSightTo(Char, { 0, 0, 0 }, false))
                    continue;

                SDK::FName TargetBone = SDK::UKismetStringLibrary::Conv_StringToName(L"head");
                SDK::FVector TargetPos = CharacterBase->Mesh->GetSocketLocation(TargetBone);

                SDK::FVector2D ScreenPos;
                if (LocalPC->ProjectWorldLocationToScreen(TargetPos, &ScreenPos, false)) {
                    ImVec2 MousePos = ImGui::GetIO().MousePos;
                    float Dist = sqrtf(powf(ScreenPos.X - MousePos.x, 2) + powf(ScreenPos.Y - MousePos.y, 2));

                    if (Dist < g_Config::AimbotFOV && Dist < Best.FovDistance) {
                        Best.Character = Char;
                        Best.BestComponentLocation = TargetPos;
                        Best.FovDistance = Dist;
                        Best.bIsValid = true;
                    }
                }
            }
        }
        return Best;
    }

    bool CalculateSpreadHitChance(SDK::AShooterWeapon* Weapon, SDK::AActor* Target, float RequiredChance) {
        if (!Weapon || !Target) return false;
        float CurrentSpread = Weapon->CurrentFiringSpread;

        float Dist = g_ESP::GetLocalPC()->Pawn->GetDistanceTo(Target);
        float SpreadRadius = Dist * tanf(CurrentSpread);
        float TargetRadius = 45.0f;

        float Chance = (TargetRadius * TargetRadius) / (SpreadRadius * SpreadRadius);
        return (Chance * 100.0f) >= RequiredChance;
    }

    void Tick() {
        static bool bIsAutoFiring = false;
        if (!g_Config::bAimbotEnabled && !g_Config::bTriggerbotEnabled) return;

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (!World || !World->PersistentLevel) return;

        SDK::APlayerController* LocalPC = g_ESP::GetLocalPC();
        if (!LocalPC || !LocalPC->Class || !LocalPC->PlayerCameraManager ||
            !LocalPC->PlayerCameraManager->Class || !LocalPC->Pawn ||
            !LocalPC->Pawn->Class) {
            bIsAutoFiring = false;
            CurrentTarget = nullptr;
            return;
        }

        TargetInfo Best = GetBestTarget();
        if (!Best.bIsValid) {
            CurrentTarget = nullptr;
            return;
        }

        if (g_Config::bAimbotEnabled) {
            if (!LocalPC->PlayerCameraManager || !LocalPC->PlayerCameraManager->Class) {
                CurrentTarget = nullptr;
                return;
            }

            SDK::FVector CamLoc = LocalPC->PlayerCameraManager->GetCameraLocation();
            SDK::FRotator TargetRot = SDK::UKismetMathLibrary::FindLookAtRotation(CamLoc, Best.BestComponentLocation);
            SDK::FRotator CurrentRot = LocalPC->ControlRotation;

            float Smooth = g_Config::AimbotSmooth / 100.0f;
            SDK::FRotator FinalRot = SDK::UKismetMathLibrary::RLerp(CurrentRot, TargetRot, Smooth, true);

            LocalPC->SetControlRotation(FinalRot);
        }

        if (g_Config::bTriggerbotEnabled) {
            auto Now = std::chrono::steady_clock::now();
            float Delay = GetRandomDelay(g_Config::TriggerDelay, g_Config::TriggerRandomPercent);

            if (!LocalPC->Pawn || !LocalPC->Pawn->IsA(SDK::AShooterCharacter::StaticClass())) {
                if (bIsAutoFiring) {
                    ((SDK::AShooterPlayerController*)LocalPC)->OnStopFire();
                    bIsAutoFiring = false;
                }
                CurrentTarget = nullptr;
                return;
            }

            SDK::AShooterCharacter* MyChar = (SDK::AShooterCharacter*)LocalPC->Pawn;

            if (!MyChar) {
                if (bIsAutoFiring) {
                    ((SDK::AShooterPlayerController*)LocalPC)->OnStopFire();
                    bIsAutoFiring = false;
                }
                CurrentTarget = nullptr;
                return;
            }

            SDK::AShooterWeapon* MyWeapon = MyChar ? MyChar->CurrentWeapon : nullptr;

            if (MyWeapon && MyWeapon->GetAmmoReloadState() == SDK::EWeaponAmmoReloadState::Ready && MyWeapon->GetCurrentAmmo() > 0) {

                bool ShouldShoot = CalculateSpreadHitChance(MyWeapon, Best.Character, g_Config::TriggerHitChance);

                if (ShouldShoot) {
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(Now - LastTriggerTime).count() > Delay) {
                        SDK::AShooterPlayerController* ShooterPC = (SDK::AShooterPlayerController*)LocalPC;

                        if (!bIsAutoFiring) {
                            ShooterPC->OnStartFire();
                            bIsAutoFiring = true;
                        }

                        LastTriggerTime = Now;
                    }
                }
                else {
                    if (bIsAutoFiring) {
                        ((SDK::AShooterPlayerController*)LocalPC)->OnStopFire();
                        bIsAutoFiring = false;
                    }
                }
            }
            else {
                if (bIsAutoFiring) {
                    if (LocalPC) {
                        ((SDK::AShooterPlayerController*)LocalPC)->OnStopFire();
                    }
                    bIsAutoFiring = false;
                }
            }
        }
    }
}