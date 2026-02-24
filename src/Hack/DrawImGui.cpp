#define NOMINMAX  
#if defined(__cpp_char8_t)
#define U8(str) reinterpret_cast<const char*>(u8##str)
#else
#define U8(str) u8##str
#endif

#include "../Minimal-D3D12-Hook-ImGui/Main/mdx12_api.h"
#include "SDK_Headers.hpp"
#include "ESP.h"
#include "Configs.h"
#include "DrawESP.h"
#include "ConfigImGui.h"
#include "DrawImGui.h"
#include "Aimbot.h"
#include "ConfigManager.h"
#include "LuaManager.h"
#include "Util.h"

#include <cstdio>
#include <string>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <map>

namespace g_DrawImGui {
	float g_MenuAlpha = 0.0f;

	void MyImGuiDraw(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags)
	{
		if (!style_initialized) { SetupCustomImGuiStyle(); style_initialized = true; }
		const float FADE_SPEED = 5.0f;
		static bool g_PrevMenuState = false;
		bool isNowOpen = g_MDX12::g_MenuState::g_isOpen;

		if (isNowOpen && !g_PrevMenuState) {
			LuaManager::Get().RefreshFileList();
			ConfigManager::Get().RefreshFileList();
		}
		g_PrevMenuState = isNowOpen;

		float deltaTime = ImGui::GetIO().DeltaTime;

		if (g_MDX12::g_MenuState::g_isOpen) {
			g_MenuAlpha += deltaTime * FADE_SPEED;
			if (g_MenuAlpha > 1.0f) g_MenuAlpha = 1.0f;
		}
		else {
			g_MenuAlpha -= deltaTime * FADE_SPEED;
			if (g_MenuAlpha < 0.0f) g_MenuAlpha = 0.0f;
		}

		if (g_MenuAlpha > 0.001f) {
			ImGui::PushFont(g_MDX12::g_Alibaba_PuHuiTi_Bold);

			ImGui::GetStyle().WindowMinSize = ImVec2(720.0f, 720.0f);
			ImGui::SetNextWindowSize(ImVec2(720, 720), ImGuiCond_FirstUseEver);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_MenuAlpha);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 22.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(7.0f / 255.0f, 8.0f / 255.0f, 10.0f / 255.0f, 0.97f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(110.0f / 255.0f, 231.0f / 255.0f, 183.0f / 255.0f, 0.08f));

			ImGuiWindowFlags wFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
			ImGui::GetIO().ConfigWindowsResizeFromEdges = false;

			if (ImGui::Begin("Begeerte", nullptr, wFlags)) {

				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				ImVec2 title_pos = ImGui::GetWindowPos();
				float title_width = ImGui::GetWindowWidth();
				float time = ImGui::GetTime();

				draw_list->AddRectFilledMultiColor(
					ImVec2(title_pos.x, title_pos.y),
					ImVec2(title_pos.x + title_width, title_pos.y + 2.0f),
					IM_COL32(79, 214, 166, 0),
					IM_COL32(110, 231, 183, 180),
					IM_COL32(110, 231, 183, 180),
					IM_COL32(79, 214, 166, 0)
				);

				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(22.0f, 11.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 0.0f));
				ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(1.0f, 1.0f, 1.0f, 0.03f));
				ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.43f, 0.91f, 0.72f, 0.1f));
				ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.43f, 0.91f, 0.72f, 0.18f));
				ImGui::PushStyleColor(ImGuiCol_TabUnfocused, ImVec4(1.0f, 1.0f, 1.0f, 0.03f));
				ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(1.0f, 1.0f, 1.0f, 0.05f));

				if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None)) {

					if (ImGui::BeginTabItem(U8("自瞄"))) {
						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14.0f, 14.0f));
						BeginTabRegion("AimBotRegion");

						ImGui::TextColored(ThemeColors::ACCENT, U8("队友设置"));
						DrawAnimatedSeparator();
						DrawCustomCheckbox(U8("自动瞄准"), &g_Config::bAimbotEnabled);
						DrawCustomSliderFloat(U8("瞄准范围"), &g_Config::AimbotFOV, 0.1f, 180.0f, "%.1f", 0.1f, U8("°"));
						DrawCustomSliderFloat(U8("瞄准速度"), &g_Config::AimbotSmooth, 0.1f, 100.0f, "%.1f", 0.1f, "%");
						DrawCustomCheckbox(U8("自动射击"), &g_Config::bTriggerbotEnabled);
						DrawAnimatedSeparator();

						EndTabRegion();
						ImGui::PopStyleVar();
						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem(U8("视觉"))) {
						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14.0f, 14.0f));
						BeginTabRegion("VisualsRegion");

						ImGui::TextColored(ThemeColors::ACCENT, U8("全局设置"));
						DrawAnimatedSeparator();
						DrawColorPickerRow(U8("方框"), &g_Config::bDrawBox, "BoxCol1", g_Config::BoxColor);
						DrawColorPickerRow(U8("名称"), &g_Config::bDrawName, "NameCol1", g_Config::NameColor);
						DrawCustomCheckbox(U8("血量"), &g_Config::bDrawHealthBar);
						DrawColorPickerRow(U8("眩晕"), &g_Config::bDrawTorpor, "TorporCol", g_Config::TorporColor);
						DrawColorPickerRow(U8("尸体"), &g_Config::bDrawRagdoll, "RagdollCol", g_Config::RagdollColor);
						DrawAnimatedSeparator();

						ImGui::TextColored(ThemeColors::ACCENT, U8("额外信息"));
						DrawAnimatedSeparator();
						DrawColorPickerRow(U8("距离"), &g_Config::bDrawDistance, "DistCol1", g_Config::DistanceColor);
						DrawColorPickerRow(U8("显示瞄准点"), &g_Config::bDrawAimPoints, "AimPointsCol1", g_Config::AimPointsColor);
						DrawColorPickerRow(U8("显示瞄准骨骼"), &g_Config::bDrawAimSkeleton, "AimSkeletonCol1", g_Config::AimSkeletonColor);
						DrawAnimatedSeparator();

						ImGui::TextColored(ThemeColors::ACCENT, U8("世界信息"));
						DrawAnimatedSeparator();

						DrawColorPickerRow(U8("掉落的物品"), &g_Config::bDrawDroppedItems, "DroppedItemNameCol", g_Config::DroppedItemNameColor);
						// DrawCustomCheckbox(U8("掉落的物品"), &g_Config::bDrawDroppedItems);
						if (g_Config::bDrawDroppedItems) {
							// DrawCustomColorPicker("DroppedItemNameCol", g_Config::DroppedItemNameColor, U8("物品名称"));
							DrawCustomColorPicker("DroppedItemDistanceCol", g_Config::DroppedItemDistanceColor, U8("物品距离"));
							DrawCustomColorPicker("DroppedItemPiledCol", g_Config::DroppedItemPiledColor, U8("堆叠颜色"));
							DrawCustomColorPicker("DroppedItemCryopodCol", g_Config::DroppedItemCryopodColor, U8("低温仓颜色"));
							DrawCustomColorPicker("DroppedItemEggCol", g_Config::DroppedItemEggColor, U8("蛋颜色"));
							DrawCustomColorPicker("DroppedItemMeatCol", g_Config::DroppedItemMeatColor, U8("肉类颜色"));
							DrawCustomColorPicker("DroppedItemSpoiledMeatCol", g_Config::DroppedItemSpoiledMeatColor, U8("腐肉颜色"));
							DrawCustomColorPicker("DroppedItemWoodCol", g_Config::DroppedItemWoodColor, U8("木头颜色"));
							DrawCustomColorPicker("DroppedItemThatchCol", g_Config::DroppedItemThatchColor, U8("茅草颜色"));
							DrawCustomColorPicker("DroppedItemMetalCol", g_Config::DroppedItemMetalColor, U8("金属颜色"));
							DrawCustomColorPicker("DroppedItemStoneCol", g_Config::DroppedItemStoneColor, U8("石头颜色"));
							DrawCustomColorPicker("DroppedItemCrystalCol", g_Config::DroppedItemCrystalColor, U8("水晶颜色"));
							DrawCustomColorPicker("DroppedItemGemCol", g_Config::DroppedItemGemColor, U8("宝石颜色"));
							DrawCustomColorPicker("DroppedItemPearlCol", g_Config::DroppedItemPearlColor, U8("珍珠颜色"));
							DrawCustomColorPicker("DroppedItemHideCol", g_Config::DroppedItemHideColor, U8("兽皮颜色"));
							DrawCustomColorPicker("DroppedItemPeltCol", g_Config::DroppedItemPeltColor, U8("毛皮颜色"));
							DrawCustomColorPicker("DroppedItemKeratinCol", g_Config::DroppedItemKeratinColor, U8("角质颜色"));
							DrawCustomColorPicker("DroppedItemChitinCol", g_Config::DroppedItemChitinColor, U8("甲壳素颜色"));
							DrawCustomColorPicker("DroppedItemCorruptedPolymerCol", g_Config::DroppedItemCorruptedPolymerColor, U8("腐化瘤颜色"));
							DrawCustomColorPicker("DroppedItemPolymer_OrganicCol", g_Config::DroppedItemPolymer_OrganicColor, U8("有机聚合物颜色"));
							DrawCustomColorPicker("DroppedItemPolymerCol", g_Config::DroppedItemPolymerColor, U8("聚合物颜色"));
							DrawCustomSliderFloat(U8("物品显示距离"), &g_Config::DroppedItemMaxDistance, 1.0f, 500.0f, "%.0f", 1.0f, "m");
						}
						DrawAnimatedSeparator();

						DrawColorPickerRow(U8("显示建筑"), &g_Config::bDrawStructures, "StructureNameCol", g_Config::StructureNameColor);
						// DrawCustomCheckbox(U8("显示建筑"), &g_Config::bDrawStructures);
						if (g_Config::bDrawStructures) {
							// DrawCustomColorPicker("StructureNameCol", g_Config::StructureNameColor, U8("建筑名称"));
							DrawCustomColorPicker("StructureOwnerCol", g_Config::StructureOwnerColor, U8("建筑所有者"));
							DrawCustomColorPicker("StructureDistanceCol", g_Config::StructureDistanceColor, U8("建筑距离"));
							DrawCustomSliderFloat(U8("建筑显示距离"), &g_Config::StructureMaxDistance, 1.0f, 10000.0f, "%.0f", 1.0f, "m");
						}
						DrawAnimatedSeparator();

						DrawColorPickerRow(U8("显示水源"), &g_Config::bDrawWater, "WaterNameCol", g_Config::WaterNameColor);
						// DrawCustomCheckbox(U8("显示水源"), &g_Config::bDrawWater);
						if (g_Config::bDrawWater) {
							// DrawCustomColorPicker("WaterNameCol", g_Config::WaterNameColor, U8("水源名称"));
							DrawCustomColorPicker("WaterDistanceCol", g_Config::WaterDistanceColor, U8("水源距离"));
							// DrawCustomSliderFloat(U8("水源显示距离"), &g_Config::WaterMaxDistance, 1.0f, 10000.0f, "%.0f", 1.0f, "m");
							DrawCustomSliderFloat(U8("显示最近水源数量"), &g_Config::WaterMaxCount, 1.0f, 10.0f, "%.0f", 1.0f, U8("个"));
						}
						DrawAnimatedSeparator();

						DrawColorPickerRow(U8("显示视野外的威胁"), &g_Config::bEnableOOF, "OOFCol1", g_Config::OOFColor);
						if (g_Config::bEnableOOF) {
							float avail = ImGui::GetContentRegionAvail().x;
							float frame_h = ImGui::GetFrameHeight();
							float pad_y = ImGui::GetStyle().FramePadding.y;
							float btn_sz = frame_h - pad_y * 2.0f;
							if (btn_sz <= 0.0f) btn_sz = frame_h * 0.8f;
							float target = ImGui::GetCursorPosX() + avail - btn_sz;
							ImGui::SetCursorPosX(target);
							ImGui::Dummy(ImVec2(0, 6.0f));
							DrawCustomSliderFloat(U8("箭头尺寸"), &g_Config::OOFSize, 5.0f, 30.0f, "%.1f", 1.0f);
							DrawCustomSliderFloat(U8("屏幕半径"), &g_Config::OOFRadius, 0.5f, 1.00f, "%.2f", 0.01f);
							DrawCustomSliderFloat(U8("呼吸速度"), &g_Config::OOFBreathSpeed, 0.1f, 5.0f, "%.1f", 0.1f);
							DrawCustomSliderFloat(U8("最小透明度"), &g_Config::OOFMinAlpha, 0.1f, 0.9f, "%.2f", 0.01f);
							DrawCustomSliderFloat(U8("最大透明度"), &g_Config::OOFMaxAlpha, 0.2f, 1.0f, "%.2f", 0.01f);
						}

						EndTabRegion();
						ImGui::PopStyleVar();
						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem(U8("队友"))) {
						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14.0f, 14.0f));
						BeginTabRegion("TeamRegion");

						ImGui::TextColored(ThemeColors::ACCENT, U8("队友设置"));
						DrawAnimatedSeparator();
						DrawColorPickerRow(U8("方框##Team"), &g_Config::bDrawBoxTeam, "BoxColTeam", g_Config::BoxColorTeam);
						DrawColorPickerRow(U8("名称##Team"), &g_Config::bDrawNameTeam, "NameColTeam", g_Config::NameColorTeam);
						DrawCustomCheckbox(U8("血量##Team"), &g_Config::bDrawHealthBarTeam);
						DrawColorPickerRow(U8("眩晕##Team"), &g_Config::bDrawTorporTeam, "TorporColTeam", g_Config::TorporColorTeam);
						DrawColorPickerRow(U8("尸体##Team"), &g_Config::bDrawRagdollTeam, "RagdollColTeam", g_Config::RagdollColorTeam);
						DrawAnimatedSeparator();

						ImGui::TextColored(ThemeColors::ACCENT, U8("额外信息"));
						DrawAnimatedSeparator();
						DrawColorPickerRow(U8("距离##Team"), &g_Config::bDrawDistanceTeam, "DistColTeam", g_Config::DistanceColorTeam);
						DrawAnimatedSeparator();

						EndTabRegion();
						ImGui::PopStyleVar();
						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem(U8("生物列表"))) {
						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14.0f, 14.0f));
						BeginTabRegion("EntityListRegion");

						ImGui::TextColored(ThemeColors::ACCENT, U8("生物列表"));
						DrawAnimatedSeparator();

						ImGui::Checkbox(U8("应用筛选到全局视觉"), &g_Config::bEnableFilter);
						ImGui::SameLine();

						ImGui::PushItemWidth(-1.0f);
						ImGui::InputTextWithHint("##EntitySearch", U8("输入生物名称进行过滤 (如: 南巨)..."), g_Config::entitySearchBuf, IM_ARRAYSIZE(g_Config::entitySearchBuf));
						ImGui::PopItemWidth();

						ImGui::Spacing();
						if (ImGui::BeginChild("##EntityListChild", ImVec2(0, 0), true)) {

							SDK::UWorld* World = SDK::UWorld::GetWorld();
							SDK::APlayerController* LocalPC = g_Util::GetLocalPC();

							if (World && World->PersistentLevel && LocalPC && LocalPC->Pawn) {
								SDK::TArray<SDK::AActor*>& Actors = World->PersistentLevel->Actors;
								SDK::AActor* LocalPawn = LocalPC->Pawn;
								std::string searchFilter = g_Config::entitySearchBuf;

								for (int i = 0; i < Actors.Num(); i++) {
									SDK::AActor* TargetActor = Actors[i];

									if (!TargetActor || TargetActor == LocalPawn || TargetActor->bHidden) continue;
									if (!TargetActor->IsA(SDK::APrimalCharacter::StaticClass())) continue;

									SDK::APrimalCharacter* TargetChar = (SDK::APrimalCharacter*)TargetActor;
									if (!TargetChar) continue;
									if (TargetChar->IsDead()) continue;

									std::string gender = "?";
									if (LocalPC->Pawn && TargetActor && TargetChar) {
										gender = TargetActor->IsFemale() ? "F" : "M";
									}

									std::string displayName;
									if (gender != "?") {
										displayName = TargetChar->PlayerState ? TargetChar->PlayerState->GetPlayerName().ToString() + "-" + gender : TargetChar->GetDescriptiveName().ToString() + "-" + gender;
									}
									else {
										displayName = TargetChar->PlayerState ? TargetChar->PlayerState->GetPlayerName().ToString() : TargetChar->GetDescriptiveName().ToString();
									}

									if (displayName.empty() || displayName == "None") continue;

									if (g_Util::IsEntityMatch(displayName, g_Config::entitySearchBuf)) {
										float dist = 0.0f;
										if (LocalPC->Pawn && TargetActor) {
											dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
										}

										float curHP = TargetChar->GetHealth();
										float maxHP = TargetChar->GetMaxHealth();

										ImGui::Text("[%dm] %s - %.0f/%.0f",
											(int)dist,
											displayName.c_str(),
											curHP,
											maxHP);
									}
								}
							}
							ImGui::EndChild();
						}

						DrawAnimatedSeparator();

						EndTabRegion();
						ImGui::PopStyleVar();
						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem(U8("建筑列表"))) {
						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14.0f, 14.0f));
						BeginTabRegion("StructureListRegion");

						ImGui::TextColored(ThemeColors::ACCENT, U8("建筑列表"));
						DrawAnimatedSeparator();

						ImGui::Checkbox(U8("应用筛选到全局视觉"), &g_Config::bEnableStructureFilter);
						ImGui::SameLine();

						ImGui::PushItemWidth(-1.0f);
						ImGui::InputTextWithHint("##StructureSearch", U8("输入建筑名称进行过滤 (如: 大门)..."), g_Config::structureSearchBuf, IM_ARRAYSIZE(g_Config::structureSearchBuf));
						ImGui::PopItemWidth();

						ImGui::Spacing();
						if (ImGui::BeginChild("##StructureListChild", ImVec2(0, 0), true)) {

							SDK::UWorld* World = SDK::UWorld::GetWorld();
							SDK::APlayerController* LocalPC = g_Util::GetLocalPC();

							if (World && World->PersistentLevel && LocalPC && LocalPC->Pawn) {
								SDK::TArray<SDK::AActor*>& Actors = World->PersistentLevel->Actors;
								std::string searchFilter = g_Config::structureSearchBuf;

								for (int i = 0; i < Actors.Num(); i++) {
									SDK::AActor* TargetActor = Actors[i];

									if (!TargetActor || TargetActor->bHidden) continue;
									if (!TargetActor->IsA(SDK::APrimalStructure::StaticClass())) continue;

									SDK::APrimalStructure* Structure = (SDK::APrimalStructure*)TargetActor;

									if (!Structure) continue;
									if (Structure->Health <= 0.0f) continue;

									std::string structureName = Structure->GetDescriptiveName().ToString();
									if (structureName.empty() || structureName == "None") {
										structureName = "Structure";
									}

									if (g_Util::IsStructureMatch(structureName, g_Config::structureSearchBuf)) {
										float dist = 0.0f;
										if (LocalPC->Pawn && TargetActor) {
											dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
										}

										std::string ownerStr = Structure->OwnerName.ToString();
										if (ownerStr.empty() || ownerStr == "None") {
											ownerStr = "*";
										}

										float curHP = Structure->Health;
										float maxHP = Structure->MaxHealth;
										int hpPercent = (maxHP > 0) ? (int)((curHP / maxHP) * 100.0f) : 0;

										ImGui::Text("[%dm] %s - %s - %.0f/%.0f",
											(int)dist,
											structureName.c_str(),
											ownerStr.c_str(),
											curHP,
											maxHP);
									}
								}
							}
							ImGui::EndChild();
						}

						DrawAnimatedSeparator();

						EndTabRegion();
						ImGui::PopStyleVar();
						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem(U8("配置"))) {
						auto& mgr = ConfigManager::Get();
						auto& configs = mgr.GetConfigs();

						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14.0f, 14.0f));
						BeginTabRegion("ConfigManagerRegion");

						ImGui::TextColored(ThemeColors::ACCENT, U8("配置管理系统"));
						DrawAnimatedSeparator();

						if (ImGui::Button(U8("刷新列表"))) {
							mgr.RefreshFileList();
						}
						ImGui::SameLine();
						if (ImGui::Button(U8("打开目录"))) {
							std::string path = mgr.GetConfigDir();
							ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWDEFAULT);
						}

						ImGui::SameLine(0.0f, 20.0f);

						static char configNameBuf[65] = { 0 };
						ImGui::SetNextItemWidth(150.0f);
						ImGui::InputTextWithHint("##ConfigName", U8("输入参数名称..."), configNameBuf, sizeof(configNameBuf));
						ImGui::SameLine();
						if (ImGui::Button(U8("创建"))) {
							std::string configName(configNameBuf);
							if (configName.empty()) {

							}
							else if (!ConfigManager::IsValidConfigName(configName)) {

							}
							else {
								if (mgr.CreateConfig(configName)) {
									configNameBuf[0] = '\0';
								}
								else {

								}
							}
						}

						ImGui::Spacing();

						static int selectedConfigIdx = -1;
						if (ImGui::BeginChild("##ConfigListChild", ImVec2(0, 300), true)) {
							if (configs.empty()) {
								ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2 - 10);
								ImGui::TextDisabled(U8("暂无配置文件"));
							}
							else {
								for (int i = 0; i < (int)configs.size(); i++) {
									auto& config = configs[i];
									ImGui::PushID(i);

									bool isSelected = (selectedConfigIdx == i);
									if (CustomSelectable(config.name.c_str(), isSelected, 8.0f) /*ImGui::Selectable(config.name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)*/) {
										selectedConfigIdx = i;
									}

									ImGui::PopID();
								}
							}
							ImGui::EndChild();
						}

						ImGui::Spacing();

						ImGui::BeginDisabled(selectedConfigIdx < 0 || selectedConfigIdx >= (int)configs.size());

						if (ImGui::Button(U8("加载配置"), ImVec2(120, 0))) {
							if (selectedConfigIdx >= 0 && selectedConfigIdx < (int)configs.size()) {
								auto& selectedConfig = configs[selectedConfigIdx];
								if (mgr.LoadConfig(selectedConfig.name)) {

								}
								else {

								}
							}
						}

						ImGui::SameLine();

						if (ImGui::Button(U8("保存配置"), ImVec2(120, 0))) {
							if (selectedConfigIdx >= 0 && selectedConfigIdx < (int)configs.size()) {
								auto& selectedConfig = configs[selectedConfigIdx];
								if (mgr.SaveConfig(selectedConfig.name)) {

								}
								else {

								}
							}
						}

						ImGui::EndDisabled();

						DrawAnimatedSeparator();
						EndTabRegion();
						ImGui::PopStyleVar();
						ImGui::EndTabItem();
					}

					LuaManager::Get().Update();
					if (ImGui::BeginTabItem(U8("脚本"))) {
						auto& mgr = LuaManager::Get();
						auto& scripts = mgr.GetScripts();

						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14.0f, 14.0f));
						BeginTabRegion("LuaManagerRegion");

						ImGui::TextColored(ThemeColors::ACCENT, U8("脚本管理系统"));
						DrawAnimatedSeparator();

						if (ImGui::Button(U8("刷新列表"))) {
							mgr.RefreshFileList();
						}
						ImGui::SameLine();
						if (ImGui::Button(U8("打开目录"))) {
							std::string path = mgr.GetScriptDir();
							ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWDEFAULT);
						}

						ImGui::Spacing();

						if (ImGui::BeginChild("##LuaScriptListChild", ImVec2(0, 300), true)) {
							if (scripts.empty()) {
								ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2 - 10);
								// ImGui::TextDisabled(U8("无可用脚本"));
							}
							else {
								for (int i = 0; i < (int)scripts.size(); i++) {
									auto& script = scripts[i];
									ImGui::PushID(i);

									bool isActive = script.isLoaded;
									if (isActive) {
										ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
									}
									else {
										ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
									}

									std::string prefix = isActive ? U8("* ") : U8("");
									std::string displayName = prefix + script.name;

									if (CustomSelectable(displayName.c_str(), false, 8.0f) /*ImGui::Selectable(displayName.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)*/) {
										bool targetState = !script.isLoaded;
										mgr.SetScriptState(i, targetState);
									}

									ImGui::PopStyleColor();

									if (script.hasError) {
										ImGui::SameLine(ImGui::GetWindowWidth() - 35);
										ImGui::TextDisabled("[!] ");

										if (ImGui::IsItemHovered()) {
											ImGui::BeginTooltip();
											ImGui::TextUnformatted(U8("右键点击图标以复制错误详情"));
											ImGui::Separator();
											ImGui::TextUnformatted(script.lastError.c_str());
											ImGui::EndTooltip();
										}

										std::string popupId = "ErrorPopup_" + std::to_string(i);
										if (ImGui::BeginPopupContextItem(popupId.c_str())) {
											if (CustomSelectable(U8("复制错误信息"), false, 8.0f) /*ImGui::Selectable(U8("复制错误信息"))*/) {
												ImGui::SetClipboardText(script.lastError.c_str());
											}
											ImGui::EndPopup();
										}
									}

									ImGui::PopID();
								}
							}
							ImGui::EndChild();
						}

						DrawAnimatedSeparator();
						EndTabRegion();
						ImGui::PopStyleVar();
						ImGui::EndTabItem();
					}

					ImGui::EndTabBar();

				}

				ImGui::PopStyleColor(5);
				ImGui::PopStyleVar(2);
			}

			ImGui::End();
			ImGui::PopStyleColor(2);
			ImGui::PopStyleVar(4);

			ImGui::PopFont();
		}

		g_Aimbot::Tick();
		g_DrawESP::DrawESP();
		LuaManager::Get().Lua_OnPaint();
	}
}