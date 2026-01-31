#define NOMINMAX
#if defined(__cpp_char8_t)
#define U8(str) reinterpret_cast<const char*>(u8##str)
#else
#define U8(str) u8##str
#endif

#include "../Minimal-D3D12-Hook-ImGui-1.0.2/Main/mdx12_api.h"
#include "SDK_Headers.hpp"
#include "ESP.h"
#include "Configs.h"
#include "DrawESP.h"
#include "DrawImGui.h"
// #include "Aimbot.h"
#include "LuaManager.h"

#include <cstdio>
#include <string>
#include <algorithm>
#include <cfloat>
#include <cmath>

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

	style.WindowRounding = 16.0f;
	style.ChildRounding = 14.0f;
	style.FrameRounding = 12.0f;
	style.PopupRounding = 12.0f;
	style.ScrollbarRounding = 9.0f;
	style.GrabRounding = 10.0f;
	style.TabRounding = 12.0f;
	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.TabBorderSize = 0.0f;
	style.WindowPadding = ImVec2(20.0f, 20.0f);
	style.FramePadding = ImVec2(14.0f, 9.0f);
	style.CellPadding = ImVec2(10.0f, 8.0f);
	style.ItemSpacing = ImVec2(14.0f, 10.0f);
	style.ItemInnerSpacing = ImVec2(10.0f, 8.0f);
	style.IndentSpacing = 22.0f;
	style.ScrollbarSize = 12.0f;
	style.GrabMinSize = 20.0f;
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_None;
	style.ColorButtonPosition = ImGuiDir_Right;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
	style.DisplaySafeAreaPadding = ImVec2(3.0f, 3.0f);

	style.AntiAliasedLines = true;
	style.AntiAliasedFill = true;

	style.Colors[ImGuiCol_Text] = ThemeColors::TEXT;
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(ThemeColors::MUTED.x, ThemeColors::MUTED.y, ThemeColors::MUTED.z, 0.5f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 0.96f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 0.85f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(14.0f / 255.0f, 16.0f / 255.0f, 18.0f / 255.0f, 0.97f);
	style.Colors[ImGuiCol_Border] = ThemeColors::GLASS_BORDER;
	style.Colors[ImGuiCol_BorderShadow] = ThemeColors::SHADOW;
	style.Colors[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.065f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.095f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 0.85f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 0.5f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.02f, 0.02f, 0.03f, 0.85f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(1.0f, 1.0f, 1.0f, 0.12f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.18f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.24f);
	style.Colors[ImGuiCol_CheckMark] = ThemeColors::ACCENT;
	style.Colors[ImGuiCol_SliderGrab] = ThemeColors::ACCENT;
	style.Colors[ImGuiCol_SliderGrabActive] = ThemeColors::ACCENT2;
	style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.065f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.095f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.43f, 0.91f, 0.72f, 0.08f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.43f, 0.91f, 0.72f, 0.14f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.43f, 0.91f, 0.72f, 0.22f);
	style.Colors[ImGuiCol_Separator] = ThemeColors::GLASS_BORDER;
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.12f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.22f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.43f, 0.91f, 0.72f, 0.35f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.43f, 0.91f, 0.72f, 0.55f);
	style.Colors[ImGuiCol_Tab] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.43f, 0.91f, 0.72f, 0.08f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.43f, 0.91f, 0.72f, 0.15f);
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.04f);
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.018f);
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(1.0f, 1.0f, 1.0f, 0.04f);
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.008f);
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.003f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.43f, 0.91f, 0.72f, 0.35f);
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.43f, 0.91f, 0.72f, 0.55f);
	style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.43f, 0.91f, 0.72f, 0.85f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.55f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.85f);
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

		bool hovered = ImGui::IsItemHovered();
		static float hover_anim = 0.0f;
		if (hovered) hover_anim = std::min(hover_anim + ImGui::GetIO().DeltaTime * 8.0f, 1.0f);
		else hover_anim = std::max(hover_anim - ImGui::GetIO().DeltaTime * 8.0f, 0.0f);

		ImGuiColorEditFlags colorbutton_flags = ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoTooltip;

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		ImVec2 btn_pos = ImGui::GetCursorScreenPos();
		ImVec2 btn_center = ImVec2(btn_pos.x + btn_size * 0.5f, btn_pos.y + btn_size * 0.5f);

		if (hover_anim > 0.0f) {
			float glow_radius = btn_size * 0.5f + hover_anim * 4.0f;
			ImU32 glow_col = ImGui::GetColorU32(ImVec4(cur.x, cur.y, cur.z, 0.3f * hover_anim));
			draw_list->AddCircleFilled(btn_center, glow_radius, glow_col, 32);
		}

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
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 5.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 0.04f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1.0f, 1.0f, 1.0f, 0.12f));

		bool res = ImGui::Checkbox(label, v);

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
		return res;
	}

	bool DrawCustomSliderFloat(const char* label, float* v, float v_min, float v_max, const char* fmt = "%.1f", float step = 1.0f, const char* custom_text = nullptr)
	{
		if (v_max <= v_min) return false;

		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;

		ImGuiID id = window->GetID(label);
		ImVec2 label_size = ImGui::CalcTextSize(label);
		float full_w = ImGui::GetContentRegionAvail().x;
		const float TRACK_HEIGHT = 9.0f;
		const float HANDLE_RADIUS = 9.0f;
		const float VALUE_TEXT_PAD = 8.0f;

		ImVec2 pos = window->DC.CursorPos;
		ImVec2 size = ImVec2(full_w, std::max(style.FramePadding.y * 2.0f + TRACK_HEIGHT, label_size.y + style.FramePadding.y * 2.0f + 16.0f));
		ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
		ImGui::ItemSize(bb, style.FramePadding.y);
		if (!ImGui::ItemAdd(bb, id))
			return false;

		float label_area_width = label_size.x + style.ItemInnerSpacing.x;
		ImVec2 track_min = ImVec2(pos.x + label_area_width, pos.y + (size.y - TRACK_HEIGHT) * 0.5f);
		ImVec2 track_max = ImVec2(pos.x + size.x - style.FramePadding.x, track_min.y + TRACK_HEIGHT);

		ImVec2 track_size = ImVec2(track_max.x - track_min.x, track_max.y - track_min.y);
		if (track_size.x < 30.0f) {
			track_max.x = track_min.x + 30.0f;
			track_size.x = 30.0f;
		}

		ImGui::SetCursorScreenPos(pos);
		ImGui::InvisibleButton(label, bb.GetSize());
		bool hovered = ImGui::IsItemHovered();
		bool active = ImGui::IsItemActive();

		float t;
		{
			ImVec2 mp = g.IO.MousePos;
			float handle_min_x = track_min.x;
			float handle_max_x = track_max.x;
			if (active && g.IO.MouseDown[0]) {
				float mx = mp.x;
				t = (mx - handle_min_x) / (handle_max_x - handle_min_x);
			}
			else {
				t = (*v - v_min) / (v_max - v_min);
			}
			if (t < 0.0f) t = 0.0f;
			if (t > 1.0f) t = 1.0f;
		}

		bool value_changed = false;
		if (active && g.IO.MouseDown[0]) {
			float mx = g.IO.MousePos.x;
			float handle_min_x = track_min.x;
			float handle_max_x = track_max.x;
			float new_t = (mx - handle_min_x) / (handle_max_x - handle_min_x);
			new_t = new_t < 0.0f ? 0.0f : (new_t > 1.0f ? 1.0f : new_t);
			float new_value = v_min + new_t * (v_max - v_min);

			if (step > 0.0f) {
				float steps_f = roundf((new_value - v_min) / step);
				new_value = v_min + steps_f * step;
				if (new_value < v_min) new_value = v_min;
				if (new_value > v_max) new_value = v_max;
			}

			if (new_value != *v) {
				*v = new_value;
				value_changed = true;
			}
			t = (*v - v_min) / (v_max - v_min);
			if (t < 0.0f) t = 0.0f;
			if (t > 1.0f) t = 1.0f;
		}

		ImDrawList* draw = ImGui::GetWindowDrawList();

		ImU32 col_track_bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
		ImU32 col_fill = ImGui::GetColorU32(ImGuiCol_SliderGrab);
		ImU32 col_fill2 = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
		ImU32 col_handle = ImGui::GetColorU32(ImGuiCol_SliderGrab);
		ImU32 col_handle_active = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
		ImU32 col_text = ImGui::GetColorU32(ImGuiCol_Text);

		float track_rounding = TRACK_HEIGHT * 0.5f;

		draw->AddRectFilled(
			ImVec2(track_min.x - 1.0f, track_min.y - 1.0f),
			ImVec2(track_max.x + 1.0f, track_max.y + 1.0f),
			IM_COL32(110, 231, 183, 10),
			track_rounding + 1.0f
		);
		draw->AddRectFilled(track_min, track_max, col_track_bg, track_rounding);

		ImVec2 filled_max = ImVec2(track_min.x + track_size.x * t, track_max.y);
		if (filled_max.x > track_min.x + 1.0f) {
			ImU32 colA = col_fill;
			ImU32 colB = col_fill2;
			draw->AddRectFilled(track_min, filled_max, colA, track_rounding);
			ImVec2 grad_mid = ImVec2((track_min.x + filled_max.x) * 0.5f, track_min.y);
			draw->AddRectFilledMultiColor(
				ImVec2(grad_mid.x, track_min.y),
				filled_max,
				colA, colB, colB, colA
			);
		}

		ImVec2 handle_center = ImVec2(track_min.x + track_size.x * t, (track_min.y + track_max.y) * 0.5f);

		float now = ImGui::GetTime();
		float pulse = 1.0f;
		if (active) {
			pulse = 1.0f + 0.12f * (float)(sin(now * 14.0f) * 0.5f + 0.5f);
		}
		else if (hovered) {
			pulse = 1.0f + 0.05f * (float)(sin(now * 8.0f) * 0.5f + 0.5f);
		}

		float handle_radius = HANDLE_RADIUS * pulse;

		ImU32 col_shadow = ImGui::GetColorU32(ImGuiCol_BorderShadow);
		draw->AddCircleFilled(ImVec2(handle_center.x + 1.0f, handle_center.y + 2.0f), handle_radius + 5.0f, col_shadow, 24);

		if (active || hovered) {
			float glow_intensity = active ? 0.4f : 0.2f;
			draw->AddCircleFilled(handle_center, handle_radius + 6.0f,
				IM_COL32(110, 231, 183, (int)(glow_intensity * 255)), 24);
		}

		draw->AddCircleFilled(handle_center, handle_radius, active ? col_handle_active : col_handle, 24);
		draw->AddCircle(handle_center, handle_radius * 0.7f, IM_COL32(255, 255, 255, 30), 24, 2.5f);

		ImVec2 label_pos = ImVec2(pos.x, pos.y + (size.y - label_size.y) * 0.5f);
		draw->AddText(label_pos, col_text, label);

		char valbuf[128];
		int n = std::snprintf(valbuf, sizeof(valbuf), fmt, *v);
		if (custom_text && custom_text[0] != '\0') {
			size_t cur = (n > 0 && n < (int)sizeof(valbuf)) ? (size_t)n : strlen(valbuf);
			std::snprintf(valbuf + cur, sizeof(valbuf) - cur, "%s", custom_text);
		}

		ImVec2 value_text_size = ImGui::CalcTextSize(valbuf);
		ImVec2 value_pos = ImVec2(handle_center.x - value_text_size.x * 0.5f, track_min.y - VALUE_TEXT_PAD - value_text_size.y);

		float left_limit = track_min.x;
		float right_limit = track_max.x - value_text_size.x;
		if (value_pos.x < left_limit) value_pos.x = left_limit;
		if (value_pos.x > right_limit) value_pos.x = right_limit;

		ImVec2 val_bb_min = ImVec2(value_pos.x - 7.0f, value_pos.y - 4.0f);
		ImVec2 val_bb_max = ImVec2(value_pos.x + value_text_size.x + 7.0f, value_pos.y + value_text_size.y + 4.0f);
		draw->AddRectFilled(val_bb_min, val_bb_max, IM_COL32(0, 0, 0, 140), 7.0f);
		draw->AddRect(val_bb_min, val_bb_max, IM_COL32(110, 231, 183, 60), 7.0f, 0, 1.0f);
		draw->AddText(value_pos, col_text, valbuf);

		return value_changed;
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
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(7.0f / 255.0f, 8.0f / 255.0f, 10.0f / 255.0f, 0.88f));
		ImGui::BeginChild(id, ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	}

	void EndTabRegion()
	{
		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}

	void DrawAnimatedSeparator()
	{
		ImDrawList* draw = ImGui::GetWindowDrawList();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		float width = ImGui::GetContentRegionAvail().x;
		float time = ImGui::GetTime();

		float x1 = pos.x;
		float x2 = pos.x + width;
		float y = pos.y;

		draw->AddRectFilledMultiColor(
			ImVec2(x1, y),
			ImVec2(x2, y + 1.0f),
			IM_COL32(110, 231, 183, 0),
			IM_COL32(110, 231, 183, 80),
			IM_COL32(110, 231, 183, 80),
			IM_COL32(110, 231, 183, 0)
		);

		ImGui::Dummy(ImVec2(0, 2.0f));
	}

	void MyImGuiDraw(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags)
	{
		if (!style_initialized) { SetupCustomImGuiStyle(); style_initialized = true; }

		static float g_MenuAlpha = 0.0f;
		const float FADE_SPEED = 5.0f;

		static bool g_PrevMenuState = false;
		bool isNowOpen = g_MDX12::g_MenuState::g_isOpen;

		if (isNowOpen && !g_PrevMenuState) {
			LuaManager::Get().RefreshFileList();
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
			ImGui::SetNextWindowSize(ImVec2(640, 640), ImGuiCond_FirstUseEver);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_MenuAlpha);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 22.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(7.0f / 255.0f, 8.0f / 255.0f, 10.0f / 255.0f, 0.97f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(110.0f / 255.0f, 231.0f / 255.0f, 183.0f / 255.0f, 0.08f));

			ImGuiWindowFlags wFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
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

					if (ImGui::BeginTabItem(U8("视觉"))) {
						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14.0f, 14.0f));
						BeginTabRegion("VisualsRegion");

						ImGui::TextColored(ThemeColors::ACCENT, U8("全局设置"));
						DrawAnimatedSeparator();
						DrawColorPickerRow(U8("方框"), &g_Config::bDrawBox, "BoxCol1", g_Config::BoxColor);
						DrawColorPickerRow(U8("名称"), &g_Config::bDrawName, "NameCol1", g_Config::NameColor);
						DrawCustomCheckbox(U8("血量"), &g_Config::bDrawHealthBar);
						DrawCustomCheckbox(U8("眩晕"), &g_Config::bDrawTorpor);
						DrawAnimatedSeparator();

						ImGui::TextColored(ThemeColors::ACCENT, U8("额外信息"));
						DrawAnimatedSeparator();
						DrawCustomCheckbox(U8("物种"), &g_Config::bDrawSpecies);
						DrawColorPickerRow(U8("距离"), &g_Config::bDrawDistance, "DistCol1", g_Config::DistanceColor);
						DrawAnimatedSeparator();

						ImGui::TextColored(ThemeColors::ACCENT, U8("世界信息"));
						DrawAnimatedSeparator();

						DrawCustomCheckbox(U8("掉落的物品"), &g_Config::bDrawDroppedItems);
						if (g_Config::bDrawDroppedItems) {
							DrawCustomSliderFloat(U8("物品显示距离"), &g_Config::DroppedItemMaxDistance, 1.0f, 500.0f, "%.0f", 1.0f, "m");
						}
						DrawAnimatedSeparator();

						DrawCustomCheckbox(U8("显示建筑"), &g_Config::bDrawStructures);
						if (g_Config::bDrawStructures) {
							DrawCustomSliderFloat(U8("建筑显示距离"), &g_Config::StructureMaxDistance, 1.0f, 10000.0f, "%.0f", 1.0f, "m");
						}
						DrawAnimatedSeparator();

						DrawCustomCheckbox(U8("显示水源"), &g_Config::bDrawWater);
						if (g_Config::bDrawWater) {
							DrawCustomSliderFloat(U8("水源显示距离"), &g_Config::WaterMaxDistance, 1.0f, 10000.0f, "%.0f", 1.0f, "m");
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
						DrawCustomCheckbox(U8("眩晕##Team"), &g_Config::bDrawTorporTeam);
						DrawAnimatedSeparator();

						ImGui::TextColored(ThemeColors::ACCENT, U8("额外信息"));
						DrawAnimatedSeparator();
						DrawCustomCheckbox(U8("物种##Team"), &g_Config::bDrawSpeciesTeam);
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

						DrawAnimatedSeparator();

						EndTabRegion();
						ImGui::PopStyleVar();
						ImGui::EndTabItem();
					}

					LuaManager::Get().Update();
					if (ImGui::BeginTabItem(U8("脚本"))) {
						static bool initialized = false;
						if (!initialized) {
							LuaManager::Get().Initialize("lua");
							initialized = true;
						}

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
								ImGui::TextDisabled(U8(""));
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

									if (ImGui::Selectable(displayName.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
										bool targetState = !script.isLoaded;
										if (targetState) {
											if (!mgr.SetScriptState(i, true)) {

											}
										}
										else {
											mgr.SetScriptState(i, false);
										}
									}

									ImGui::PopStyleColor();

									if (script.hasError) {

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

			ImGui::PopStyleColor(2);
			ImGui::PopStyleVar(2);
			ImGui::End();

			ImGui::PopStyleVar();
		}

		g_DrawESP::DrawESP();
		LuaManager::Get().Lua_OnPaint();
	}
}