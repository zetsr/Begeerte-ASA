#pragma once

#include "../../external/CppSDK/SDK.hpp"
#include "../../external/ezVMT/include/ezVMT.hpp"
#include "../CheatData/CheatData.hpp"

#include "../../external/Shadow-Gui/src/Shadow.h"
#include "../MEM/MEM.hpp"

namespace Hook {
    typedef void(__fastcall* tPostRender)(SDK::UGameViewportClient* _this, SDK::UCanvas* Canvas);
	inline tPostRender oPostRender = nullptr;

	inline void Test(SDK::UCanvas* Canvas) {
		SDK::APlayerController* LocalPC = MEM::GetLocalPC();
		if (!LocalPC) return;

		SDK::APawn* LocalPawn = LocalPC->Pawn;
		if (!LocalPawn) return;

		static std::vector<SDK::AActor*> Actors;
		Actors.clear();

		MEM::GetAllActorsOfClass(SDK::UWorld::GetWorld(), SDK::AActor::StaticClass(), Actors);
		for (SDK::AActor* Actor : Actors) {
			if (!Actor || !Actor->RootComponent) continue;

			float distance = LocalPawn->GetDistanceTo(Actor) / 100.f;
			if (distance > 100.f) continue;

			SDK::FVector origin, extent;
			Actor->GetActorBounds(true, &origin, &extent, false);

			SDK::FVector worldTop = { origin.X, origin.Y, origin.Z + extent.Z };
			SDK::FVector worldBottom = { origin.X, origin.Y, origin.Z - extent.Z };

			SDK::FVector screenTopVec = Canvas->K2_Project(worldTop);
			SDK::FVector screenBottomVec = Canvas->K2_Project(worldBottom);

			if (screenTopVec.Z > 0.0f && screenBottomVec.Z > 0.0f) {
				SDK::FVector2D screenTop = { screenTopVec.X, screenTopVec.Y };
				SDK::FVector2D screenBottom = { screenBottomVec.X, screenBottomVec.Y };

				float height = std::abs(screenBottom.Y - screenTop.Y);
				float width = height * 0.5f;

				Shadow::GetBackgroundDrawList()->AddText({ height, width }, { 1.f, 1.f, 1.f, 1.f }, Actor->GetName());
			}
		}
	}

	inline void __fastcall hkPostRender(SDK::UGameViewportClient* _this, SDK::UCanvas* Canvas) {
        oPostRender(_this, Canvas);

		Shadow::NewFrame(Canvas);

		Test(Canvas);

		Shadow::Render();
    }

	typedef void(__fastcall* tActorTick)(SDK::AActor* _this, float DeltaTime);
	inline tActorTick oActorTick = nullptr;

	inline void __fastcall hkActorTick(SDK::AActor* _this, float DeltaTime) {
		oActorTick(_this, DeltaTime);
	}

	inline void Init() {
		while (true) {
			SDK::UEngine* Engine = SDK::UEngine::GetEngine();
			SDK::UWorld* World = SDK::UWorld::GetWorld();

			if (Engine && Engine->GameViewport && World && World->PersistentLevel && World->PersistentLevel->Actors[0]) {
				ezVMT::CreateHook((void*)&hkPostRender, Engine->GameViewport, CheatData::VTableIndex::PostRender, (void**)&oPostRender);
				ezVMT::CreateHook((void*)&hkActorTick, World->PersistentLevel->Actors[0], CheatData::VTableIndex::Tick, (void**)&oActorTick);
				ezVMT::EnableHook(ezVMT::All_Hooks);
				break;
			}

			Sleep(100);
		}
	}

	inline void Uninit() {
		ezVMT::RemoveHook(ezVMT::All_Hooks);
	}
}