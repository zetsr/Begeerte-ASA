#pragma once
#include <vector>

namespace g_Aimbot {
    struct TargetInfo {
        SDK::APrimalCharacter* Character = nullptr;
        SDK::FVector BestComponentLocation;
        float FovDistance = 999999.0f;
        bool bIsValid = false;
    };

    void Tick();
    void ResetTarget();

    TargetInfo GetBestTarget();
    bool CalculateSpreadHitChance(SDK::AShooterWeapon* Weapon, SDK::AActor* Target, float RequiredChance);
    SDK::FVector GetPredictLocation(SDK::APrimalCharacter* Target, float BulletSpeed);
}