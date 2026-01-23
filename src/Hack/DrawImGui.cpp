#define NOMINMAX
#if defined(__cpp_char8_t)
#define U8(str) reinterpret_cast<const char*>(u8##str)
#else
#define U8(str) u8##str
#endif

#include "../Minimal-D3D12-Hook-ImGui-1.0.2/Main/mdx12_api.h"
#include "../CppSDK/SDK.hpp"
#include "ESP.h"
#include "Configs.h"
#include "DrawESP.h"
#include "DrawImGui.h"
#include "Aimbot.h"

#include <cstdio>
#include <string>
#include <algorithm>
#include <cfloat>

namespace ThemeColors {
    const ImVec4 BG = ImVec4(7.0f / 255.0f, 8.0f / 255.0f, 10.0f / 255.0f, 1.0f);
    const ImVec4 MUTED = ImVec4(154.0f / 255.0f, 163.0f / 255.0f, 178.0f / 255.0f, 1.0f);
    const ImVec4 TEXT = ImVec4(215.0f / 255.0f, 225.0f / 255.0f, 234.0f / 255.0f, 1.0f);
    const ImVec4 ACCENT = ImVec4(110.0f / 255.0f, 231.0f / 255.0f, 183.0f / 255.0f, 1.0f);
    const ImVec4 ACCENT2 = ImVec4(79.0f / 255.0f, 214.0f / 255.0f, 166.0f / 255.0f, 1.0f);
    const ImVec4 GLASS_BORDER = ImVec4(1.0f, 1.0f, 1.0f, 0.04f);
    const ImVec4 SHADOW = ImVec4(0.01f, 0.02f, 0.09f, 0.7f);
}

void SetupCustomImGuiStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 12.0f;
    style.ChildRounding = 12.0f;
    style.FrameRounding = 10.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 5.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 10.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;

    style.WindowPadding = ImVec2(20.0f, 20.0f);
    style.FramePadding = ImVec2(14.0f, 10.0f);
    style.CellPadding = ImVec2(8.0f, 8.0f);
    style.ItemSpacing = ImVec2(12.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 8.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 20.0f;

    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
    style.DisplaySafeAreaPadding = ImVec2(3.0f, 3.0f);

    style.Colors[ImGuiCol_Text] = ThemeColors::TEXT;
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(ThemeColors::MUTED.x, ThemeColors::MUTED.y, ThemeColors::MUTED.z, 0.5f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 0.95f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 0.8f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(14.0f / 255.0f, 16.0f / 255.0f, 18.0f / 255.0f, 0.96f);
    style.Colors[ImGuiCol_Border] = ThemeColors::GLASS_BORDER;
    style.Colors[ImGuiCol_BorderShadow] = ThemeColors::SHADOW;

    style.Colors[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.018f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.05f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);

    style.Colors[ImGuiCol_TitleBg] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 0.8f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 0.5f);

    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.02f, 0.02f, 0.03f, 0.8f);

    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.15f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);

    style.Colors[ImGuiCol_CheckMark] = ThemeColors::ACCENT;

    style.Colors[ImGuiCol_SliderGrab] = ThemeColors::ACCENT;
    style.Colors[ImGuiCol_SliderGrabActive] = ThemeColors::ACCENT2;

    style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 1.0f, 1.0f, 0.018f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.05f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);

    style.Colors[ImGuiCol_Header] = ImVec4(0.43f, 0.91f, 0.72f, 0.06f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.43f, 0.91f, 0.72f, 0.12f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.43f, 0.91f, 0.72f, 0.2f);

    style.Colors[ImGuiCol_Separator] = ThemeColors::GLASS_BORDER;
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);

    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.43f, 0.91f, 0.72f, 0.3f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.43f, 0.91f, 0.72f, 0.5f);

    style.Colors[ImGuiCol_Tab] = ImVec4(1.0f, 1.0f, 1.0f, 0.018f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.43f, 0.91f, 0.72f, 0.06f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.43f, 0.91f, 0.72f, 0.12f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(1.0f, 1.0f, 1.0f, 0.018f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);

    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.014f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.006f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.002f);

    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.43f, 0.91f, 0.72f, 0.3f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.43f, 0.91f, 0.72f, 0.5f);

    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.43f, 0.91f, 0.72f, 0.8f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.5f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.3f);

    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
}

namespace g_DrawImGui {
    static float g_saved_color[4] = { 1.0f,1.0f,1.0f,1.0f };
    static bool style_initialized = false;

    static float CalcPopupMinWidthForItems(const char* items[], int count)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        float max_text_w = 0.0f;
        for (int i = 0; i < count; ++i) {
            ImVec2 ts = ImGui::CalcTextSize(items[i]);
            if (ts.x > max_text_w) max_text_w = ts.x;
        }

        float extra = style.FramePadding.x * 2.0f + style.WindowPadding.x * 2.0f + 24.0f;
        return max_text_w + extra;
    }

    void DrawCustomColorPicker(const char* label_id, float* col_ptr)
    {
        float frame_h = ImGui::GetFrameHeight();
        float pad_y = ImGui::GetStyle().FramePadding.y;
        float btn_size = frame_h - pad_y * 2.0f;
        if (btn_size <= 0.0f) btn_size = frame_h * 0.8f;
        ImVec2 btn_size_vec(btn_size, btn_size);

        char button_id[256];
        char popup_id[256];
        std::snprintf(button_id, sizeof(button_id), "##ColorBtn_%s", label_id);
        std::snprintf(popup_id, sizeof(popup_id), "##ColorPopup_%s", label_id);

        ImVec4 cur = ImVec4(col_ptr[0], col_ptr[1], col_ptr[2], col_ptr[3]);

        ImGuiColorEditFlags colorbutton_flags = ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoTooltip;
        if (ImGui::ColorButton(button_id, cur, colorbutton_flags, btn_size_vec))
        {
            ImGui::OpenPopup(popup_id);
        }

        const char* popup_items[] = { U8("复制"), U8("粘贴") };
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup(popup_id);
        }

        float popup_min_w = CalcPopupMinWidthForItems(popup_items, IM_ARRAYSIZE(popup_items));
        ImGui::SetNextWindowSizeConstraints(ImVec2(popup_min_w, 0.0f), ImVec2(FLT_MAX, FLT_MAX));

        if (ImGui::BeginPopup(popup_id, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::MenuItem(U8("复制"))) {
                g_saved_color[0] = col_ptr[0]; g_saved_color[1] = col_ptr[1];
                g_saved_color[2] = col_ptr[2]; g_saved_color[3] = col_ptr[3];
            }
            if (ImGui::MenuItem(U8("粘贴"))) {
                col_ptr[0] = g_saved_color[0]; col_ptr[1] = g_saved_color[1];
                col_ptr[2] = g_saved_color[2]; col_ptr[3] = g_saved_color[3];
            }
            ImGui::EndPopup();
        }

        float pick_w = std::max(180.0f, frame_h * 9.0f);
        float pick_h = std::max(110.0f, frame_h * 5.0f);
        ImGui::SetNextWindowSize(ImVec2(pick_w, pick_h), ImGuiCond_Always);

        if (ImGui::BeginPopup(popup_id, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGuiColorEditFlags picker_flags =
                ImGuiColorEditFlags_AlphaBar |
                ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoOptions |
                ImGuiColorEditFlags_NoSmallPreview |
                ImGuiColorEditFlags_NoLabel |
                ImGuiColorEditFlags_NoTooltip;

#ifdef ImGuiColorEditFlags_NoSidePreview
            picker_flags = (ImGuiColorEditFlags)(picker_flags | ImGuiColorEditFlags_NoSidePreview);
#endif

            ImGui::ColorPicker4("##Picker", col_ptr, picker_flags);

            ImGui::EndPopup();
        }
    }

    bool DrawCustomCheckbox(const char* label, bool* v)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 0.03f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1.0f, 1.0f, 1.0f, 0.09f));

        bool res = ImGui::Checkbox(label, v);

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        return res;
    }

    bool DrawCustomSliderFloat(const char* label, float* v, float v_min, float v_max, const char* fmt = "%.1f")
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 0.03f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1.0f, 1.0f, 1.0f, 0.09f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ThemeColors::ACCENT);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ThemeColors::ACCENT2);

        bool r = ImGui::SliderFloat(label, v, v_min, v_max, fmt);

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
        return r;
    }

    void DrawColorPickerRow(const char* checkbox_label, bool* checkbox_value, const char* color_picker_label, float* color_value)
    {
        DrawCustomCheckbox(checkbox_label, checkbox_value);

        if (*checkbox_value) {
            float avail = ImGui::GetContentRegionAvail().x;
            float frame_h = ImGui::GetFrameHeight();
            float pad_y = ImGui::GetStyle().FramePadding.y;
            float btn_sz = frame_h - pad_y * 2.0f;
            if (btn_sz <= 0.0f) btn_sz = frame_h * 0.8f;

            float target = ImGui::GetCursorPosX() + avail - btn_sz;
            ImGui::SameLine();
            ImGui::SetCursorPosX(target);

            DrawCustomColorPicker(color_picker_label, color_value);
        }
    }

    void BeginTabRegion(const char* id)
    {
        ImGui::BeginChild(id, ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    }
    void EndTabRegion()
    {
        ImGui::EndChild();
    }

    void MyImGuiDraw(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags)
    {
        if (!style_initialized) { SetupCustomImGuiStyle(); style_initialized = true; }

        if (g_MDX12::g_MenuState::g_isOpen) {
            ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowBgAlpha(0.95f);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(7.0f / 255.0f, 8.0f / 255.0f, 10.0f / 255.0f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.02f));

            if (ImGui::Begin("Begeerte", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20.0f, 10.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(1.0f, 1.0f, 1.0f, 0.018f));
                ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.43f, 0.91f, 0.72f, 0.06f));
                ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.43f, 0.91f, 0.72f, 0.12f));
                ImGui::PushStyleColor(ImGuiCol_TabUnfocused, ImVec4(1.0f, 1.0f, 1.0f, 0.018f));
                ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(1.0f, 1.0f, 1.0f, 0.03f));

                if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None)) {

                    if (ImGui::BeginTabItem(U8("自瞄"))) {
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 12.0f));
                        BeginTabRegion("AimbotRegion");

                        ImGui::TextColored(ThemeColors::ACCENT, U8("[Test] 自动瞄准"));
                        ImGui::Separator();
                        DrawCustomCheckbox(U8("启用自瞄"), &g_Config::bAimbotEnabled);
                        if (g_Config::bAimbotEnabled) {
                            DrawCustomSliderFloat(U8("自瞄范围"), &g_Config::AimbotFOV, 1.0f, 180.0f, "%.0f");
                            DrawCustomSliderFloat(U8("自瞄平滑"), &g_Config::AimbotSmooth, 0.0f, 100.0f, "%.0f");
                        }
                        ImGui::Separator();

                        ImGui::TextColored(ThemeColors::ACCENT, U8("[Test] 自动扳机"));
                        ImGui::Separator();
                        DrawCustomCheckbox(U8("启用扳机"), &g_Config::bTriggerbotEnabled);
                        if (g_Config::bTriggerbotEnabled) {
                            DrawCustomSliderFloat(U8("扳机延迟"), &g_Config::TriggerDelay, 0.0f, 200.0f, "%.0f");
                            DrawCustomSliderFloat(U8("扳机随机延迟"), &g_Config::TriggerRandomPercent, 0.0f, 100.0f, "%.0f");
                            DrawCustomSliderFloat(U8("扳机命中率"), &g_Config::TriggerHitChance, 0.0f, 100.0f, "%.0f");
                        }
                        ImGui::Separator();

                        EndTabRegion();
                        ImGui::PopStyleVar();
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(U8("视觉"))) {
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 12.0f));
                        BeginTabRegion("VisualsRegion");

                        ImGui::TextColored(ThemeColors::ACCENT, U8("全局设置"));
                        ImGui::Separator();
                        DrawColorPickerRow(U8("方框"), &g_Config::bDrawBox, "BoxCol1", g_Config::BoxColor);
                        DrawColorPickerRow(U8("名称"), &g_Config::bDrawName, "NameCol1", g_Config::NameColor);
                        DrawCustomCheckbox(U8("血量"), &g_Config::bDrawHealthBar);
                        ImGui::Separator();

                        ImGui::TextColored(ThemeColors::ACCENT, U8("额外信息"));
                        ImGui::Separator();
                        DrawCustomCheckbox(U8("物种"), &g_Config::bDrawSpecies);
                        // DrawCustomCheckbox("Growth Stage", &g_Config::bDrawGrowth);
                        DrawColorPickerRow(U8("距离"), &g_Config::bDrawDistance, "DistCol1", g_Config::DistanceColor);
                        ImGui::Separator();

                        ImGui::TextColored(ThemeColors::ACCENT, U8("世界信息"));
                        ImGui::Separator();
                        DrawCustomCheckbox(U8("掉落的物品"), &g_Config::bDrawDroppedItems);
                        if (g_Config::bDrawDroppedItems) {
                            DrawCustomSliderFloat(U8("物品显示距离"), &g_Config::DroppedItemMaxDistance, 1.0f, 300.0f, "%.0f");
                        }
                        DrawCustomCheckbox(U8("显示宝箱"), &g_Config::bDrawSupplyDrops);
                        if (g_Config::bDrawDroppedItems) {
                            DrawCustomSliderFloat(U8("宝箱显示距离"), &g_Config::SupplyDropMaxDistance, 100.0f, 10000.0f, "%.0f");
                        }
                        ImGui::Separator();

                        DrawColorPickerRow(U8("显示视野外的威胁"), &g_Config::bEnableOOF, "OOFCol1", g_Config::OOFColor);
                        if (g_Config::bEnableOOF) {
                            float avail = ImGui::GetContentRegionAvail().x;
                            float frame_h = ImGui::GetFrameHeight();
                            float pad_y = ImGui::GetStyle().FramePadding.y;
                            float btn_sz = frame_h - pad_y * 2.0f;
                            if (btn_sz <= 0.0f) btn_sz = frame_h * 0.8f;
                            float target = ImGui::GetCursorPosX() + avail - btn_sz;
                            ImGui::SetCursorPosX(target);
                            ImGui::Dummy(ImVec2(0, 4.0f));
                            DrawCustomSliderFloat(U8("箭头尺寸"), &g_Config::OOFSize, 5.0f, 30.0f, "%.1f");
                            DrawCustomSliderFloat(U8("屏幕半径"), &g_Config::OOFRadius, 0.5f, 1.00f, "%.2f");
                            DrawCustomSliderFloat(U8("呼吸速度"), &g_Config::OOFBreathSpeed, 0.1f, 5.0f, "%.1f");
                            DrawCustomSliderFloat(U8("最小透明度"), &g_Config::OOFMinAlpha, 0.1f, 0.9f, "%.2f");
                            DrawCustomSliderFloat(U8("最大透明度"), &g_Config::OOFMaxAlpha, 0.2f, 1.0f, "%.2f");
                        }

                        EndTabRegion();
                        ImGui::PopStyleVar();
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(U8("队友"))) {
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 12.0f));
                        BeginTabRegion("TeamRegion");

                        ImGui::TextColored(ThemeColors::ACCENT, U8("队友设置"));
                        ImGui::Separator();
                        DrawColorPickerRow(U8("方框##Team"), &g_Config::bDrawBoxTeam, "BoxColTeam", g_Config::BoxColorTeam);
                        DrawColorPickerRow(U8("名称##Team"), &g_Config::bDrawNameTeam, "NameColTeam", g_Config::NameColorTeam);
                        DrawCustomCheckbox(U8("血量##Team"), &g_Config::bDrawHealthBarTeam);
                        ImGui::Separator();

                        ImGui::TextColored(ThemeColors::ACCENT, U8("额外信息"));
                        ImGui::Separator();
                        DrawCustomCheckbox(U8("物种##Team"), &g_Config::bDrawSpeciesTeam);
                        // DrawCustomCheckbox("Growth Stage##Team", &g_Config::bDrawGrowthTeam);
                        DrawColorPickerRow(U8("距离##Team"), &g_Config::bDrawDistanceTeam, "DistColTeam", g_Config::DistanceColorTeam);
                        ImGui::Separator();

                        EndTabRegion();
                        ImGui::PopStyleVar();
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(U8("生物列表"))) {
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 12.0f));
                        BeginTabRegion("EntityListRegion");

                        ImGui::TextColored(ThemeColors::ACCENT, U8("生物列表"));
                        ImGui::Separator();

                        ImGui::Checkbox(U8("应用筛选到全局视觉"), &g_Config::bEnableFilter);
                        ImGui::SameLine();

                        ImGui::PushItemWidth(-1.0f);
                        ImGui::InputTextWithHint("##EntitySearch", U8("输入生物名称进行过滤 (如: 南巨)..."), g_Config::entitySearchBuf, IM_ARRAYSIZE(g_Config::entitySearchBuf));
                        ImGui::PopItemWidth();

                        ImGui::Spacing();
                        if (ImGui::BeginChild("##EntityListChild", ImVec2(0, 0), true)) {

                            SDK::UWorld* World = SDK::UWorld::GetWorld();
                            SDK::APlayerController* LocalPC = g_ESP::GetLocalPC();

                            if (World && World->PersistentLevel && LocalPC && LocalPC->Pawn) {
                                SDK::TArray<SDK::AActor*>& Actors = World->PersistentLevel->Actors;
                                SDK::AActor* LocalPawn = LocalPC->Pawn;
                                std::string searchFilter = g_Config::entitySearchBuf;

                                for (int i = 0; i < Actors.Num(); i++) {
                                    SDK::AActor* TargetActor = Actors[i];

                                    if (!TargetActor || TargetActor == LocalPawn || TargetActor->bHidden) continue;
                                    if (!TargetActor->IsA(SDK::APrimalCharacter::StaticClass())) continue;

                                    SDK::APrimalCharacter* TargetChar = (SDK::APrimalCharacter*)TargetActor;
                                    if (TargetChar->IsDead()) continue;

                                    std::string displayName;
                                    if (TargetChar->PlayerState) {
                                        displayName = TargetChar->PlayerState->GetPlayerName().ToString();
                                    }
                                    else {
                                        displayName = TargetChar->GetDescriptiveName().ToString();
                                    }

                                    if (displayName.empty() || displayName == "None") continue;

                                    if (g_ESP::IsEntityMatch(displayName, g_Config::entitySearchBuf)) {
                                        float dist = LocalPC->Pawn->GetDistanceTo(TargetActor) / 100.0f;
                                        ImGui::Text("[%dm] %s", (int)dist, displayName.c_str());
                                    }
                                }
                            }
                            ImGui::EndChild();
                        }

                        ImGui::Separator();

                        EndTabRegion();
                        ImGui::PopStyleVar();
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();

                } // End TabBar

                ImGui::PopStyleColor(5);
                ImGui::PopStyleVar(2);
            } // End Window Begin content

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
            ImGui::End();
        } // End if menu open

        // ESP 绘制
        g_DrawESP::DrawESP();

        // Aimbot 调用
        g_Aimbot::Tick();
    }

} // namespace g_DrawImGui