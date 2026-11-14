#if defined(__cpp_char8_t)
#define U8(str) reinterpret_cast<const char*>(u8##str)
#else
#define U8(str) u8##str
#endif

#include "../../external/Minimal-D3D12-Hook-ImGui/Main/mdx12_api.h"
#include "../Config/Configs.h"
#include "DrawImGui.h"
#include "../Util/Util.h"
#include "../Language/LanguageManager.h"

#include <cstdio>
#include <string>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <map>

namespace g_DrawImGui {
	namespace ThemeColors {
		// 基础背景色 (7, 8, 10)
		const ImVec4 BG = ImVec4(0.027f, 0.031f, 0.039f, 1.000f);
		// 菜单栏背景
		const ImVec4 MENUBAR_BG = ImVec4(0.020f, 0.020f, 0.030f, 0.850f);

		// 次要文字/禁用色 (154, 163, 178)
		const ImVec4 MUTED = ImVec4(0.604f, 0.639f, 0.698f, 1.000f);
		// 主文字色 (215, 225, 234)
		const ImVec4 TEXT = ImVec4(0.843f, 0.882f, 0.918f, 1.000f);

		// 主题强调色 (110, 231, 183)
		// const ImVec4 ACCENT = ImGui::ColorConvertU32ToFloat4(g_Util::GetU32Color(g_Config::MenuColor));
		inline ImVec4 GetAccent() { return ImGui::ColorConvertU32ToFloat4(g_Util::GetU32Color(g_Config::MenuColor)); }

		// 主题强调色2 (79, 214, 166)
		const ImVec4 ACCENT2 = ImVec4(0.310f, 0.839f, 0.651f, 1.000f);

		// 边框与阴影
		const ImVec4 GLASS_BORDER = ImVec4(1.000f, 1.000f, 1.000f, 0.040f);
		const ImVec4 SHADOW = ImVec4(0.010f, 0.020f, 0.090f, 0.700f);

		// 交互辅助色
		const ImVec4 WHITE_LOW = ImVec4(1.000f, 1.000f, 1.000f, 0.025f);
		const ImVec4 WHITE_MID = ImVec4(1.000f, 1.000f, 1.000f, 0.065f);
		const ImVec4 WHITE_HIGH = ImVec4(1.000f, 1.000f, 1.000f, 0.120f);

		const ImVec4 BLACK_LOW = ImVec4(0.000f, 0.000f, 0.000f, 0.350f);
		const ImVec4 BLACK_MID = ImVec4(0.000f, 0.000f, 0.000f, 0.850f);
	}

	inline void SetupCustomImGuiStyle()
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
		style.Colors[ImGuiCol_PopupBg] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 0.97f);
		style.Colors[ImGuiCol_Border] = ThemeColors::GLASS_BORDER;
		style.Colors[ImGuiCol_BorderShadow] = ThemeColors::SHADOW;

		style.Colors[ImGuiCol_FrameBg] = ImVec4(ThemeColors::WHITE_LOW);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(ThemeColors::WHITE_MID);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.095f);

		style.Colors[ImGuiCol_TitleBg] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 0.85f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 1.0f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 0.5f);

		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(ThemeColors::MENUBAR_BG);

		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(ThemeColors::WHITE_HIGH);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.18f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.24f);

		style.Colors[ImGuiCol_CheckMark] = ThemeColors::GetAccent();

		style.Colors[ImGuiCol_SliderGrab] = ThemeColors::GetAccent();
		style.Colors[ImGuiCol_SliderGrabActive] = ThemeColors::GetAccent(); // ThemeColors::ACCENT2

		style.Colors[ImGuiCol_Button] = ImVec4(ThemeColors::WHITE_LOW);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.065f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.095f);

		style.Colors[ImGuiCol_Header] = ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.08f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.14f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.22f);

		style.Colors[ImGuiCol_Separator] = ThemeColors::GLASS_BORDER;
		style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.12f);
		style.Colors[ImGuiCol_SeparatorActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.22f);

		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.35f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.55f);

		style.Colors[ImGuiCol_Tab] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
		style.Colors[ImGuiCol_TabHovered] = ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.08f);
		style.Colors[ImGuiCol_TabActive] = ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.15f);
		style.Colors[ImGuiCol_TabUnfocused] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
		style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.04f);
		style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.018f);
		style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(1.0f, 1.0f, 1.0f, 0.04f);
		style.Colors[ImGuiCol_TableBorderLight] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
		style.Colors[ImGuiCol_TableRowBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.008f);
		style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.003f);

		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.35f);
		style.Colors[ImGuiCol_DragDropTarget] = ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.55f);
		style.Colors[ImGuiCol_NavHighlight] = ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.85f);
		style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.55f);
		style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(ThemeColors::BLACK_LOW);
		style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(ThemeColors::BLACK_MID);
	}

	extern float g_MenuAlpha;
	static float g_saved_color[4] = { 1.0f,1.0f,1.0f,1.0f };
	static bool style_initialized = false;

	struct CustomComboItem {
		const char* label = nullptr; // 子项显示的文本
		bool        selected = false;   // 子项自身在下拉框中的选中状态
		float* color_ptr = nullptr; // 调色盘绑定的颜色指针（float[4]），为 nullptr 则不显示调色盘
		bool* checkbox_ptr = nullptr;  // 外部绑定的逻辑指针。若不为 nullptr，其勾选状态将与外部变量同步（等价于复选框）
	};

	struct ColorPickerData {
		const char* label_id;
		float* col_ptr;
	};

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

	inline void DrawCustomColorPicker(const char* label_id, float* col_ptr, bool active, const char* display_text)
	{
		ImGuiContext& g = *GImGui;
		ImGuiStorage* storage = ImGui::GetStateStorage();
		ImGuiID anim_id = ImGui::GetID(label_id);

		float enable_anim = storage->GetFloat(anim_id + 1, 0.0f);
		enable_anim = ImLinearSweep(enable_anim, active ? 1.0f : 0.0f, g.IO.DeltaTime * 6.0f);
		storage->SetFloat(anim_id + 1, enable_anim);

		float frame_h = ImGui::GetFrameHeight();
		float pad_y = g.Style.FramePadding.y;
		float btn_size = frame_h - pad_y * 2.0f;
		if (btn_size <= 0.0f) btn_size = frame_h * 0.8f;
		ImVec2 btn_size_vec(btn_size, btn_size);

		ImGui::BeginGroup();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 5.0f));

		if (display_text && display_text[0] != '\0')
		{
			ImGui::AlignTextToFramePadding();
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, enable_anim * g_MenuAlpha);
			const char* text_end = ImGui::FindRenderedTextEnd(display_text);
			ImGui::TextUnformatted(display_text, text_end);
			ImGui::PopStyleVar();

			ImGui::SameLine();
			float avail_x = ImGui::GetContentRegionAvail().x;
			if (avail_x > btn_size) {
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail_x - btn_size);
			}
		}

		if (enable_anim > 0.0f)
		{
			char button_id[256];
			char popup_picker_id[256];
			char popup_menu_id[256];
			std::snprintf(button_id, sizeof(button_id), "##ColorBtn_%s", label_id);
			std::snprintf(popup_picker_id, sizeof(popup_picker_id), "##ColorPickerPopup_%s", label_id);
			std::snprintf(popup_menu_id, sizeof(popup_menu_id), "##ColorMenuPopup_%s", label_id);

			ImVec4 cur = ImVec4(col_ptr[0], col_ptr[1], col_ptr[2], col_ptr[3]);
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2 btn_pos = ImGui::GetCursorScreenPos();
			ImVec2 btn_center = ImVec2(btn_pos.x + btn_size * 0.5f, btn_pos.y + btn_size * 0.5f);

			float final_alpha = enable_anim * g_MenuAlpha;
			float hover_anim = storage->GetFloat(anim_id, 0.0f);

			if (hover_anim > 0.0f && final_alpha > 0.01f) {
				float glow_radius = btn_size * 0.5f + hover_anim * 4.0f;
				ImU32 glow_col = ImGui::GetColorU32(ImVec4(cur.x, cur.y, cur.z, 0.3f * hover_anim * final_alpha));
				draw_list->AddCircleFilled(btn_center, glow_radius, glow_col, 32);
			}

			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, final_alpha);
			if (ImGui::ColorButton(button_id, cur, ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoTooltip, btn_size_vec))
			{
				ImGui::OpenPopup(popup_picker_id);
			}
			ImGui::PopStyleVar();

			bool hovered = ImGui::IsItemHovered();
			hover_anim = hovered ? std::min(hover_anim + g.IO.DeltaTime * 8.0f, 1.0f) : std::max(hover_anim - g.IO.DeltaTime * 8.0f, 0.0f);
			storage->SetFloat(anim_id, hover_anim);

			if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
			{
				ImGui::OpenPopup(popup_menu_id);
			}

			// ==================== 弹窗 1：纯右键菜单（复制/粘贴） ====================
			const char* popup_items[] = { LanguageManager::ConfigImGui_Menu::Copy, LanguageManager::ConfigImGui_Menu::Paste };
			float padding_w = g.Style.WindowPadding.x * 2.0f;

			// 计算文本宽度，用于完美对齐和匹配
			float text_only_w = ImGui::CalcTextSize(LanguageManager::ConfigImGui_Menu::Copy).x;
			// 精确计算“复制/粘贴”四个字的实际占用宽度，扣除任何默认对齐边距
			float tight_w = text_only_w + padding_w + g.Style.FramePadding.x * 2.0f;

			// 严格限制弹窗的最小和最大宽度，使其完美匹配“复制/粘贴”四个字的宽度。
			ImGui::SetNextWindowSizeConstraints(ImVec2(tight_w, 0.0f), ImVec2(tight_w, FLT_MAX));

			if (ImGui::BeginPopup(popup_menu_id, ImGuiWindowFlags_AlwaysAutoResize))
			{
				// 临时消除 Item 内部的额外边距，强制菜单项自适应最窄宽度
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, g.Style.ItemSpacing.y));

				// 临时将圆角（FrameRounding）设置为你需要的值，用于此弹出窗口中的所有控件。
				// 这是解决悬浮效果是方角矩形的核心。
				float original_frame_rounding = g.Style.FrameRounding;
				g.Style.FrameRounding = 4.0f; // 强制在此弹出窗口中使用 4.0 的圆角

				// 我们不使用 MenuItem，因为 MenuItem 默认设计就是直角，并且不提供自定义圆角的选项。
				// 我们自定义一个“圆角、文本居中、宽度匹配”的按钮来替代它。

				// 计算按钮的宽度：窗口的可用宽度扣除窗口内部边距
				float btn_w = ImGui::GetContentRegionAvail().x;

				// 绘制“复制”按钮
				if (ImGui::Button(LanguageManager::ConfigImGui_Menu::Copy, ImVec2(btn_w, 0))) {
					g_saved_color[0] = col_ptr[0]; g_saved_color[1] = col_ptr[1];
					g_saved_color[2] = col_ptr[2]; g_saved_color[3] = col_ptr[3];
					ImGui::CloseCurrentPopup(); // 点击按钮后关闭弹出窗口
				}

				// 绘制“粘贴”按钮
				if (ImGui::Button(LanguageManager::ConfigImGui_Menu::Paste, ImVec2(btn_w, 0))) {
					col_ptr[0] = g_saved_color[0]; col_ptr[1] = g_saved_color[1];
					col_ptr[2] = g_saved_color[2]; col_ptr[3] = g_saved_color[3];
					ImGui::CloseCurrentPopup(); // 点击按钮后关闭弹出窗口
				}

				// 恢复原来的 FrameRounding 设置
				g.Style.FrameRounding = original_frame_rounding;

				ImGui::PopStyleVar();
				ImGui::EndPopup();
			}

			// ==================== 弹窗 2：纯左键菜单（只显示调色盘） ====================
			ImGui::SetNextWindowSizeConstraints(ImVec2(frame_h * 9.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
			if (ImGui::BeginPopup(popup_picker_id, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::SetNextItemWidth(frame_h * 9.0f);
				// 此处加入了 ImGuiColorEditFlags_NoSidePreview 标志，用于干掉右侧的 "Current" 预览区域
				ImGui::ColorPicker4("##Picker", col_ptr, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoSidePreview);
				ImGui::EndPopup();
			}
		}
		else
		{
			ImGui::Dummy(btn_size_vec);
		}

		ImGui::PopStyleVar();
		ImGui::EndGroup();
	}

	inline void DrawCustomColorPicker(const char* label_id, float* col_ptr, const char* display_text)
	{
		DrawCustomColorPicker(label_id, col_ptr, true, display_text);
	}

	inline void DrawMultiColorPicker(const char* display_text, bool active, std::initializer_list<ColorPickerData> pickers)
	{
		ImGuiContext& g = *GImGui;
		ImGuiStorage* storage = ImGui::GetStateStorage();

		float frame_h = ImGui::GetFrameHeight();
		float pad_y = g.Style.FramePadding.y;
		float btn_size = frame_h - pad_y * 2.0f;
		if (btn_size <= 0.0f) btn_size = frame_h * 0.8f;
		ImVec2 btn_size_vec(btn_size, btn_size);
		float item_spacing_x = g.Style.ItemSpacing.x;

		ImGui::BeginGroup();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 5.0f));

		// 1. 处理左侧文本
		if (display_text && display_text[0] != '\0')
		{
			ImGui::AlignTextToFramePadding();
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_MenuAlpha);
			const char* text_end = ImGui::FindRenderedTextEnd(display_text);
			ImGui::TextUnformatted(display_text, text_end);
			ImGui::PopStyleVar();
		}

		int index = 0;
		int total_count = (int)pickers.size();

		// 2. 渲染颜色选择器
		for (const auto& picker : pickers)
		{
			// 每个 Picker 都必须在同一行，除非它是 Group 的第一个元素且没文本
			// 我们直接在循环开始就强制 SameLine，ImGui 会自动处理第一个元素的特殊情况
			if (index > 0 || (display_text && display_text[0] != '\0'))
				ImGui::SameLine();

			ImGuiID anim_id = ImGui::GetID(picker.label_id);
			float* col_ptr = picker.col_ptr;

			float enable_anim = storage->GetFloat(anim_id + 1, 0.0f);
			enable_anim = ImLinearSweep(enable_anim, active ? 1.0f : 0.0f, g.IO.DeltaTime * 6.0f);
			storage->SetFloat(anim_id + 1, enable_anim);

			if (enable_anim > 0.0f)
			{
				// --- 核心修复：精确对齐计算 ---
				float remaining_items_count = (float)(total_count - index);
				float total_needed_w = (remaining_items_count * btn_size) + ((remaining_items_count - 1) * item_spacing_x);

				float avail_x = ImGui::GetContentRegionAvail().x;
				if (avail_x > total_needed_w) {
					// 使用 SetCursorPosX 之前不应有其他换行逻辑
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail_x - total_needed_w);
				}

				char button_id[256];
				char popup_picker_id[256];
				char popup_menu_id[256];
				std::snprintf(button_id, sizeof(button_id), "##ColorBtn_%s", picker.label_id);
				std::snprintf(popup_picker_id, sizeof(popup_picker_id), "##ColorPickerPopup_%s", picker.label_id);
				std::snprintf(popup_menu_id, sizeof(popup_menu_id), "##ColorMenuPopup_%s", picker.label_id);

				ImVec4 cur = ImVec4(col_ptr[0], col_ptr[1], col_ptr[2], col_ptr[3]);
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				ImVec2 btn_pos = ImGui::GetCursorScreenPos();
				ImVec2 btn_center = ImVec2(btn_pos.x + btn_size * 0.5f, btn_pos.y + btn_size * 0.5f);

				float final_alpha = enable_anim * g_MenuAlpha;
				float hover_anim = storage->GetFloat(anim_id, 0.0f);

				if (hover_anim > 0.0f && final_alpha > 0.01f) {
					float glow_radius = btn_size * 0.5f + hover_anim * 4.0f;
					ImU32 glow_col = ImGui::GetColorU32(ImVec4(cur.x, cur.y, cur.z, 0.3f * hover_anim * final_alpha));
					draw_list->AddCircleFilled(btn_center, glow_radius, glow_col, 32);
				}

				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, final_alpha);
				if (ImGui::ColorButton(button_id, cur, ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoTooltip, btn_size_vec))
				{
					ImGui::OpenPopup(popup_picker_id);
				}
				ImGui::PopStyleVar();

				bool hovered = ImGui::IsItemHovered();
				hover_anim = hovered ? std::min(hover_anim + g.IO.DeltaTime * 8.0f, 1.0f) : std::max(hover_anim - g.IO.DeltaTime * 8.0f, 0.0f);
				storage->SetFloat(anim_id, hover_anim);

				if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
				{
					ImGui::OpenPopup(popup_menu_id);
				}

				// 右键菜单
				float padding_w = g.Style.WindowPadding.x * 2.0f;
				float text_only_w = ImGui::CalcTextSize(LanguageManager::ConfigImGui_Menu::Copy).x;
				float tight_w = text_only_w + padding_w + g.Style.FramePadding.x * 2.0f;
				ImGui::SetNextWindowSizeConstraints(ImVec2(tight_w, 0.0f), ImVec2(tight_w, FLT_MAX));
				if (ImGui::BeginPopup(popup_menu_id, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, g.Style.ItemSpacing.y));
					float original_frame_rounding = g.Style.FrameRounding;
					g.Style.FrameRounding = 4.0f;
					float btn_w = ImGui::GetContentRegionAvail().x;
					if (ImGui::Button(LanguageManager::ConfigImGui_Menu::Copy, ImVec2(btn_w, 0))) {
						g_saved_color[0] = col_ptr[0]; g_saved_color[1] = col_ptr[1];
						g_saved_color[2] = col_ptr[2]; g_saved_color[3] = col_ptr[3];
						ImGui::CloseCurrentPopup();
					}
					if (ImGui::Button(LanguageManager::ConfigImGui_Menu::Paste, ImVec2(btn_w, 0))) {
						col_ptr[0] = g_saved_color[0]; col_ptr[1] = g_saved_color[1];
						col_ptr[2] = g_saved_color[2]; col_ptr[3] = g_saved_color[3];
						ImGui::CloseCurrentPopup();
					}
					g.Style.FrameRounding = original_frame_rounding;
					ImGui::PopStyleVar();
					ImGui::EndPopup();
				}

				// 左键 Picker
				ImGui::SetNextWindowSizeConstraints(ImVec2(frame_h * 9.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
				if (ImGui::BeginPopup(popup_picker_id, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::SetNextItemWidth(frame_h * 9.0f);
					ImGui::ColorPicker4("##Picker", col_ptr, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoSidePreview);
					ImGui::EndPopup();
				}
			}
			else
			{
				// 隐藏状态下也保持在当前行，使用 Dummy 占位或跳过
				ImGui::Dummy(ImVec2(0, 0));
			}

			index++;
		}

		ImGui::PopStyleVar();
		ImGui::EndGroup();
	}

	inline bool DrawCustomCheckbox(const char* label, bool* v)
	{
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;
		if (window->SkipItems) return false;

		ImGuiID id = window->GetID(label);
		ImGuiStorage* storage = ImGui::GetStateStorage();

		// 1. 动画状态机
		float check_anim = storage->GetFloat(id, *v ? 1.0f : 0.0f);
		check_anim = ImLinearSweep(check_anim, *v ? 1.0f : 0.0f, g.IO.DeltaTime * 8.0f);
		storage->SetFloat(id, check_anim);

		float hover_anim = storage->GetFloat(id + 1, 0.0f);

		// 2. 尺寸与圆角独立设定 (摆脱全局圆角变成圆形的 BUG)
		float square_sz = g.FontSize * 1.4f; // 尺寸放大，保证能看清内部层次 (约 20-22px)
		float box_rounding = square_sz * 0.2f; // 强制保持适度的圆角矩形 (约 4px)

		const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

		// 3. 完美的垂直居中与包围盒计算
		float frame_h = ImGui::GetFrameHeight();
		float pad_y = g.Style.FramePadding.y;

		ImVec2 pos = window->DC.CursorPos;
		float total_w = square_sz + (label_size.x > 0.0f ? g.Style.ItemInnerSpacing.x + label_size.x : 0.0f);
		ImRect total_bb(pos, ImVec2(pos.x + total_w, pos.y + frame_h));

		ImGui::ItemSize(total_bb, pad_y);
		if (!ImGui::ItemAdd(total_bb, id)) return false;

		// 4. 交互逻辑
		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
		if (pressed) {
			*v = !(*v);
			ImGui::MarkItemEdited(id);
		}

		hover_anim = hovered ? std::min(hover_anim + g.IO.DeltaTime * 8.0f, 1.0f) : std::max(hover_anim - g.IO.DeltaTime * 8.0f, 0.0f);
		storage->SetFloat(id + 1, hover_anim);

		// ==========================================
		// 5. 高质感分层渲染 (圆角矩形)
		// ==========================================
		ImDrawList* draw_list = window->DrawList;
		float final_alpha = g_MenuAlpha;
		ImVec4 accent_color = ThemeColors::GetAccent();

		// 方块的 Y 轴坐标，保证和行高完美居中
		float box_offset_y = (frame_h - square_sz) * 0.5f;
		ImVec2 box_min = ImVec2(pos.x, pos.y + box_offset_y);
		ImVec2 box_max = ImVec2(pos.x + square_sz, pos.y + box_offset_y + square_sz);

		// [背景层 Glow] 悬浮时的外发光圈 (改成矩形发光)
		if (hover_anim > 0.0f && final_alpha > 0.01f) {
			float glow_expand = 3.0f * hover_anim;
			ImVec2 glow_min = ImVec2(box_min.x - glow_expand, box_min.y - glow_expand);
			ImVec2 glow_max = ImVec2(box_max.x + glow_expand, box_max.y + glow_expand);
			ImU32 glow_col = ImGui::GetColorU32(ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.25f * hover_anim * final_alpha));
			draw_list->AddRectFilled(glow_min, glow_max, glow_col, box_rounding + glow_expand);
		}

		// [第一层] 底座背景 (解决 OFF 状态黑洞问题)
		// 未启用时用正常的 WHITE_LOW (像普通的输入框背景)，启用时微微泛出强调色的底光
		ImVec4 base_col_off = ThemeColors::WHITE_LOW;
		ImVec4 base_col_on = ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.15f);
		ImVec4 bg_col = ImVec4(
			ImLerp(base_col_off.x, base_col_on.x, check_anim),
			ImLerp(base_col_off.y, base_col_on.y, check_anim),
			ImLerp(base_col_off.z, base_col_on.z, check_anim),
			ImLerp(base_col_off.w, base_col_on.w, check_anim)
		);
		bg_col.w *= final_alpha;
		draw_list->AddRectFilled(box_min, box_max, ImGui::GetColorU32(bg_col), box_rounding);

		// [第二层] ON 状态的核心方块 (解决层次感问题)
		if (check_anim > 0.001f) {
			// 内缩距离设置为整体尺寸的 10%，保证中心方块和外框之间有明显的间隙
			float inset = square_sz * 0.10f;
			ImVec2 inner_min = ImVec2(box_min.x + inset, box_min.y + inset);
			ImVec2 inner_max = ImVec2(box_max.x - inset, box_max.y - inset);

			ImVec4 core_col = accent_color;
			core_col.w *= check_anim * final_alpha;

			float inner_rounding = std::max(1.0f, box_rounding - inset * 0.5f);
			draw_list->AddRectFilled(inner_min, inner_max, ImGui::GetColorU32(core_col), inner_rounding);
		}

		// [第三层] 玻璃边框
		ImVec4 border_col_vec = ThemeColors::GLASS_BORDER;
		border_col_vec.x = ImLerp(border_col_vec.x, accent_color.x, check_anim * 0.5f);
		border_col_vec.y = ImLerp(border_col_vec.y, accent_color.y, check_anim * 0.5f);
		border_col_vec.z = ImLerp(border_col_vec.z, accent_color.z, check_anim * 0.5f);
		border_col_vec.w *= final_alpha;
		draw_list->AddRect(box_min, box_max, ImGui::GetColorU32(border_col_vec), box_rounding, 0, 1.0f);

		// 6. 渲染文本 (完美垂直对齐基线)
		if (label_size.x > 0.0f) {
			ImVec2 text_pos = ImVec2(pos.x + square_sz + g.Style.ItemInnerSpacing.x, pos.y + pad_y);
			ImGui::RenderText(text_pos, label);
		}

		return pressed;
	}

	inline bool DrawCustomSliderFloat(const char* label, float* v, float v_min, float v_max, const char* fmt = "%.1f", float step = 1.0f, const char* custom_text = nullptr)
	{
		if (v_max <= v_min) return false;

		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;

		ImGuiID id = window->GetID(label);
		const char* label_end = ImGui::FindRenderedTextEnd(label);
		ImVec2 label_size = ImGui::CalcTextSize(label, label_end);
		float full_w = ImGui::GetContentRegionAvail().x;
		const float TRACK_HEIGHT = 9.0f;
		const float HANDLE_RADIUS = 9.0f;
		const float VALUE_TEXT_PAD = 8.0f;

		ImVec2 pos = window->DC.CursorPos;
		ImVec2 size = ImVec2(full_w, std::max(style.FramePadding.y * 2.0f + TRACK_HEIGHT, label_size.y + style.FramePadding.y * 2.0f));
		ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
		ImGui::ItemSize(bb, 0.0f);
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

		if (g.NavId == id && ImGui::IsMouseClicked(0) && !hovered) {
			ImGui::ClearActiveID(); // 清理活跃状态
			g.NavId = 0;            // 清除当前控件焦点
		}

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

		// 键盘方向键微调功能逻辑
		// 条件：当前滑块拥有焦点（点过一次）且未按住鼠标左键（防止与拖拽冲突）
		if (g.NavId == id && !g.IO.MouseDown[0]) {
			float keyboard_delta = 0.0f;
			if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) keyboard_delta -= step;
			if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) keyboard_delta += step;

			if (keyboard_delta != 0.0f) {
				*v += keyboard_delta;
				if (*v < v_min) *v = v_min;
				if (*v > v_max) *v = v_max;
				value_changed = true;
			}
		}

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
		ImU32 col_handle = ImGui::GetColorU32(ImGuiCol_SliderGrab);
		ImU32 col_handle_active = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
		ImU32 col_text = ImGui::GetColorU32(ImGuiCol_Text);

		float track_rounding = TRACK_HEIGHT * 0.5f;

		draw->AddRectFilled(
			ImVec2(track_min.x - 1.0f, track_min.y - 1.0f),
			ImVec2(track_max.x + 1.0f, track_max.y + 1.0f),
			ImGui::GetColorU32(ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.04f)),
			track_rounding + 1.0f
		);
		draw->AddRectFilled(track_min, track_max, col_track_bg, track_rounding);

		ImVec2 filled_max = ImVec2(track_min.x + track_size.x * t, track_max.y);
		if (filled_max.x > track_min.x + 1.0f) {
			draw->AddRectFilled(track_min, filled_max, col_fill, track_rounding, ImDrawFlags_RoundCornersLeft);
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

		if (active || hovered || g.NavId == id) {
			float glow_intensity = active ? 0.4f : (g.NavId == id ? 0.3f : 0.2f);
			draw->AddCircleFilled(handle_center, handle_radius + 6.0f,
				ImGui::GetColorU32(ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, glow_intensity)), 24);
		}

		draw->AddCircleFilled(handle_center, handle_radius, (active || g.NavId == id) ? col_handle_active : col_handle, 24);
		draw->AddCircle(handle_center, handle_radius * 0.7f, IM_COL32(255, 255, 255, 30), 24, 2.5f);

		ImVec2 label_pos = ImVec2(pos.x, pos.y + (size.y - label_size.y) * 0.5f);
		draw->AddText(g.Font, g.FontSize, label_pos, col_text, label, label_end);

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
		draw->AddRectFilled(val_bb_min, val_bb_max, ImGui::GetColorU32(ThemeColors::BLACK_LOW), 7.0f);
		draw->AddRect(val_bb_min, val_bb_max, ImGui::GetColorU32(ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.23f)), 7.0f, 0, 1.0f);
		draw->AddText(value_pos, col_text, valbuf);

		return value_changed;
	}

	inline void DrawColorPickerRow(const char* checkbox_label, bool* checkbox_value, const char* color_picker_label, float* color_value)
	{
		DrawCustomCheckbox(checkbox_label, checkbox_value);

		float avail = ImGui::GetContentRegionAvail().x;
		float frame_h = ImGui::GetFrameHeight();
		float pad_y = ImGui::GetStyle().FramePadding.y;
		float btn_sz = frame_h - pad_y * 2.0f;
		if (btn_sz <= 0.0f) btn_sz = frame_h * 0.8f;

		ImGui::SameLine();
		float target_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - btn_sz;
		ImGui::SetCursorPosX(target_x);

		DrawCustomColorPicker(color_picker_label, color_value, *checkbox_value, nullptr);
	}

	inline bool CustomSelectable(const char* label, bool selected, float rounding = 6.0f) {
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems) return false;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		const ImGuiID id = window->GetID(label);
		const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

		const float horizontal_pad = 6.0f;
		const float vertical_pad = 2.0f;

		ImVec2 pos = window->DC.CursorPos;
		pos.y += window->DC.CurrLineTextBaseOffset;

		ImRect bb(pos, ImVec2(pos.x + label_size.x + horizontal_pad * 2.0f,
			pos.y + label_size.y + vertical_pad * 2.0f));

		ImGui::ItemSize(bb);
		if (!ImGui::ItemAdd(bb, id)) return false;

		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

		if (selected || hovered) {
			ImU32 col = ImGui::GetColorU32(selected ?
				ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.25f) :
				ImVec4(1.0f, 1.0f, 1.0f, 0.05f));

			window->DrawList->AddRectFilled(bb.Min, bb.Max, col, rounding);
		}

		ImGui::RenderText(ImVec2(bb.Min.x + horizontal_pad, bb.Min.y + vertical_pad), label);

		return pressed;
	}

	inline bool DrawCustomButton(const char* label, const ImVec2& size_arg = ImVec2(0, 0))
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		const ImGuiID id = window->GetID(label);
		const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

		ImVec2 pos = window->DC.CursorPos;
		ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

		const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
		ImGui::ItemSize(size, style.FramePadding.y);
		if (!ImGui::ItemAdd(bb, id))
			return false;

		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

		ImGuiStorage* storage = ImGui::GetStateStorage();
		float anim = storage->GetFloat(id, 0.0f);

		anim = ImLinearSweep(anim, (hovered || held) ? 1.0f : 0.0f, g.IO.DeltaTime * 6.0f);
		storage->SetFloat(id, anim);

		ImDrawList* draw_list = window->DrawList;

		ImU32 col_bg = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
		draw_list->AddRectFilled(bb.Min, bb.Max, col_bg, style.FrameRounding);

		if (anim > 0.001f)
		{
			ImVec4 accent_bg = ThemeColors::GetAccent();
			accent_bg.w *= (anim * 0.12f * g_MenuAlpha);
			draw_list->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(accent_bg), style.FrameRounding);

			ImVec4 border_col = ThemeColors::GetAccent();
			border_col.w *= (anim * 0.4f * g_MenuAlpha);
			draw_list->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(border_col), style.FrameRounding, 0, 1.0f);
		}
		else
		{
			draw_list->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(ThemeColors::GLASS_BORDER), style.FrameRounding, 0, 1.0f);
		}

		ImVec2 text_pos = ImVec2(
			bb.Min.x + (size.x - label_size.x) * 0.5f,
			bb.Min.y + (size.y - label_size.y) * 0.5f
		);

		if (held)
			text_pos.y += 1.0f;

		ImU32 text_col = ImGui::GetColorU32(((hovered || held) && anim > 0.5f) ? ThemeColors::GetAccent() : ThemeColors::TEXT);
		const char* label_end = ImGui::FindRenderedTextEnd(label);

		draw_list->AddText(NULL, 0.0f, text_pos, text_col, label, label_end);

		return pressed;
	}

	inline bool DrawCustomCombo(const char* label, int* current_item, const std::vector<const char*>& items, const char* custom_text = nullptr)
	{
		if (items.empty() || current_item == nullptr) return false;

		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems) return false;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		const ImGuiID id = window->GetID(label);

		// 检查 label 是否为隐藏 ID (以 ## 开头)
		bool has_label_text = (label[0] != '#' || (label[1] != '\0' && label[1] != '#'));
		const ImVec2 label_size = has_label_text ? ImGui::CalcTextSize(label, NULL, true) : ImVec2(0.0f, 0.0f);

		// 1. 【核心改进】动态计算下拉框主体的目标宽度
		float combo_box_w = 0.0f;

		// 如果外部通过 ImGui::SetNextItemWidth() 强行指定了宽度，则尊重外部设定
		if (window->DC.ItemWidthStack.Size > 0) {
			combo_box_w = ImGui::CalcItemWidth();
		}
		else {
			// 否则：根据 items 里面最长的文本自适应宽度，并加上 padding 和右侧小箭头的空间
			float max_text_w = 0.0f;
			for (const char* item : items) {
				float tw = ImGui::CalcTextSize(item).x;
				if (tw > max_text_w) max_text_w = tw;
			}
			// 如果有附加的 custom_text 后缀，也算上它的宽度
			if (custom_text && custom_text[0] != '\0') {
				max_text_w += ImGui::CalcTextSize(custom_text).x;
			}

			// 额外留白：左边距 + 右边距 + 箭头大小(10px) + 间隙
			combo_box_w = max_text_w + style.FramePadding.x * 2.0f + 18.0f;
		}

		// 2. 重新计算整体总宽度 (Label 区域 + 间距 + 下拉框主体)
		float label_area_width = label_size.x > 0.0f ? label_size.x + style.ItemInnerSpacing.x : 0.0f;
		float total_w = label_area_width + combo_box_w;

		float frame_h = ImGui::GetFrameHeight();
		float box_rounding = 6.0f;

		ImVec2 pos = window->DC.CursorPos;
		ImVec2 size = ImVec2(total_w, std::max(frame_h, label_size.y + style.FramePadding.y * 2.0f));
		ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

		ImGui::ItemSize(bb, style.FramePadding.y);
		if (!ImGui::ItemAdd(bb, id)) return false;

		// 交互逻辑
		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

		// 动画状态机
		ImGuiStorage* storage = ImGui::GetStateStorage();
		float hover_anim = storage->GetFloat(id, 0.0f);
		hover_anim = hovered ? std::min(hover_anim + g.IO.DeltaTime * 8.0f, 1.0f) : std::max(hover_anim - g.IO.DeltaTime * 8.0f, 0.0f);
		storage->SetFloat(id, hover_anim);

		char popup_id[256];
		std::snprintf(popup_id, sizeof(popup_id), "##ComboPopup_%s", label);

		if (pressed) {
			ImGui::OpenPopup(popup_id);
		}

		// ==========================================
		// 高质感分层渲染（空间已裁剪，不再顶满）
		// ==========================================
		ImDrawList* draw_list = window->DrawList;
		float final_alpha = g_MenuAlpha;
		ImVec4 accent_color = ThemeColors::GetAccent();

		// 精确确定右侧选择框的真实起始与结束坐标
		ImVec2 combo_min = ImVec2(pos.x + label_area_width, pos.y + (size.y - frame_h) * 0.5f);
		ImVec2 combo_max = ImVec2(combo_min.x + combo_box_w, combo_min.y + frame_h);

		// [背景层 Glow] 悬浮外发光
		if (hover_anim > 0.0f && final_alpha > 0.01f) {
			float glow_expand = 3.0f * hover_anim;
			ImVec2 glow_min = ImVec2(combo_min.x - glow_expand, combo_min.y - glow_expand);
			ImVec2 glow_max = ImVec2(combo_max.x + glow_expand, combo_max.y + glow_expand);
			ImU32 glow_col = ImGui::GetColorU32(ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.20f * hover_anim * final_alpha));
			draw_list->AddRectFilled(glow_min, glow_max, glow_col, box_rounding + glow_expand);
		}

		// [第一层] 底座背景
		ImVec4 bg_col = ThemeColors::WHITE_LOW;
		bg_col.w *= final_alpha;
		draw_list->AddRectFilled(combo_min, combo_max, ImGui::GetColorU32(bg_col), box_rounding);

		// [第二层] 渲染当前选中项文本与右侧小箭头
		int selected_idx = (*current_item >= 0 && *current_item < (int)items.size()) ? *current_item : 0;
		char preview_buf[256];
		std::snprintf(preview_buf, sizeof(preview_buf), "%s", items[selected_idx]);
		if (custom_text && custom_text[0] != '\0') {
			std::strncat(preview_buf, custom_text, sizeof(preview_buf) - std::strlen(preview_buf) - 1);
		}

		// 文本居左对齐（带Padding）
		ImVec2 text_pos = ImVec2(combo_min.x + style.FramePadding.x, combo_min.y + style.FramePadding.y);
		draw_list->AddText(text_pos, ImGui::GetColorU32(ThemeColors::TEXT), preview_buf);

		// 绘制极简三角箭头
		float arrow_sz = 10.0f;
		ImVec2 arrow_center = ImVec2(combo_max.x - style.FramePadding.x - arrow_sz * 0.5f, combo_min.y + frame_h * 0.5f);
		ImU32 arrow_col = ImGui::GetColorU32(hovered ? accent_color : ThemeColors::TEXT);

		ImVec2 p1 = ImVec2(arrow_center.x - arrow_sz * 0.5f, arrow_center.y - arrow_sz * 0.25f);
		ImVec2 p2 = ImVec2(arrow_center.x + arrow_sz * 0.5f, arrow_center.y - arrow_sz * 0.25f);
		ImVec2 p3 = ImVec2(arrow_center.x, arrow_center.y + arrow_sz * 0.35f);
		draw_list->AddTriangleFilled(p1, p2, p3, arrow_col);

		// [第三层] 玻璃边框
		ImVec4 border_col_vec = ThemeColors::GLASS_BORDER;
		if (hovered) {
			border_col_vec.x = ImLerp(border_col_vec.x, accent_color.x, 0.5f);
			border_col_vec.y = ImLerp(border_col_vec.y, accent_color.y, 0.5f);
			border_col_vec.z = ImLerp(border_col_vec.z, accent_color.z, 0.5f);
		}
		border_col_vec.w *= final_alpha;
		draw_list->AddRect(combo_min, combo_max, ImGui::GetColorU32(border_col_vec), box_rounding, 0, 1.0f);

		// 渲染左侧 Label 文本
		if (label_size.x > 0.0f) {
			ImVec2 label_pos = ImVec2(pos.x, pos.y + (size.y - label_size.y) * 0.5f);
			ImGui::RenderText(label_pos, label);
		}

		// ==========================================
		// 弹窗渲染：展开下拉菜单列表
		// ==========================================
		bool value_changed = false;

		// 弹出窗宽度精确限制在 combo_box_w，使其与选择框严丝合缝
		ImGui::SetNextWindowSizeConstraints(ImVec2(combo_box_w, 0.0f), ImVec2(combo_box_w, frame_h * 8.0f));

		if (ImGui::BeginPopup(popup_id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
		{
			float original_frame_rounding = g.Style.FrameRounding;
			g.Style.FrameRounding = 4.0f;

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, g.Style.ItemSpacing.y));

			for (int i = 0; i < (int)items.size(); ++i)
			{
				bool is_selected = (*current_item == i);

				ImGui::PushID(i);
				if (CustomSelectable(items[i], is_selected, 4.0f))
				{
					*current_item = i;
					value_changed = true;
					ImGui::CloseCurrentPopup();
				}
				ImGui::PopID();
			}

			ImGui::PopStyleVar();
			g.Style.FrameRounding = original_frame_rounding;
			ImGui::EndPopup();
		}

		return value_changed;
	}

	// 针对传统 C 风格数组的重载接口（保持你代码一设的多态扩展性）
	inline bool DrawCustomCombo(const char* label, int* current_item, const char* items[], int items_count, const char* custom_text = nullptr)
	{
		std::vector<const char*> vec_items(items, items + items_count);
		return DrawCustomCombo(label, current_item, vec_items, custom_text);
	}

	inline bool DrawCustomMultiCombo(const char* label, std::vector<CustomComboItem>& items, const char* placeholder = nullptr)
	{
		if (items.empty()) return false;

		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems) return false;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		const ImGuiID id = window->GetID(label);

		bool has_label_text = (label[0] != '#' || (label[1] != '\0' && label[1] != '#'));
		const ImVec2 label_size = has_label_text ? ImGui::CalcTextSize(label, NULL, true) : ImVec2(0.0f, 0.0f);

		// ==========================================
		// 1. 【动态生成预览文本】
		// ==========================================
		std::string preview_str = "";
		int selected_count = 0;
		for (size_t i = 0; i < items.size(); ++i) {
			if (items[i].checkbox_ptr != nullptr) items[i].selected = *(items[i].checkbox_ptr);
			if (items[i].selected) {
				selected_count++;
				if (selected_count <= 2) {
					if (!preview_str.empty()) preview_str += ", ";
					preview_str += items[i].label;
				}
			}
		}

		if (selected_count == 0) {
			preview_str = (placeholder && placeholder[0] != '\0') ? placeholder : U8("未选择...");
		}
		else if (selected_count > 2) {
			char count_buf[64];
			std::snprintf(count_buf, sizeof(count_buf), U8("%dx"), selected_count);
			preview_str = count_buf;
		}

		// ==========================================
		// 2. 【精准宽度计算】
		// ==========================================
		float combo_box_w = 0.0f;
		float frame_h = ImGui::GetFrameHeight();

		if (window->DC.ItemWidthStack.Size > 0) {
			combo_box_w = ImGui::CalcItemWidth();
		}
		else {
			// 计算弹窗内所有项中，内容最宽的一行
			float max_content_w = ImGui::CalcTextSize(U8("1x")).x;

			for (const auto& item : items) {
				if (!item.label) continue;
				float item_w = ImGui::CalcTextSize(item.label).x;

				// 无论该项是否有调色盘，我们按“统一对齐”的逻辑计算宽度
				// 宽度 = 调色盘(frame_h) + 间距(6.0f) + 文本宽度(item_w)
				float total_row_w = frame_h + 6.0f + item_w;

				if (total_row_w > max_content_w) max_content_w = total_row_w;
			}

			// 最终宽度 = 内容最宽处 + 左右内边距 + 箭头预留位(约等于frame_h)
			combo_box_w = max_content_w + (style.FramePadding.x * 2.0f);
		}

		float label_area_width = label_size.x > 0.0f ? label_size.x + style.ItemInnerSpacing.x : 0.0f;
		float total_w = label_area_width + combo_box_w;

		ImVec2 pos = window->DC.CursorPos;
		ImVec2 size = ImVec2(total_w, std::max(frame_h, label_size.y + style.FramePadding.y * 2.0f));
		ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

		ImGui::ItemSize(bb, style.FramePadding.y);
		if (!ImGui::ItemAdd(bb, id)) return false;

		// 交互逻辑
		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

		// 动画处理
		ImGuiStorage* storage = ImGui::GetStateStorage();
		float hover_anim = storage->GetFloat(id, 0.0f);
		hover_anim = hovered ? std::min(hover_anim + g.IO.DeltaTime * 8.0f, 1.0f) : std::max(hover_anim - g.IO.DeltaTime * 8.0f, 0.0f);
		storage->SetFloat(id, hover_anim);

		char popup_id[256];
		std::snprintf(popup_id, sizeof(popup_id), "##MultiComboPopup_%s", label);
		if (pressed) ImGui::OpenPopup(popup_id);

		// ==========================================
		// 3. 【渲染预览框】
		// ==========================================
		ImDrawList* draw_list = window->DrawList;
		float final_alpha = g_MenuAlpha;
		ImVec4 accent_color = ThemeColors::GetAccent();
		float box_rounding = 6.0f;

		ImVec2 combo_min = ImVec2(pos.x + label_area_width, pos.y + (size.y - frame_h) * 0.5f);
		ImVec2 combo_max = ImVec2(combo_min.x + combo_box_w, combo_min.y + frame_h);

		// 背景与发光
		if (hover_anim > 0.0f) {
			draw_list->AddRectFilled(combo_min, combo_max, ImGui::GetColorU32(ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.15f * hover_anim * final_alpha)), box_rounding);
		}
		draw_list->AddRectFilled(combo_min, combo_max, ImGui::GetColorU32(ThemeColors::WHITE_LOW), box_rounding);

		// 裁剪文本防止溢出预览框
		ImGui::PushClipRect(combo_min, ImVec2(combo_max.x - 25.0f, combo_max.y), true);
		draw_list->AddText(ImVec2(combo_min.x + style.FramePadding.x, combo_min.y + style.FramePadding.y), ImGui::GetColorU32(ThemeColors::TEXT), preview_str.c_str());
		ImGui::PopClipRect();

		// 箭头
		float arrow_sz = 10.0f;
		ImVec2 arrow_center = ImVec2(combo_max.x - style.FramePadding.x - arrow_sz * 0.5f, combo_min.y + frame_h * 0.5f);
		draw_list->AddTriangleFilled(
			ImVec2(arrow_center.x - 5, arrow_center.y - 2),
			ImVec2(arrow_center.x + 5, arrow_center.y - 2),
			ImVec2(arrow_center.x, arrow_center.y + 4),
			ImGui::GetColorU32(hovered ? accent_color : ThemeColors::TEXT)
		);

		if (label_size.x > 0.0f) {
			ImGui::RenderText(ImVec2(pos.x, pos.y + (size.y - label_size.y) * 0.5f), label);
		}

		// ==========================================
		// 4. 【多选弹窗逻辑】
		// ==========================================
		bool value_changed = false;

		// 设置弹窗宽度刚好等于预览框宽度，不再盲目扩充
		ImGui::SetNextWindowSizeConstraints(ImVec2(combo_box_w, 0.0f), ImVec2(combo_box_w, frame_h * 15.0f));

		if (ImGui::BeginPopup(popup_id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, style.ItemSpacing.y));
			float text_align_pos_x = style.FramePadding.x + frame_h + 6.0f;

			for (int i = 0; i < (int)items.size(); ++i)
			{
				ImGui::PushID(i);
				ImGui::BeginGroup();

				if (items[i].color_ptr != nullptr) {
					DrawCustomColorPicker("##cp", items[i].color_ptr, true, nullptr);
					ImGui::SameLine();
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0f);
					text_align_pos_x = ImGui::GetCursorPosX(); // 捕捉真实的对齐线
				}
				else {
					ImGui::SetCursorPosX(text_align_pos_x);
				}

				// 计算文本可用的剩余宽度
				float avail_w = ImGui::GetContentRegionAvail().x - style.FramePadding.x;
				if (CustomSelectable(items[i].label, items[i].selected, 4.0f))
				{
					items[i].selected = !items[i].selected;
					if (items[i].checkbox_ptr != nullptr) *(items[i].checkbox_ptr) = items[i].selected;
					value_changed = true;
				}

				ImGui::EndGroup();
				ImGui::PopID();
			}
			ImGui::PopStyleVar();
			ImGui::EndPopup();
		}

		return value_changed;
	}

	inline void DrawKeyBindButton(const char* label, UINT& bindKey) {
		// ImGui::Text("%s:", label);
		// ImGui::SameLine();

		char btnLabel[64];
		if (g_MDX12::g_MenuState::g_pCurrentBindingKey == &bindKey) {
			strcpy_s(btnLabel, "Press any key...");
		}
		else {
			sprintf_s(btnLabel, "%s##%s", g_Util::GetKeyName(bindKey), label);
		}

		if (DrawCustomButton(btnLabel)) {
			g_MDX12::g_MenuState::g_pCurrentBindingKey = &bindKey;
		}
	}

	inline void BeginTabRegion(const char* id)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ThemeColors::BG.x, ThemeColors::BG.y, ThemeColors::BG.z, 0.88f));
		ImGui::BeginChild(id, ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	}

	inline void EndTabRegion()
	{
		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}

	inline void DrawAnimatedSeparator()
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
			ImGui::GetColorU32(ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.0f)),
			ImGui::GetColorU32(ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.31f)),
			ImGui::GetColorU32(ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.31f)),
			ImGui::GetColorU32(ImVec4(ThemeColors::GetAccent().x, ThemeColors::GetAccent().y, ThemeColors::GetAccent().z, 0.0f))
		);

		ImGui::Dummy(ImVec2(0, 2.0f));
	}
}