#pragma once
#include <vector>
#include "SDK_Headers.hpp"

namespace g_Aimbot {
	struct TargetInfo {
		SDK::APrimalCharacter* Character = nullptr;
		SDK::FVector BestComponentLocation;
		float FovDistance = 999999.0f;
		float Health = 0.f;
		float Distance = 0.f;
		int BestBoneIndex = -1;
		bool bIsValid = false;
		bool bIsLocked = false;
		bool bIsTriggering = false;
	};
	void Tick();
	TargetInfo GetBestTarget();
}