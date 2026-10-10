#pragma once
#include "../../external/CppSDK/SDK.hpp"

namespace MEM {
    inline SDK::APlayerController* GetLocalPC() {
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

        SDK::ULocalPlayer* LocalPlayer = World->OwningGameInstance->LocalPlayers[0];
        if (!LocalPlayer) {
            return nullptr;
        }

        SDK::APlayerController* PC = LocalPlayer->PlayerController;
        if (!PC) {
            return nullptr;
        }

        return PC;
    }

    inline void GetAllActorsOfClass(const SDK::UWorld* World, const SDK::UClass* TargetClass, std::vector<SDK::AActor*>& OutActors) {
        if (!World || !TargetClass || !World->Levels) return;

        for (int32_t i = 0; i < World->Levels.Num(); ++i) {
            if (!World->Levels[i]) continue;

            for (int32_t j = 0; j < World->Levels[i]->Actors.Num(); ++j) {
                SDK::AActor* Actor = World->Levels[i]->Actors[j];
                if (!Actor) continue;

                if (Actor->IsA(TargetClass)) {
                    OutActors.push_back(Actor);
                }
            }
        }
    }
}