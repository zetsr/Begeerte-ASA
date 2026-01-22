#include "../Minimal-D3D12-Hook-ImGui-1.0.2/Main/mdx12_api.h"
#include "../CppSDK/SDK.hpp"
#include "ESP.h"
#include "Configs.h"
#include "DrawESP.h"
#include <vector>
#include <string>

namespace g_DrawESP {
	inline ImU32 ToImColor(float r, float g, float b, float a) {
		return ImGui::ColorConvertFloat4ToU32(ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
	}

	void DrawESP() {
		SDK::UWorld* World = SDK::UWorld::GetWorld();
		if (!World || !World->GameState || !World->PersistentLevel) return;

		SDK::APlayerController* LocalPC = g_ESP::GetLocalPC();
		if (!LocalPC || !LocalPC->Pawn) return;

		SDK::APlayerState* LocalPS = LocalPC->PlayerState;
		if (!LocalPS) return;

		ImGuiIO& io = ImGui::GetIO();
		float screenW = io.DisplaySize.x;
		float screenH = io.DisplaySize.y;

		SDK::TArray<SDK::AActor*>& Actors = World->PersistentLevel->Actors;

		for (int i = 0; i < Actors.Num(); i++) {
			SDK::AActor* TargetActor = Actors[i];

			if (!TargetActor || TargetActor == LocalPC->Pawn) continue;
			if (!TargetActor->IsA(SDK::APrimalCharacter::StaticClass())) continue;

			SDK::APrimalCharacter* TargetChar = (SDK::APrimalCharacter*)TargetActor;

			if (TargetChar->IsDead()) continue;

			SDK::APlayerState* TargetPS = TargetChar->PlayerState;
			g_ESP::RelationType relation = g_ESP::GetPlayerRelation(TargetPS, LocalPS);

			bool bDrawBox = false, bDrawHealthBar = false, bDrawName = false;
			bool bDrawSpecies = false, bDrawGrowth = false, bDrawDistance = false;
			float* BoxColor = nullptr;
			float* NameColor = nullptr;
			float* DistanceColor = nullptr;

			if (relation == g_ESP::RelationType::Team) {
				bDrawBox = g_Config::bDrawBoxTeam;
				BoxColor = g_Config::BoxColorTeam;
				bDrawHealthBar = g_Config::bDrawHealthBarTeam;
				bDrawName = g_Config::bDrawNameTeam;
				NameColor = g_Config::NameColorTeam;
				bDrawSpecies = g_Config::bDrawSpeciesTeam;
				bDrawGrowth = g_Config::bDrawGrowthTeam;
				bDrawDistance = g_Config::bDrawDistanceTeam;
				DistanceColor = g_Config::DistanceColorTeam;
			}

			else { // Enemy »ò Wild
				bDrawBox = g_Config::bDrawBox;
				BoxColor = g_Config::BoxColor;
				bDrawHealthBar = g_Config::bDrawHealthBar;
				bDrawName = g_Config::bDrawName;
				NameColor = g_Config::NameColor;
				bDrawSpecies = g_Config::bDrawSpecies;
				bDrawGrowth = g_Config::bDrawGrowth;
				bDrawDistance = g_Config::bDrawDistance;
				DistanceColor = g_Config::DistanceColor;
			}

			SDK::FVector2D screenPos;
			SDK::FVector actorLoc = TargetActor->K2_GetActorLocation();

			if (LocalPC->ProjectWorldLocationToScreen(actorLoc, &screenPos, false)) {
				if (screenPos.X > 0 && screenPos.X < screenW && screenPos.Y > 0 && screenPos.Y < screenH) {

					g_ESP::FlagManager flagMgr;
					flagMgr.Reset();

					g_ESP::BoxRect rect = g_ESP::DrawBox(TargetActor,
						BoxColor[0] * 255.0f, BoxColor[1] * 255.0f,
						BoxColor[2] * 255.0f, BoxColor[3] * 255.0f, 0.5f, !bDrawBox);

					if (rect.valid) {
						if (bDrawHealthBar) {
							float currentHP = TargetChar->GetHealth();
							float maxHP = TargetChar->GetMaxHealth();
							g_ESP::DrawHealthBar(rect, currentHP, maxHP, 255.0f);

							std::string hpStr = std::to_string((int)currentHP);
							float hpPercent = (maxHP > 0) ? (currentHP / maxHP) : 0.0f;
							ImU32 hpCol = (hpPercent > 0.5f) ? ToImColor(100, 255, 100, 255) : ToImColor(255, 100, 100, 255);
							flagMgr.AddFlag(rect, hpStr, hpCol, g_ESP::FlagPos::Left);
						}

						if (bDrawName) {
							std::string displayName;
							if (TargetPS) {
								displayName = TargetPS->GetPlayerName().ToString();
							}
							else {
								displayName = TargetChar->GetDescriptiveName().ToString();
							}

							g_ESP::DrawName(TargetActor, rect,
								NameColor[0] * 255.0f, NameColor[1] * 255.0f,
								NameColor[2] * 255.0f, NameColor[3] * 255.0f);
						}

						if (bDrawSpecies) {
							std::string species = "Human";
							if (TargetActor->IsA(SDK::APrimalDinoCharacter::StaticClass())) {
								SDK::APrimalDinoCharacter* Dino = (SDK::APrimalDinoCharacter*)TargetActor;
								species = Dino->GetDescriptiveName().ToString();
							}
							flagMgr.AddFlag(rect, species, ToImColor(220, 220, 220, 255), g_ESP::FlagPos::Right);
						}

						if (bDrawDistance) {
							float dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
							std::string distStr = std::to_string((int)dist) + "m";
							flagMgr.AddFlag(rect, distStr, g_Config::GetU32Color(DistanceColor), g_ESP::FlagPos::Right);
						}
					}
				}

				else if (g_Config::bEnableOOF) {
					std::vector<g_ESP::OOFFlag> oofFlags;
					std::string oofName = TargetPS ? TargetPS->GetPlayerName().ToString() : TargetChar->GetDescriptiveName().ToString();

					if (bDrawName) {
						oofFlags.push_back({ oofName, g_Config::GetU32Color(NameColor) });
					}
					if (bDrawDistance) {
						float dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
						oofFlags.push_back({ std::to_string((int)dist) + "m", g_Config::GetU32Color(DistanceColor) });
					}
					g_ESP::DrawOutOfFOV(TargetActor, LocalPC, oofFlags);
				}
			}
		}
	}
}