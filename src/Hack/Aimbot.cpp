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
	static SDK::APrimalCharacter* pCurrentLockedTarget = nullptr; // 目标粘性：防止在两个目标间无限切换

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
				ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(sPos.X, sPos.Y), 1.2f, g_Util::GetU32Color(g_Config::AimPointsColor));

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

	// 辅助函数：根据你提供的 SDK 修正撞击检测逻辑
	bool IsHitTarget(const SDK::FHitResult& Hit, SDK::APrimalCharacter* Target) {
		if (!Target) return false;
		// 使用 SDK 中的 HitObjectHandle 获取引用对象
		SDK::UObject* HitObj = Hit.HitObjectHandle.ReferenceObject.Get();
		return (HitObj == (SDK::UObject*)Target);
	}

	// 修复BUG 2：精确可见性检查，打向瞄准点位置
	bool IsLocationVisible(SDK::APlayerController* PC, SDK::FVector Start, SDK::FVector End, SDK::AActor* TargetActor) {
		SDK::FHitResult Hit;
		SDK::TArray<SDK::AActor*> Ignore;
		Ignore.Add(PC->Pawn);

		// 从相机到“瞄准点”打射线
		bool bHasHit = SDK::UKismetSystemLibrary::LineTraceSingle(PC, Start, End,
			SDK::ETraceTypeQuery::TraceTypeQuery1, false, Ignore, SDK::EDrawDebugTrace::None, &Hit, true,
			SDK::FLinearColor(0, 0, 0, 0), SDK::FLinearColor(0, 0, 0, 0), 0.0f);

		// 如果没撞到东西说明路径清晰；如果撞到了，检查撞到的是不是目标本身
		if (!bHasHit) return true;

		SDK::UObject* HitObj = Hit.HitObjectHandle.ReferenceObject.Get();
		return (HitObj == (SDK::UObject*)TargetActor);
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
		SDK::FVector BestLoc = { 0,0,0 };

		for (int i = 0; i < Actors.Num(); i++) {
			SDK::AActor* Actor = Actors[i];
			if (!Actor || Actor == LocalPC->Pawn || Actor->bHidden || !Actor->IsA(SDK::APrimalCharacter::StaticClass())) continue;

			SDK::APrimalCharacter* Char = (SDK::APrimalCharacter*)Actor;
			if (Char->IsDead() || g_ESP::GetRelation(Char, (SDK::APrimalCharacter*)LocalPC->Pawn) == g_ESP::RelationType::Team) continue;

			// 计算该目标的 AABB 中心作为瞄准点
			SDK::USkeletalMeshComponent* Mesh = Char->Mesh;
			if (!Mesh) continue;

			SDK::FVector MinB = { FLT_MAX, FLT_MAX, FLT_MAX };
			SDK::FVector MaxB = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
			int BoneCount = Mesh->GetNumBones();
			for (int j = 0; j < BoneCount; j++) {
				SDK::FVector BoneLoc = Mesh->GetSocketLocation(Mesh->GetBoneName(j));
				if (BoneLoc.IsZero()) continue;
				MinB.X = min(MinB.X, BoneLoc.X); MinB.Y = min(MinB.Y, BoneLoc.Y); MinB.Z = min(MinB.Z, BoneLoc.Z);
				MaxB.X = max(MaxB.X, BoneLoc.X); MaxB.Y = max(MaxB.Y, BoneLoc.Y); MaxB.Z = max(MaxB.Z, BoneLoc.Z);
			}
			SDK::FVector CurrentTargetLoc = (MinB + MaxB) / 2.0f;

			// 修复 BUG 2：不再对实体中心测试，而是对精确瞄准点测试射线
			if (!IsLocationVisible(LocalPC, CamLoc, CurrentTargetLoc, Char)) continue;

			float Dist = SDK::UKismetMathLibrary::Vector_Distance(CamLoc, CurrentTargetLoc);
			float Angle = GetAngleDistance(CamLoc, CurrentTargetLoc, CamRot);

			if (Angle > g_Config::AimbotFOV) continue;

			// 修复 BUG 1：引入目标粘性逻辑
			float Bias = 0.0f;
			if (pCurrentLockedTarget && Char == pCurrentLockedTarget) {
				Bias = 300.0f; // 给予当前目标 3米的虚拟距离优势，防止在边缘抖动切换
			}

			bool bIsBetter = false;
			if ((Dist - Bias) < MinDistance - 150.0f) {
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
				BestLoc = CurrentTargetLoc;
			}
		}

		if (BestChar) {
			pCurrentLockedTarget = BestChar;
			Best.Character = BestChar;
			Best.BestComponentLocation = BestLoc;
			Best.FovDistance = GetAngleDistance(CamLoc, BestLoc, CamRot);
			Best.Distance = MinDistance;
			Best.Health = MinHealth;
			Best.bIsValid = true;

			float TriggerThreshold = (MinDistance > 5000.0f) ? 0.8f : 1.8f;

			SDK::FHitResult Hit;
			SDK::FVector TraceEnd = CamLoc + (SDK::UKismetMathLibrary::GetForwardVector(CamRot) * 20000.0f);
			SDK::TArray<SDK::AActor*> Ignore; Ignore.Add(LocalPC->Pawn);

			bool bHit = SDK::UKismetSystemLibrary::LineTraceSingle(LocalPC, CamLoc, TraceEnd,
				SDK::ETraceTypeQuery::TraceTypeQuery1, false, Ignore, SDK::EDrawDebugTrace::None, &Hit, true,
				SDK::FLinearColor(0, 0, 0, 0), SDK::FLinearColor(0, 0, 0, 0), 0.0f);

			if (bHit && IsHitTarget(Hit, BestChar)) {
				Best.bIsLocked = true;
			}
			else {
				Best.bIsLocked = (Best.FovDistance < TriggerThreshold);
			}
		}
		else {
			pCurrentLockedTarget = nullptr;
		}
		return Best;
	}

	void Tick() {
		if (!g_Config::bAimbotEnabled && !g_Config::bTriggerbotEnabled) {
			pCurrentLockedTarget = nullptr;
			return;
		}

		SDK::UWorld* World = SDK::UWorld::GetWorld();
		if (!World || !World->PersistentLevel) return;

		SDK::APlayerController* LocalPC = g_Util::GetLocalPC();
		if (!LocalPC || !LocalPC->Pawn) return;

		SDK::AShooterPlayerController* ShooterPC = (SDK::AShooterPlayerController*)LocalPC;
		SDK::AShooterCharacter* MyChar = (SDK::AShooterCharacter*)ShooterPC->Pawn;
		if (!MyChar) return;

		SDK::AShooterWeapon* MyWeapon = MyChar->CurrentWeapon;
		if (!MyWeapon || MyWeapon->GetAmmoReloadState() != SDK::EWeaponAmmoReloadState::Ready || MyWeapon->GetCurrentAmmo() <= 0) {
			if (bIsAutoFiring) {
				g_Util::MimicMouseClick(false);
				bIsAutoFiring = false;
			}
			return;
		}

		TargetInfo Best = GetBestTarget();

		if (g_Config::bDrawAimPoints && Best.bIsValid) {
			VisualizeTargetBones(Best.Character, LocalPC);
		}

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

		if (g_Config::bTriggerbotEnabled && Best.bIsValid) {
			if (Best.bIsLocked) {
				g_Util::MimicMouseClick(true);
				bIsAutoFiring = true;
				Best.bIsTriggering = true;
			}
			else if (bIsAutoFiring) {
				g_Util::MimicMouseClick(false);
				bIsAutoFiring = false;
			}
		}
		else if (bIsAutoFiring) {
			g_Util::MimicMouseClick(false);
			bIsAutoFiring = false;
		}

		DrawStatusText(Best);
	}
}