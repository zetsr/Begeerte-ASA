#pragma once
#include "Shadow.h"
#include "../misc/textures/Shadow_Titlebar.h"

namespace Shadow {
    // HelpMarker
    inline void HelpMarker(std::string_view desc, float TextWrap = 200.f) {
        Shadow::SameLine();
        Shadow::TextColored(Shadow::g_Ctx.Style.Colors[Shadow::GuiCol_TextDisabled], "(?)");
        if (Shadow::IsItemHovered(Shadow::ShadowHoveredFlags_DelayNone)) {
            Shadow::BeginTooltip();
            Shadow::PushTextWrapPos(TextWrap);
            Shadow::TextWrapped({ 1.0f, 1.0f, 1.0f, 1.0f }, desc.data());
            Shadow::PopTextWrapPos();
            Shadow::EndTooltip();
        }
    }

    // ShowDemoWindow
    inline void ShowDemoWindow() {
        static bool show_demo = true;
        if (!show_demo) return;

        static Shadow::ShadowWindowFlags win_flags = Shadow::ShadowWindowFlags_MenuBar;
        static Shadow::ShadowTabBarFlags tab_flags = Shadow::ShadowTabBarFlags_Reorderable | Shadow::ShadowTabBarFlags_FittingPolicyScroll;

        if (Shadow::Begin("Shadow Gui Demo##DemoWindow", win_flags)) {
            if (Shadow::BeginMenuBar()) {
                if (Shadow::BeginMenu("File")) {

                    if (Shadow::MenuItem("New", "Ctrl+N")) {}
                    if (Shadow::MenuItem("Open", "Ctrl+O")) {}

                    if (Shadow::BeginMenu("Open Recent 1")) {
                        Shadow::MenuItem("File 1.txt");
                        Shadow::MenuItem("File 2.txt");

                        if (Shadow::BeginMenu("Open Recent 2")) {
                            Shadow::MenuItem("File 3.txt");
                            Shadow::MenuItem("File 4.txt");

                            if (Shadow::BeginMenu("Open Recent 3")) {
                                Shadow::MenuItem("File 5.txt");
                                Shadow::MenuItem("File 6.txt");
                            }
                            Shadow::EndMenu();
                        }
                        Shadow::EndMenu();
                    }
                    Shadow::EndMenu();

                    Shadow::Separator();
                    static bool save_as_checked = false;
                    Shadow::MenuItem("Save As...", "", &save_as_checked);
                    Shadow::MenuItem("Exit", "Alt+F4");
                }
                Shadow::EndMenu();

                if (Shadow::BeginMenu("Edit")) {
                    Shadow::MenuItem("Undo", "Ctrl+Z");
                    Shadow::MenuItem("Redo", "Ctrl+Y", nullptr, false); // Disabled
                    Shadow::Separator();
                    Shadow::MenuItem("Cut", "Ctrl+X");
                    Shadow::MenuItem("Copy", "Ctrl+C");
                    Shadow::MenuItem("Paste", "Ctrl+V");
                }
                Shadow::EndMenu();

                if (Shadow::BeginMenu("View")) {
                    Shadow::MenuItem("Show Toolbar");
                    Shadow::MenuItem("Show Sidebar");

                    if (Shadow::BeginMenu("Theme")) {
                        Shadow::MenuItem("Dark");
                        Shadow::MenuItem("Light");
                    }
                    Shadow::EndMenu();
                }
                Shadow::EndMenu();
                Shadow::MenuItem("Direct Item");
            }
            Shadow::EndMenuBar();

            if (Shadow::BeginTabBar("DemoTabBar", tab_flags)) {

                // Page 1: All standard controls and full flag tests (with combined flag demos)
                if (Shadow::BeginTabItem("Standard Controls")) {

                    if (Shadow::TreeNode("Text Elements")) {
                        Shadow::Text("Standard Text");
                        Shadow::HelpMarker("Basic text output using Shadow::Text.");

                        Shadow::PushTextOutline();
                        Shadow::Text("Outlined Text");
                        Shadow::PopTextOutline();
                        Shadow::HelpMarker("using Shadow::PushTextOutline.");

                        Shadow::TextDisabled("Disabled Text");
                        Shadow::HelpMarker("Text with disabled color using Shadow::TextDisabled.");

                        Shadow::TextColored({ 1.f, 0.5f, 0.5f, 1.f }, "Colored Text");
                        Shadow::HelpMarker("Text with custom RGB color using Shadow::TextColored.");

                        Shadow::TextWrapped({ 0.5f, 1.f, 0.5f, 1.f }, "This is a long wrapped text that will automatically break into multiple lines depending on the window width...");
                        Shadow::HelpMarker("Wrapped text adjusts to window/clip width automatically using Shadow::TextWrapped.");
                    }
                    Shadow::TreePop(); // Unconditional Pop

                    if (Shadow::TreeNode("Basic Widgets")) {
                        static bool bVal1 = false, bVal2 = true;
                        Shadow::Checkbox("Checkbox 1", &bVal1);
                        Shadow::HelpMarker("Standard Checkbox widget.");

                        Shadow::Switch("Switch 1", &bVal2);
                        Shadow::HelpMarker("Standard Switch toggle widget.");

                        if (Shadow::Button("Click Me")) {}
                        Shadow::HelpMarker("Standard Button widget.");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Images & Image Buttons")) {
                        SDK::UTexture2D* demoTex = Shadow::LoadTextureFromBuffer(Shadow_Titlebar::Logo, sizeof(Shadow_Titlebar::Logo));

                        // 1. Shadow::Image 基础与不同参数用法演示
                        Shadow::Text("Shadow::Image");

                        // 基础默认颜色与尺寸
                        Shadow::Image(demoTex, { 48.f, 48.f });
                        Shadow::HelpMarker("Shadow::Image rendered with default full-white tint color.");

                        // 自定义着色 / 滤镜
                        Shadow::SameLine();
                        Shadow::Image(demoTex, { 48.f, 48.f }, { 1.f, 0.4f, 0.4f, 1.f });
                        Shadow::HelpMarker("Shadow::Image rendered with custom red color tint.");

                        Shadow::SameLine();
                        Shadow::Image(demoTex, { 48.f, 48.f }, { 0.4f, 0.8f, 1.f, 0.6f });
                        Shadow::HelpMarker("Shadow::Image rendered with semi-transparent cyan tint.");

                        // 禁用状态下的 Image
                        Shadow::BeginDisabled(true);
                        Shadow::SameLine();
                        Shadow::Image(demoTex, { 48.f, 48.f });
                        Shadow::EndDisabled();
                        Shadow::HelpMarker("Shadow::Image rendered inside BeginDisabled() block (auto opacity scaled).");

                        Shadow::Separator();

                        // 2. Shadow::ImageButton 各种交互与排布演示
                        Shadow::Text("Shadow::ImageButton Demos");

                        static int img_btn_click_count = 0;
                        if (Shadow::ImageButton("BasicImageBtn##1", demoTex, { 32.f, 32.f })) {
                            img_btn_click_count++;
                        }
                        Shadow::HelpMarker("Standard ImageButton. Click to increment counter.");
                        Shadow::SameLine();
                        Shadow::Text(std::format("Clicked: {} times", img_btn_click_count));

                        // 横向排布与不同尺寸的 ImageButton
                        Shadow::Text("Horizontal Toolbar of ImageButtons:");
                        for (int i = 0; i < 3; ++i) {
                            if (Shadow::ImageButton(std::format("ToolbarBtn_{}##Tool", i), demoTex, { 28.f, 28.f })) {
                                // Toolbar button click action
                            }
                            if (i < 2) Shadow::SameLine();
                        }
                        Shadow::HelpMarker("Multiple ImageButtons packed horizontally using SameLine().");

                        // 禁用状态下的 ImageButton
                        Shadow::BeginDisabled(true);
                        Shadow::ImageButton("DisabledImageBtn##Dis", demoTex, { 32.f, 32.f });
                        Shadow::EndDisabled();
                        Shadow::HelpMarker("Shadow::ImageButton rendered inside BeginDisabled() block.");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Selectables & Combos")) {
                        static bool sel1 = false, sel2 = true;
                        Shadow::Selectable("Selectable 1", &sel1);
                        Shadow::HelpMarker("Selectable taking a boolean pointer (toggle mode).");

                        Shadow::Selectable("Selectable 2 (No Pointer)", sel2);
                        Shadow::HelpMarker("Selectable taking a simple boolean (trigger mode).");

                        static int combo_standard_idx = 0;
                        static int combo_notext_idx = 0;
                        static int combo_noright_idx = 0;
                        static int combo_fittext_idx = 0;
                        static int combo_notext_noright_idx = 0;
                        static int combo_fittext_noright_idx = 0;
                        static int combo_notext_fittext_idx = 0;
                        static int combo_notext_noright_fittext_idx = 0;
                        std::vector<std::string> combo_items = { "Item A", "Item B", "Item C" };

                        Shadow::Combo("Standard Combo", &combo_standard_idx, combo_items);
                        Shadow::HelpMarker("Standard combo box (ShadowComboFlags_None).");

                        Shadow::Combo("Combo NoText", &combo_notext_idx, combo_items, Shadow::ShadowComboFlags_NoText);
                        Shadow::HelpMarker("Combo without left label (ShadowComboFlags_NoText).");

                        Shadow::Combo("Combo NoRightAlign", &combo_noright_idx, combo_items, Shadow::ShadowComboFlags_NoRightAlign);
                        Shadow::HelpMarker("Combo tightly packed next to text (ShadowComboFlags_NoRightAlign).");

                        Shadow::Combo("Combo FitText", &combo_fittext_idx, combo_items, Shadow::ShadowComboFlags_FitText);
                        Shadow::HelpMarker("Combo width adapts to selected text length (ShadowComboFlags_FitText).");

                        // Combo Flags combined demos
                        Shadow::Combo("Combo NoText+NoRightAlign", &combo_notext_noright_idx, combo_items, Shadow::ShadowComboFlags_NoText | Shadow::ShadowComboFlags_NoRightAlign);
                        Shadow::HelpMarker("Combines NoText and NoRightAlign.");

                        Shadow::Combo("Combo FitText+NoRightAlign", &combo_fittext_noright_idx, combo_items, Shadow::ShadowComboFlags_FitText | Shadow::ShadowComboFlags_NoRightAlign);
                        Shadow::HelpMarker("Combines FitText and NoRightAlign.");

                        Shadow::Combo("Combo NoText+FitText", &combo_notext_fittext_idx, combo_items, Shadow::ShadowComboFlags_NoText | Shadow::ShadowComboFlags_FitText);
                        Shadow::HelpMarker("Combines NoText and FitText.");

                        Shadow::Combo("Combo NoText+NoRightAlign+FitText", &combo_notext_noright_fittext_idx, combo_items, Shadow::ShadowComboFlags_NoText | Shadow::ShadowComboFlags_NoRightAlign | Shadow::ShadowComboFlags_FitText);
                        Shadow::HelpMarker("Combines NoText, NoRightAlign, and FitText.");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Sliders")) {
                        static float s1 = 0.5f, s2 = 1.0f, s3 = 50.f, s4 = 75.f;
                        Shadow::Slider("Standard Slider", &s1, 0.f, 1.f);
                        Shadow::HelpMarker("Basic float slider (ShadowSliderFlags_None).");

                        Shadow::Slider("Slider NoText", &s2, 0.f, 10.f, 0.f, Shadow::ShadowSliderFlags_NoText);
                        Shadow::HelpMarker("Slider without label (ShadowSliderFlags_NoText).");

                        Shadow::Slider("Slider NoRightAlign", &s3, 0.f, 100.f, 1.f, Shadow::ShadowSliderFlags_NoRightAlign);
                        Shadow::HelpMarker("Slider that doesn't stretch to right margin (ShadowSliderFlags_NoRightAlign).");

                        // Slider Flags combined demos
                        Shadow::Slider("Slider NoText+NoRightAlign", &s4, 0.f, 100.f, 0.f, Shadow::ShadowSliderFlags_NoText | Shadow::ShadowSliderFlags_NoRightAlign);
                        Shadow::HelpMarker("Combines NoText and NoRightAlign.");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Inputs (Text & Float)")) {
                        static std::string txt_standard = "";
                        static std::string txt_hint = "";
                        static std::string txt_decimal = "123";
                        static std::string txt_hexadecimal = "123";
                        static std::string txt_scientific = "123";
                        static std::string txt_uppercase = "123";
                        static std::string txt_noblank = "123";
                        static std::string txt_password = "secret";
                        static std::string txt_readonly = "secret";
                        static std::string txt_escape = "123";
                        static std::string txt_autoselect = "123";

                        Shadow::InputText("Standard Input", txt_standard);
                        Shadow::HelpMarker("Standard text input (ShadowInputTextFlags_None).");

                        Shadow::InputTextWithHint("Hint Input", "Enter your name...", txt_hint);
                        Shadow::HelpMarker("Input with background hint text.");

                        Shadow::InputText("Decimal Only", txt_decimal, Shadow::ShadowInputTextFlags_CharsDecimal);
                        Shadow::HelpMarker("Only numbers allowed (ShadowInputTextFlags_CharsDecimal).");

                        Shadow::InputText("Hexadecimal Only", txt_hexadecimal, Shadow::ShadowInputTextFlags_CharsHexadecimal);
                        Shadow::HelpMarker("Only hex allowed (ShadowInputTextFlags_CharsHexadecimal).");

                        Shadow::InputText("Scientific Only", txt_scientific, Shadow::ShadowInputTextFlags_CharsScientific);
                        Shadow::HelpMarker("Scientific numbers (ShadowInputTextFlags_CharsScientific).");

                        Shadow::InputText("Uppercase Only", txt_uppercase, Shadow::ShadowInputTextFlags_CharsUppercase);
                        Shadow::HelpMarker("Auto converts to uppercase (ShadowInputTextFlags_CharsUppercase).");

                        Shadow::InputText("No Blanks", txt_noblank, Shadow::ShadowInputTextFlags_CharsNoBlank);
                        Shadow::HelpMarker("Spaces restricted (ShadowInputTextFlags_CharsNoBlank).");

                        Shadow::InputText("Password", txt_password, Shadow::ShadowInputTextFlags_Password);
                        Shadow::HelpMarker("Text obfuscated with * (ShadowInputTextFlags_Password).");

                        Shadow::InputText("Read Only", txt_readonly, Shadow::ShadowInputTextFlags_ReadOnly);
                        Shadow::HelpMarker("Cannot be modified (ShadowInputTextFlags_ReadOnly).");

                        Shadow::InputText("Escape Clears", txt_escape, Shadow::ShadowInputTextFlags_EscapeClearsAll);
                        Shadow::HelpMarker("Press Esc to clear text (ShadowInputTextFlags_EscapeClearsAll).");

                        Shadow::InputText("Auto Select All", txt_autoselect, Shadow::ShadowInputTextFlags_AutoSelectAll);
                        Shadow::HelpMarker("Highlights all text on click (ShadowInputTextFlags_AutoSelectAll).");

                        static std::string txt_noname = "NoName";
                        Shadow::InputText("##NoNameInput", txt_noname, Shadow::ShadowInputTextFlags_NoName);
                        Shadow::HelpMarker("InputText with NoName flag, no label drawn (ShadowInputTextFlags_NoName).");

                        static std::string txt_align = "Center";
                        Shadow::InputText("Align Center", txt_align, Shadow::ShadowInputTextFlags_AlignCenter);
                        Shadow::HelpMarker("InputText with AlignCenter flag (ShadowInputTextFlags_AlignCenter).");

                        // InputText Flags combined demos
                        static std::string txt_combo1 = "my_secret";
                        Shadow::InputText("Pwd+AutoSel+EscClr", txt_combo1, Shadow::ShadowInputTextFlags_Password | Shadow::ShadowInputTextFlags_AutoSelectAll | Shadow::ShadowInputTextFlags_EscapeClearsAll);
                        Shadow::HelpMarker("Combines Password, AutoSelectAll, and EscapeClearsAll.");

                        static std::string txt_combo2 = "12345";
                        Shadow::InputText("Dec+NoBlank+AutoSel", txt_combo2, Shadow::ShadowInputTextFlags_CharsDecimal | Shadow::ShadowInputTextFlags_CharsNoBlank | Shadow::ShadowInputTextFlags_AutoSelectAll);
                        Shadow::HelpMarker("Combines CharsDecimal, CharsNoBlank, and AutoSelectAll.");

                        static std::string txt_combo3 = "readonly";
                        Shadow::InputText("ReadOnly+AutoSel", txt_combo3, Shadow::ShadowInputTextFlags_ReadOnly | Shadow::ShadowInputTextFlags_AutoSelectAll);
                        Shadow::HelpMarker("Combines ReadOnly and AutoSelectAll.");

                        static std::string txt_combo4 = "UPPER_NO_SPACE";
                        Shadow::InputText("Upper+NoBlank", txt_combo4, Shadow::ShadowInputTextFlags_CharsUppercase | Shadow::ShadowInputTextFlags_CharsNoBlank);
                        Shadow::HelpMarker("Combines CharsUppercase and CharsNoBlank.");

                        static float f_input = 3.14f;
                        Shadow::InputFloat("Float Input", &f_input);
                        Shadow::HelpMarker("Standard float input.");

                        static float f_emptyref = 3.14f;
                        Shadow::InputFloat("Float EmptyRef", &f_emptyref, 0.1f, 1.0f, "{:.2f}", Shadow::ShadowInputTextFlags_ParseEmptyRefVal | Shadow::ShadowInputTextFlags_DisplayEmptyRefVal);
                        Shadow::HelpMarker("Handles empty value parsing and display.");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Color Pickers")) {
                        static float cp_standard_r = 1.f, cp_standard_g = 0.f, cp_standard_b = 0.f, cp_standard_a = 1.f;
                        static float cp_notext_r = 1.f, cp_notext_g = 0.f, cp_notext_b = 0.f, cp_notext_a = 1.f;
                        static float cp_noright_r = 1.f, cp_noright_g = 0.f, cp_noright_b = 0.f, cp_noright_a = 1.f;
                        static float cp_combo_r = 1.f, cp_combo_g = 0.f, cp_combo_b = 0.f, cp_combo_a = 1.f;

                        Shadow::ColorPicker("Standard Picker", &cp_standard_r, &cp_standard_g, &cp_standard_b, &cp_standard_a);
                        Shadow::HelpMarker("Standard RGBA Picker (ShadowColorPickerFlags_None).");

                        Shadow::ColorPicker("Picker NoText", &cp_notext_r, &cp_notext_g, &cp_notext_b, &cp_notext_a, Shadow::ShadowColorPickerFlags_NoText);
                        Shadow::HelpMarker("Picker without left label (ShadowColorPickerFlags_NoText).");

                        Shadow::ColorPicker("Picker NoRightAlign", &cp_noright_r, &cp_noright_g, &cp_noright_b, &cp_noright_a, Shadow::ShadowColorPickerFlags_NoRightAlign);
                        Shadow::HelpMarker("Picker packed tightly (ShadowColorPickerFlags_NoRightAlign).");

                        // ColorPicker Flags combined demos
                        Shadow::ColorPicker("Picker NoText+NoRightAlign", &cp_combo_r, &cp_combo_g, &cp_combo_b, &cp_combo_a, Shadow::ShadowColorPickerFlags_NoText | Shadow::ShadowColorPickerFlags_NoRightAlign);
                        Shadow::HelpMarker("Combines NoText and NoRightAlign.");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Hotkeys")) {
                        static int hk_simple = 0;

                        static int hk_advanced = 0x41; // 'A'
                        static bool hk_advanced_active = false;
                        static Shadow::HotkeyMode hk_advanced_mode = Shadow::HotkeyMode::ToggleOn;

                        static int hk_notext = 0x41; // 'A'
                        static bool hk_notext_active = false;
                        static Shadow::HotkeyMode hk_notext_mode = Shadow::HotkeyMode::ToggleOn;

                        static int hk_noright = 0x41; // 'A'
                        static bool hk_noright_active = false;
                        static Shadow::HotkeyMode hk_noright_mode = Shadow::HotkeyMode::ToggleOn;

                        static int hk_nostate = 0x41; // 'A'
                        static bool hk_nostate_active = false;
                        static Shadow::HotkeyMode hk_nostate_mode = Shadow::HotkeyMode::ToggleOn;

                        static int hk_notext_noright = 0x41; // 'A'
                        static bool hk_notext_noright_active = false;
                        static Shadow::HotkeyMode hk_notext_noright_mode = Shadow::HotkeyMode::ToggleOn;

                        static int hk_notext_nostate = 0x41; // 'A'
                        static bool hk_notext_nostate_active = false;
                        static Shadow::HotkeyMode hk_notext_nostate_mode = Shadow::HotkeyMode::ToggleOn;

                        static int hk_noright_nostate = 0x41; // 'A'
                        static bool hk_noright_nostate_active = false;
                        static Shadow::HotkeyMode hk_noright_nostate_mode = Shadow::HotkeyMode::ToggleOn;

                        static int hk_all = 0x41; // 'A'
                        static bool hk_all_active = false;
                        static Shadow::HotkeyMode hk_all_mode = Shadow::HotkeyMode::ToggleOn;

                        Shadow::HotKey("Simple Hotkey", &hk_simple);
                        Shadow::HelpMarker("Registers simple hotkey taking int* only.");

                        Shadow::HotKey("Advanced Hotkey", &hk_advanced, &hk_advanced_active, &hk_advanced_mode);
                        Shadow::HelpMarker("Hotkey with mode selection. Right-click to configure mode.");

                        Shadow::HotKey("Hotkey NoText", &hk_notext, &hk_notext_active, &hk_notext_mode, Shadow::ShadowHotkeyFlags_NoText);
                        Shadow::HelpMarker("Hotkey without label (ShadowHotkeyFlags_NoText).");

                        Shadow::HotKey("Hotkey NoRightAlign", &hk_noright, &hk_noright_active, &hk_noright_mode, Shadow::ShadowHotkeyFlags_NoRightAlign);
                        Shadow::HelpMarker("Hotkey tightly packed (ShadowHotkeyFlags_NoRightAlign).");

                        Shadow::HotKey("Hotkey NoStateDisplay", &hk_nostate, &hk_nostate_active, &hk_nostate_mode, Shadow::ShadowHotkeyFlags_NoStateDisplay);
                        Shadow::HelpMarker("Hotkey without status indicator dot (ShadowHotkeyFlags_NoStateDisplay).");

                        // Hotkeys Flags combined demos
                        Shadow::HotKey("HK NoText+NoRightAlign", &hk_notext_noright, &hk_notext_noright_active, &hk_notext_noright_mode, Shadow::ShadowHotkeyFlags_NoText | Shadow::ShadowHotkeyFlags_NoRightAlign);
                        Shadow::HelpMarker("Combines NoText and NoRightAlign.");

                        Shadow::HotKey("HK NoText+NoStateDisp", &hk_notext_nostate, &hk_notext_nostate_active, &hk_notext_nostate_mode, Shadow::ShadowHotkeyFlags_NoText | Shadow::ShadowHotkeyFlags_NoStateDisplay);
                        Shadow::HelpMarker("Combines NoText and NoStateDisplay.");

                        Shadow::HotKey("HK NoRightAlign+NoStateDisp", &hk_noright_nostate, &hk_noright_nostate_active, &hk_noright_nostate_mode, Shadow::ShadowHotkeyFlags_NoRightAlign | Shadow::ShadowHotkeyFlags_NoStateDisplay);
                        Shadow::HelpMarker("Combines NoRightAlign and NoStateDisplay.");

                        Shadow::HotKey("HK All Flags Combo", &hk_all, &hk_all_active, &hk_all_mode, Shadow::ShadowHotkeyFlags_NoText | Shadow::ShadowHotkeyFlags_NoRightAlign | Shadow::ShadowHotkeyFlags_NoStateDisplay);
                        Shadow::HelpMarker("Combines NoText, NoRightAlign, and NoStateDisplay.");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Cursor & Scroll APIs")) {
                        // --- 1. Cursor Position Queries ---
                        if (Shadow::TreeNode("Cursor Queries")) {
                            Vec2 cur_pos = Shadow::GetCursorPos();
                            float cur_x = Shadow::GetCursorPosX();
                            float cur_y = Shadow::GetCursorPosY();
                            Vec2 screen_pos = Shadow::GetCursorScreenPos();
                            Vec2 start_pos = Shadow::GetCursorStartPos();

                            Shadow::Text(std::format("GetCursorPos(): ({:.1f}, {:.1f})", cur_pos.x, cur_pos.y));
                            Shadow::Text(std::format("GetCursorPosX(): {:.1f} | GetCursorPosY(): {:.1f}", cur_x, cur_y));
                            Shadow::Text(std::format("GetCursorScreenPos(): ({:.1f}, {:.1f})", screen_pos.x, screen_pos.y));
                            Shadow::Text(std::format("GetCursorStartPos(): ({:.1f}, {:.1f})", start_pos.x, start_pos.y));

                            Shadow::HelpMarker("GetCursorPos/X/Y get coordinates relative to content origin.\nGetCursorScreenPos gets absolute screen coordinates.\nGetCursorStartPos gets content top-left screen position.");
                        }
                        Shadow::TreePop();

                        // --- 2. Cursor Position Modifiers ---
                        if (Shadow::TreeNode("Cursor Modifiers")) {
                            Shadow::Text("1. SetCursorPosX demo (Aligning item horizontally):");
                            Shadow::SetCursorPosX(120.f);
                            Shadow::Button("Button at Relative X = 120##PosXBtn");

                            Shadow::Text("2. SetCursorPosY demo (Adding custom vertical gap):");
                            float current_y = Shadow::GetCursorPosY();
                            Shadow::SetCursorPosY(current_y + 15.f);
                            Shadow::Button("Button with +15px Y Offset##PosYBtn");

                            Shadow::Text("3. SetCursorPos demo (Placing custom widget at relative X/Y):");
                            Vec2 base_pos = Shadow::GetCursorPos();
                            Shadow::SetCursorPos({ base_pos.x + 40.f, base_pos.y + 5.f });
                            Shadow::Button("Custom Pos Button (X+40, Y+5)##PosBtn");

                            Shadow::Text("4. SetCursorScreenPos demo (Screen space positioning):");
                            Vec2 screen_p = Shadow::GetCursorScreenPos();
                            Shadow::SetCursorScreenPos({ screen_p.x + 20.f, screen_p.y + 5.f });
                            Shadow::Button("Screen Shifted Button##ScreenPosBtn");

                            Shadow::HelpMarker("SetCursorPos/X/Y manually reposition the drawing cursor relative to content.\nSetCursorScreenPos directly sets absolute screen cursor coordinates.");
                        }
                        Shadow::TreePop();

                        // --- 3. Scroll Queries ---
                        if (Shadow::TreeNode("Scroll Queries")) {
                            float scroll_x = Shadow::GetScrollX();
                            float scroll_y = Shadow::GetScrollY();
                            float scroll_max_x = Shadow::GetScrollMaxX();
                            float scroll_max_y = Shadow::GetScrollMaxY();

                            Shadow::Text(std::format("Scroll Y: {:.1f} / {:.1f} (Max)", scroll_y, scroll_max_y));
                            Shadow::Text(std::format("Scroll X: {:.1f} / {:.1f} (Max)", scroll_x, scroll_max_x));

                            Shadow::HelpMarker("GetScrollX/Y return current horizontal and vertical pixel scroll offsets.\nGetScrollMaxX/Y return maximum allowed scroll range.");
                        }
                        Shadow::TreePop();
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Text Measurement & Rect Visibility")) {
                        Shadow::Vec2 text_size = Shadow::MeasureTextSize("Measure Me");
                        float char_w = Shadow::MeasureCharWidth(L'A');
                        float text_h = Shadow::MeasureTextHeight("Measure Height");
                        Shadow::Text(std::format("MeasureTextSize: {:.1f} x {:.1f}", text_size.x, text_size.y));
                        Shadow::Text(std::format("MeasureCharWidth('A'): {:.1f}", char_w));
                        Shadow::Text(std::format("MeasureTextHeight: {:.1f}", text_h));

                        Shadow::Vec2 test_pos = Shadow::g_Ctx.Cursor;
                        Shadow::Vec2 test_size = { 100.f, 50.f };
                        bool visible = Shadow::IsRectVisible(test_pos, test_size);
                        bool fully_visible = Shadow::IsRectFullyVisible(test_pos, test_size);
                        Shadow::Text(std::format("IsRectVisible: {} | IsRectFullyVisible: {}", visible ? "TRUE" : "FALSE", fully_visible ? "TRUE" : "FALSE"));

                        Shadow::Vec2 clipped_pos = test_pos;
                        Shadow::Vec2 clipped_size = test_size;
                        Shadow::ClipRect(clipped_pos, clipped_size);
                        Shadow::Text(std::format("ClipRect result: pos({:.1f},{:.1f}) size({:.1f},{:.1f})", clipped_pos.x, clipped_pos.y, clipped_size.x, clipped_size.y));
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Layout, Spacing & Stack Modifiers")) {
                        Shadow::Text("Item 1");
                        Shadow::SameLine();
                        Shadow::Text("Item 2 (Same Line)");
                        Shadow::HelpMarker("Uses SameLine() without args to pack horizontally.");

                        Shadow::Text("Item 3");
                        Shadow::SameLine(150.f);
                        Shadow::Text("Item 4 (SameLine offset)");
                        Shadow::HelpMarker("Uses SameLine(150.f) to force alignment to 150px.");

                        Shadow::Text("Item 5");
                        Shadow::SameLine(0.f, 40.f);
                        Shadow::Text("Item 6 (SameLine space)");
                        Shadow::HelpMarker("Uses SameLine(0.f, 40.f) to force 40px spacing.");

                        Shadow::Separator();
                        Shadow::HelpMarker("Separator() line above.");

                        // NewLine 完整功能演示
                        Shadow::Text("NewLine()");

                        // 1. 物理空行（模拟按 Enter 键留出完整单行文本高度）
                        Shadow::Text("Line 1: Normal paragraph line.");
                        Shadow::NewLine();
                        Shadow::Text("Line 2: Appears after a full empty line created by NewLine().");
                        Shadow::HelpMarker("NewLine() allocates a full font line height, simulating pressing Enter.");

                        // 2. 对比 Spacing() 与 NewLine() 的留白高度差异
                        Shadow::Text("Compare: Text before Spacing()");
                        Shadow::Spacing();
                        Shadow::Text("Compare: Text after Spacing() (Small gap)");
                        Shadow::NewLine();
                        Shadow::Text("Compare: Text after NewLine() (Full text row gap)");
                        Shadow::HelpMarker("Spacing() only adds ItemSpacing.y gap, whereas NewLine() adds full line height.");

                        // 3. 显式打断 SameLine() 状态
                        Shadow::Button("Btn A##SameBreak");
                        Shadow::SameLine();
                        Shadow::Button("Btn B##SameBreak");
                        Shadow::SameLine();
                        Shadow::NewLine(); // 强行中断横向流并换行
                        Shadow::Button("Btn C (Broken to Next Line)##SameBreak");
                        Shadow::HelpMarker("Calling NewLine() explicitly breaks and commits pending SameLine() horizontal chaining.");

                        // 4. 动态循环网格排版末尾安全收尾
                        Shadow::Text("Loop with conditional SameLine() and NewLine() cleanup:");
                        for (int i = 0; i < 3; ++i) {
                            Shadow::Button(std::format("Grid Item {}##GridDemo", i + 1));
                            if (i < 2) {
                                Shadow::SameLine();
                            }
                        }
                        Shadow::NewLine(); // 收尾换行，防止后续控件被挤入网格末尾
                        Shadow::Text("Text safely placed below the 3-button row.");

                        Shadow::Separator();

                        // Indent & Unindent 完整功能演示
                        Shadow::Text("Indent & Unindent");

                        // 1. 默认参数缩进与恢复
                        Shadow::Text("Unindented text");
                        Shadow::Indent();
                        Shadow::Text("Indented text (Default Style IndentSpacing)");
                        Shadow::Button("Indented Button##DefIndent");
                        Shadow::Unindent();
                        Shadow::Text("Unindented text restored");

                        // 2. 自定义绝对像素宽度缩进与恢复
                        Shadow::Indent(40.0f);
                        Shadow::Text("Indented text (+40px custom width)");
                        Shadow::Button("Indented Button (+40px)##CustomIndent");
                        Shadow::Unindent(40.0f);
                        Shadow::Text("Restored after -40px unindent");

                        // 3. 多层嵌套缩进与逐级回退
                        Shadow::Indent();
                        Shadow::Text("Level 1 Indent");
                        Shadow::Indent();
                        Shadow::Text("Level 2 Indent (Nested)");
                        Shadow::Indent(50.0f);
                        Shadow::Text("Level 3 Indent (+50px Custom Nested)");
                        Shadow::Unindent(50.0f);
                        Shadow::Text("Back to Level 2");
                        Shadow::Unindent();
                        Shadow::Text("Back to Level 1");
                        Shadow::Unindent();
                        Shadow::Text("Back to Root Level");

                        Shadow::Separator();

                        Shadow::Text("Below is a 50x30 Dummy");
                        Shadow::Dummy({ 50.f, 30.f });
                        Shadow::Text("After Dummy");
                        Shadow::HelpMarker("Dummy inserts empty space manually.");

                        Shadow::Spacing();
                        Shadow::HelpMarker("Spacing() adds vertical padding between items.");

                        Shadow::PushTextWrapPos(150.f);
                        Shadow::Text("This text is forcibly wrapped at 150 pixels due to PushTextWrapPos.");
                        Shadow::PopTextWrapPos();
                        Shadow::HelpMarker("PushTextWrapPos / PopTextWrapPos Demonstration.");

                        Shadow::Vec2 cur = Shadow::g_Ctx.Cursor;
                        Shadow::PushClipRect(cur, { cur.x + 100.f, cur.y + Shadow::g_Ctx.ItemHeight });
                        Shadow::Text("This long text will be physically clipped at 100px width.");
                        Shadow::PopClipRect();
                        Shadow::HelpMarker("PushClipRect / PopClipRect Demonstration.");

                        Shadow::PushFont(Shadow::g_Ctx.DefaultFont, 1.5f);
                        Shadow::Text("Text with Font scale 1.5x");
                        Shadow::PopFont();
                        Shadow::HelpMarker("PushFont / PopFont Demonstration.");

                        Shadow::PushFont(Shadow::g_Ctx.DefaultFont, 3.f);
                        Shadow::Text("Text with Font scale 3.x");
                        Shadow::PopFont();
                        Shadow::HelpMarker("PushFont / PopFont Demonstration.");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Input Allowed Keys & Mouse")) {
                        static bool allow_w = false;
                        static bool allow_a = false;
                        static bool allow_mouse = false;
                        Shadow::Checkbox("Allow 'W' Key", &allow_w);
                        Shadow::Checkbox("Allow 'A' Key", &allow_a);
                        Shadow::Checkbox("Allow Mouse Move", &allow_mouse);

                        std::vector<int> keys;
                        if (allow_w) keys.push_back('W');
                        if (allow_a) keys.push_back('A');
                        Shadow::SetAllowedKeys(keys);

                        std::vector<UINT> msgs;
                        if (allow_mouse) msgs.push_back(WM_MOUSEMOVE);
                        Shadow::SetAllowedMouseMsgs(msgs);

                        Shadow::Text(std::format("IsKeyAllowed('W'): {}", Shadow::IsKeyAllowed('W') ? "TRUE" : "FALSE"));
                        Shadow::Text(std::format("IsMouseMsgAllowed(WM_MOUSEMOVE): {}", Shadow::IsMouseMsgAllowed(WM_MOUSEMOVE) ? "TRUE" : "FALSE"));
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Style Colors & Style Vars Stack")) {
                        // --- 1. PushStyleColor / PopStyleColor 基本演示 ---
                        if (Shadow::TreeNode("Style Colors (Basic & Nested)")) {
                            Shadow::Text("Default colored text");

                            // 单层 PushStyleColor
                            Shadow::PushStyleColor(Shadow::GuiCol_Text, { 1.0f, 0.2f, 0.2f, 1.0f });
                            Shadow::Text("Red text via PushStyleColor(GuiCol_Text)");

                            Shadow::PushStyleColor(Shadow::GuiCol_Button, { 0.8f, 0.4f, 0.0f, 1.0f });
                            Shadow::PushStyleColor(Shadow::GuiCol_ButtonHovered, { 1.0f, 0.6f, 0.1f, 1.0f });
                            Shadow::Button("Orange Button");
                            Shadow::PopStyleColor(2); // 批量弹出 2 个颜色
                            Shadow::PopStyleColor();  // 弹出文字颜色

                            Shadow::Text("Restored text color after PopStyleColor()");

                            // 嵌套 PushStyleColor
                            Shadow::PushStyleColor(Shadow::GuiCol_Text, { 0.2f, 0.8f, 1.0f, 1.0f });
                            Shadow::Text("Outer Blue Text");
                            {
                                Shadow::PushStyleColor(Shadow::GuiCol_Text, { 0.2f, 1.0f, 0.4f, 1.0f });
                                Shadow::Text("  -> Inner Green Text (Nested)");
                                Shadow::PopStyleColor();
                            }
                            Shadow::Text("Back to Outer Blue Text");
                            Shadow::PopStyleColor();

                            Shadow::HelpMarker("PushStyleColor pushes a temporary color state onto stack.\nPopStyleColor() restores previous color. Supports nesting and batch pop.");
                        }
                        Shadow::TreePop();

                        // --- 2. PushStyleVar / PopStyleVar (Float & Vec2 & Nested) ---
                        if (Shadow::TreeNode("Style Vars (Float, Vec2 & Nested)")) {
                            Shadow::Text("1. Vec2 StyleVar demo (FramePadding & ItemSpacing):");

                            // 修改 FramePadding (Vec2)
                            Shadow::PushStyleVar(Shadow::GuiStyleVar_FramePadding, { 16.f, 8.f });
                            Shadow::Button("Large FramePadding Button");
                            Shadow::PopStyleVar();

                            // 嵌套修改 ItemSpacing (Vec2) 与 FramePadding (Vec2)
                            Shadow::PushStyleVar(Shadow::GuiStyleVar_ItemSpacing, { 25.f, 20.f });
                            Shadow::PushStyleVar(Shadow::GuiStyleVar_FramePadding, { 12.f, 6.f });
                            Shadow::Button("Spaced Btn 1");
                            Shadow::SameLine();
                            Shadow::Button("Spaced Btn 2");
                            Shadow::PopStyleVar(2); // 批量恢复 2 个变量

                            Shadow::Text("2. Float StyleVar demo (ScrollbarSize & TabExtraWidth):");
                            Shadow::PushStyleVar(Shadow::GuiStyleVar_ScrollbarSize, 20.f);
                            Shadow::PushStyleVar(Shadow::GuiStyleVar_TabExtraWidth, 40.f);
                            Shadow::Text("Modified ScrollbarSize (20px) and TabExtraWidth (40px)");
                            Shadow::PopStyleVar(2);

                            Shadow::Text("3. More StyleVar demos:");
                            Shadow::PushStyleVar(Shadow::GuiStyleVar_WindowPadding, { 20.f, 20.f });
                            Shadow::Text("WindowPadding set to 20,20");
                            Shadow::PopStyleVar();

                            Shadow::PushStyleVar(Shadow::GuiStyleVar_ScrollbarMargin, 10.f);
                            Shadow::Text("ScrollbarMargin set to 10");
                            Shadow::PopStyleVar();

                            Shadow::PushStyleVar(Shadow::GuiStyleVar_ResizeGripSize, 20.f);
                            Shadow::Text("ResizeGripSize set to 20");
                            Shadow::PopStyleVar();

                            Shadow::PushStyleVar(Shadow::GuiStyleVar_ControlOffsetMin, 150.f);
                            Shadow::Text("ControlOffsetMin set to 150");
                            Shadow::PopStyleVar();

                            Shadow::PushStyleVar(Shadow::GuiStyleVar_ControlOffsetRatio, 0.6f);
                            Shadow::Text("ControlOffsetRatio set to 0.6");
                            Shadow::PopStyleVar();

                            Shadow::PushStyleVar(Shadow::GuiStyleVar_CPPadding, 12.f);
                            Shadow::Text("CPPadding set to 12");
                            Shadow::PopStyleVar();

                            Shadow::PushStyleVar(Shadow::GuiStyleVar_CPSVSize, 300.f);
                            Shadow::Text("CPSVSize set to 300");
                            Shadow::PopStyleVar();

                            Shadow::PushStyleVar(Shadow::GuiStyleVar_CPHueWidth, 30.f);
                            Shadow::Text("CPHueWidth set to 30");
                            Shadow::PopStyleVar();

                            Shadow::PushStyleVar(Shadow::GuiStyleVar_CPAlphaWidth, 30.f);
                            Shadow::Text("CPAlphaWidth set to 30");
                            Shadow::PopStyleVar();

                            Shadow::PushStyleVar(Shadow::GuiStyleVar_CPSpacing, 12.f);
                            Shadow::Text("CPSpacing set to 12");
                            Shadow::PopStyleVar();

                            Shadow::PushStyleVar(Shadow::GuiStyleVar_WindowMinSize, { 300.f, 200.f });
                            Shadow::Text("WindowMinSize set to 300,200");
                            Shadow::PopStyleVar();

                            Shadow::PushStyleVar(Shadow::GuiStyleVar_FontScaleDpi, 1.5f);
                            Shadow::Text("FontScaleDpi set to 1.5");
                            Shadow::PopStyleVar();

                            Shadow::HelpMarker("PushStyleVar supports both float (e.g. ScrollbarSize) and Vec2 (e.g. FramePadding, ItemSpacing).\nPopStyleVar() restores previous style metric unconditionally.");
                        }
                        Shadow::TreePop();
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Disabled Status")) {
                        static bool disable_all = true;
                        Shadow::Checkbox("Disable the following group", &disable_all);

                        Shadow::BeginDisabled(disable_all);
                        Shadow::Button("Disabled Button");
                        static float dummy_f = 0.5f;
                        Shadow::Slider("Disabled Slider", &dummy_f, 0.f, 1.f);
                        Shadow::EndDisabled();

                        Shadow::HelpMarker("Widgets bounded by BeginDisabled(bool) / EndDisabled().");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("TreeNodes & Flags")) {
                        if (Shadow::TreeNode("Default Open Node", Shadow::ShadowTreeNodeFlags_DefaultOpen)) {
                            Shadow::Text("This is open by default.");
                        }
                        Shadow::TreePop(); // Unconditional Pop

                        if (Shadow::TreeNode("Framed Node", Shadow::ShadowTreeNodeFlags_Framed)) {
                            Shadow::Text("This node has a background frame.");
                        }
                        Shadow::TreePop();

                        if (Shadow::TreeNode("FitText Framed Node", Shadow::ShadowTreeNodeFlags_Framed | Shadow::ShadowTreeNodeFlags_FitText)) {
                            Shadow::Text("The frame stops at the text end, ignoring right margin.");
                        }
                        Shadow::TreePop();

                        if (Shadow::TreeNode("No Indent Node", Shadow::ShadowTreeNodeFlags_NoIndent)) {
                            Shadow::Text("Children inside this node are not indented horizontally.");
                        }
                        Shadow::TreePop();

                        // TreeNode Flags combined demos
                        if (Shadow::TreeNode("All Flags Combo Node", Shadow::ShadowTreeNodeFlags_Framed | Shadow::ShadowTreeNodeFlags_DefaultOpen | Shadow::ShadowTreeNodeFlags_FitText | Shadow::ShadowTreeNodeFlags_NoIndent)) {
                            Shadow::Text("This node combines Framed, DefaultOpen, FitText, and NoIndent.");
                        }
                        Shadow::TreePop();
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Hovered Flags Demo")) {
                        Shadow::Button("Hover Me (Normal)");
                        if (Shadow::IsItemHovered(Shadow::ShadowHoveredFlags_None)) {
                            Shadow::BeginTooltip(); Shadow::Text("Normal Hover"); Shadow::EndTooltip();
                        }

                        Shadow::Button("Hover Me (Delay Short)");
                        if (Shadow::IsItemHovered(Shadow::ShadowHoveredFlags_DelayShort)) {
                            Shadow::BeginTooltip(); Shadow::Text("Short Delay Hover Tooltip"); Shadow::EndTooltip();
                        }

                        Shadow::Button("Hover Me (Delay Normal)");
                        if (Shadow::IsItemHovered(Shadow::ShadowHoveredFlags_DelayNormal)) {
                            Shadow::BeginTooltip(); Shadow::Text("Normal Delay Hover Tooltip"); Shadow::EndTooltip();
                        }

                        Shadow::Button("Hover Me (Stationary)");
                        if (Shadow::IsItemHovered(Shadow::ShadowHoveredFlags_Stationary)) {
                            Shadow::BeginTooltip(); Shadow::Text("Stationary Hover Tooltip"); Shadow::EndTooltip();
                        }

                        // Hover Flags combined demos
                        Shadow::Button("Hover Me (DelayNormal + Stationary)");
                        if (Shadow::IsItemHovered(Shadow::ShadowHoveredFlags_DelayNormal | Shadow::ShadowHoveredFlags_Stationary)) {
                            Shadow::BeginTooltip(); Shadow::Text("DelayNormal + Stationary Hover Tooltip"); Shadow::EndTooltip();
                        }

                        Shadow::BeginDisabled(true);
                        Shadow::Button("Hover Disabled (DelayShort + Allow)");
                        if (Shadow::IsItemHovered(Shadow::ShadowHoveredFlags_DelayShort | Shadow::ShadowHoveredFlags_AllowWhenDisabled)) {
                            Shadow::BeginTooltip(); Shadow::Text("DelayShort + AllowWhenDisabled Hover Tooltip"); Shadow::EndTooltip();
                        }
                        Shadow::EndDisabled();

                        Shadow::Button("Hover Me (AllowWhenBlockedByPopup)");
                        if (Shadow::IsItemHovered(Shadow::ShadowHoveredFlags_AllowWhenBlockedByPopup)) {
                            Shadow::BeginTooltip(); Shadow::Text("AllowWhenBlockedByPopup Hover Tooltip"); Shadow::EndTooltip();
                        }

                        Shadow::Button("Hover Me (AllowWhenBlockedByActiveItem)");
                        if (Shadow::IsItemHovered(Shadow::ShadowHoveredFlags_AllowWhenBlockedByActiveItem)) {
                            Shadow::BeginTooltip(); Shadow::Text("AllowWhenBlockedByActiveItem Hover Tooltip"); Shadow::EndTooltip();
                        }

                        Shadow::Button("Hover Me (DelayNormal + NoSharedDelay)");
                        if (Shadow::IsItemHovered(Shadow::ShadowHoveredFlags_DelayNormal | Shadow::ShadowHoveredFlags_NoSharedDelay)) {
                            Shadow::BeginTooltip(); Shadow::Text("DelayNormal + NoSharedDelay Hover Tooltip"); Shadow::EndTooltip();
                        }
                    }
                    Shadow::TreePop();

                    // Item Status & Query API Demonstrations
                    if (Shadow::TreeNode("Item Status & Query APIs")) {

                        // --- 1. IsItemActive, IsItemActivated, IsItemDeactivated ---
                        if (Shadow::TreeNode("Active / Activated / Deactivated")) {
                            static float slider_val = 0.5f;
                            static int activated_count = 0;
                            static int deactivated_count = 0;

                            static int btn_activated_count = 0;
                            static int btn_deactivated_count = 0;

                            Shadow::Slider("Test Slider##StatusDemo", &slider_val, 0.0f, 1.0f);
                            bool slider_active = Shadow::IsItemActive();
                            if (Shadow::IsItemActivated()) {
                                activated_count++;
                            }
                            if (Shadow::IsItemDeactivated()) {
                                deactivated_count++;
                            }

                            Shadow::Text(std::format("Slider IsActive: {} | Activated Count: {} | Deactivated Count: {}",
                                slider_active ? "TRUE" : "FALSE", activated_count, deactivated_count));

                            Shadow::Button("Hold/Click Button##StatusBtn");
                            bool btn_active = Shadow::IsItemActive();
                            if (Shadow::IsItemActivated()) {
                                btn_activated_count++;
                            }
                            if (Shadow::IsItemDeactivated()) {
                                btn_deactivated_count++;
                            }

                            Shadow::Text(std::format("Button IsActive: {} | Activated Count: {} | Deactivated Count: {}",
                                btn_active ? "TRUE" : "FALSE",
                                btn_activated_count,
                                btn_deactivated_count));

                            Shadow::HelpMarker("IsItemActive() returns true while dragging/holding.\nIsItemActivated() returns true on the exact frame it becomes active.\nIsItemDeactivated() returns true on the frame it is released.");
                        }
                        Shadow::TreePop();

                        // --- 2. IsItemClicked with Left, Right, Middle buttons ---
                        if (Shadow::TreeNode("IsItemClicked (Left / Right / Middle)")) {
                            static std::string last_click_btn = "None";

                            Shadow::Button("Click Me (Left / Right / Middle)##MultiClickBtn", { 250.f, 0.f });

                            if (Shadow::IsItemClicked(Shadow::ShadowMouseButton_Left)) {
                                last_click_btn = "Left Mouse Button";
                            }
                            else if (Shadow::IsItemClicked(Shadow::ShadowMouseButton_Right)) {
                                last_click_btn = "Right Mouse Button";
                            }
                            else if (Shadow::IsItemClicked(Shadow::ShadowMouseButton_Middle)) {
                                last_click_btn = "Middle Mouse Button";
                            }

                            Shadow::Text(std::format("Last Clicked With: {}", last_click_btn));
                            Shadow::HelpMarker("Shadow::IsItemClicked supports querying ShadowMouseButton_Left (0), Right (1), and Middle (2).");
                        }
                        Shadow::TreePop();

                        // --- 3. IsItemVisible ---
                        if (Shadow::TreeNode("IsItemVisible")) {
                            static bool is_vis = false;
                            Shadow::Text(std::format("Above Button Visible in viewport: {}", is_vis ? "TRUE" : "FALSE"));
                            Shadow::HelpMarker("IsItemVisible() returns false if the item was completely clipped outside the window/clipping area.");

                            Shadow::Button("Visible Check Button##VisBtn", { 500.f, 500.f });
                            is_vis = Shadow::IsItemVisible();
                        }
                        Shadow::TreePop();

                        // --- 4. GetItemRectMin, GetItemRectMax, GetItemRectSize ---
                        if (Shadow::TreeNode("Item Rect Bounds & Coordinates")) {
                            Shadow::Button("Target Rect Button##BoundsBtn");
                            Shadow::Vec2 rect_min = Shadow::GetItemRectMin();
                            Shadow::Vec2 rect_max = Shadow::GetItemRectMax();
                            Shadow::Vec2 rect_sz = Shadow::GetItemRectSize();

                            Shadow::Text(std::format("RectMin: ({:.1f}, {:.1f})", rect_min.x, rect_min.y));
                            Shadow::Text(std::format("RectMax: ({:.1f}, {:.1f})", rect_max.x, rect_max.y));
                            Shadow::Text(std::format("RectSize: {:.1f} x {:.1f}", rect_sz.x, rect_sz.y));

                            // 可视化：绘制上一项包围盒的绿色外框
                            Shadow::GetWindowDrawList()->AddRect(
                                { rect_min.x - 2.f, rect_min.y - 2.f },
                                { rect_sz.x + 4.f, rect_sz.y + 4.f },
                                Shadow::GetColor({ 0.f, 1.f, 0.f, 0.8f }),
                                1.5f
                            );

                            Shadow::HelpMarker("GetItemRectMin / GetItemRectMax / GetItemRectSize return the absolute screen AABB of the previous item.");
                        }
                        Shadow::TreePop();

                        // --- 5. GetItemID ---
                        if (Shadow::TreeNode("GetItemID")) {
                            Shadow::Button("Sample ID Button##UniqueBtn");
                            size_t item_id = Shadow::GetItemID();

                            Shadow::Text(std::format("Button Global Hash ID: 0x{:016X}", item_id));
                            Shadow::HelpMarker("GetItemID() returns the calculated 64-bit unique hash ID of the previous widget.");
                        }
                        Shadow::TreePop();

                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Window Flags Editor")) {
                        static bool f_NoResize = false, f_NoMove = false, f_NoScrollbar = false, f_NoTitleBar = false, f_NoMouseInputs = false, f_MenuBar = true;

                        if (Shadow::Selectable("MenuBar", &f_MenuBar)) { if (f_MenuBar) win_flags |= Shadow::ShadowWindowFlags_MenuBar; else win_flags &= ~Shadow::ShadowWindowFlags_MenuBar; }
                        if (Shadow::Selectable("NoResize", &f_NoResize)) { if (f_NoResize) win_flags |= Shadow::ShadowWindowFlags_NoResize; else win_flags &= ~Shadow::ShadowWindowFlags_NoResize; }
                        if (Shadow::Selectable("NoMove", &f_NoMove)) { if (f_NoMove) win_flags |= Shadow::ShadowWindowFlags_NoMove; else win_flags &= ~Shadow::ShadowWindowFlags_NoMove; }
                        if (Shadow::Selectable("NoScrollbar", &f_NoScrollbar)) { if (f_NoScrollbar) win_flags |= Shadow::ShadowWindowFlags_NoScrollbar; else win_flags &= ~Shadow::ShadowWindowFlags_NoScrollbar; }
                        if (Shadow::Selectable("NoTitleBar", &f_NoTitleBar)) { if (f_NoTitleBar) win_flags |= Shadow::ShadowWindowFlags_NoTitleBar; else win_flags &= ~Shadow::ShadowWindowFlags_NoTitleBar; }
                        if (Shadow::Selectable("NoMouseInputs", &f_NoMouseInputs)) { if (f_NoMouseInputs) win_flags |= Shadow::ShadowWindowFlags_NoMouseInputs; else win_flags &= ~Shadow::ShadowWindowFlags_NoMouseInputs; }

                        static int align_idx = 0;
                        std::vector<std::string> aligns = { "Left", "Center", "Right" };

                        Shadow::Combo("TextAlign (Applies to Title)", &align_idx, aligns);
                        win_flags &= ~(Shadow::ShadowWindowFlags_TextAlignCenter | Shadow::ShadowWindowFlags_TextAlignRight);
                        if (align_idx == 1) win_flags |= Shadow::ShadowWindowFlags_TextAlignCenter;
                        if (align_idx == 2) win_flags |= Shadow::ShadowWindowFlags_TextAlignRight;
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("TabBar Flags Editor")) {
                        static bool t_Reorderable = true, t_FittingPolicyScroll = true, t_NoScrollbar = false;

                        if (Shadow::Selectable("Reorderable", &t_Reorderable)) { if (t_Reorderable) tab_flags |= Shadow::ShadowTabBarFlags_Reorderable; else tab_flags &= ~Shadow::ShadowTabBarFlags_Reorderable; }
                        if (Shadow::Selectable("FittingPolicyScroll", &t_FittingPolicyScroll)) { if (t_FittingPolicyScroll) tab_flags |= Shadow::ShadowTabBarFlags_FittingPolicyScroll; else tab_flags &= ~Shadow::ShadowTabBarFlags_FittingPolicyScroll; }
                        if (Shadow::Selectable("NoScrollbar", &t_NoScrollbar)) { if (t_NoScrollbar) tab_flags |= Shadow::ShadowTabBarFlags_NoScrollbar; else tab_flags &= ~Shadow::ShadowTabBarFlags_NoScrollbar; }
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Style Menu Colors Editor")) {
                        const char* bg_names_en[] = { "WindowBg", "TitleBarBg", "FrameBg", "FrameBgHovered", "PopupBg", "Tab", "TabHovered", "TabActive", "SwitchBg", "SwitchBgHovered", "SwitchBgActive", "SwitchBgActiveHovered", "DropdownActive" };
                        int bg_ids[] = { Shadow::GuiCol_WindowBg, Shadow::GuiCol_TitleBarBg, Shadow::GuiCol_FrameBg, Shadow::GuiCol_FrameBgHovered, Shadow::GuiCol_PopupBg, Shadow::GuiCol_Tab, Shadow::GuiCol_TabHovered, Shadow::GuiCol_TabActive, Shadow::GuiCol_SwitchBg, Shadow::GuiCol_SwitchBgHovered, Shadow::GuiCol_SwitchBgActive, Shadow::GuiCol_SwitchBgActiveHovered, Shadow::GuiCol_DropdownActive };

                        const char* text_names_en[] = { "Text", "TextDisabled", "TextHighlight", "ErrorText", "TextShadow", "TextOutline" };
                        int text_ids[] = { Shadow::GuiCol_Text, Shadow::GuiCol_TextDisabled, Shadow::GuiCol_TextHighlight, Shadow::GuiCol_ErrorText, Shadow::GuiCol_TextShadow, Shadow::GuiCol_TextOutline };

                        const char* btn_names_en[] = { "Button", "ButtonHovered", "SliderGrab", "SliderKnob", "CheckMark", "Separator", "ResizeGrip", "ResizeGripHovered", "ResizeGripActive", "ActiveIndicator", "InactiveIndicator", "Border", "PopupBorder", "ControlDisabled", "SwitchKnob" };
                        int btn_ids[] = { Shadow::GuiCol_Button, Shadow::GuiCol_ButtonHovered, Shadow::GuiCol_SliderGrab, Shadow::GuiCol_SliderKnob, Shadow::GuiCol_CheckMark, Shadow::GuiCol_Separator, Shadow::GuiCol_ResizeGrip, Shadow::GuiCol_ResizeGripHovered, Shadow::GuiCol_ResizeGripActive, Shadow::GuiCol_ActiveIndicator, Shadow::GuiCol_InactiveIndicator, Shadow::GuiCol_Border, Shadow::GuiCol_PopupBorder, Shadow::GuiCol_ControlDisabled, Shadow::GuiCol_SwitchKnob };

                        const char* cp_names_en[] = { "ColorPickerDark", "ColorPickerLight", "CheckerboardLight", "CheckerboardDark", "ColorPickerShadow" };
                        int cp_ids[] = { Shadow::GuiCol_ColorPickerDark, Shadow::GuiCol_ColorPickerLight, Shadow::GuiCol_CheckerboardLight, Shadow::GuiCol_CheckerboardDark, Shadow::GuiCol_ColorPickerShadow };

                        if (Shadow::TreeNode("Backgrounds")) {
                            for (size_t i = 0; i < sizeof(bg_ids) / sizeof(bg_ids[0]); ++i) {
                                Shadow::Color& c = Shadow::g_Ctx.Style.Colors[bg_ids[i]];
                                Shadow::ColorPicker(bg_names_en[i], &c.r, &c.g, &c.b, &c.a);
                            }
                        }
                        Shadow::TreePop();

                        if (Shadow::TreeNode("Texts")) {
                            for (size_t i = 0; i < sizeof(text_ids) / sizeof(text_ids[0]); ++i) {
                                Shadow::Color& c = Shadow::g_Ctx.Style.Colors[text_ids[i]];
                                Shadow::ColorPicker(text_names_en[i], &c.r, &c.g, &c.b, &c.a);
                            }
                        }
                        Shadow::TreePop();

                        if (Shadow::TreeNode("Controls & Borders")) {
                            for (size_t i = 0; i < sizeof(btn_ids) / sizeof(btn_ids[0]); ++i) {
                                Shadow::Color& c = Shadow::g_Ctx.Style.Colors[btn_ids[i]];
                                Shadow::ColorPicker(btn_names_en[i], &c.r, &c.g, &c.b, &c.a);
                            }
                        }
                        Shadow::TreePop();

                        if (Shadow::TreeNode("Color Picker Specific")) {
                            for (size_t i = 0; i < sizeof(cp_ids) / sizeof(cp_ids[0]); ++i) {
                                Shadow::Color& c = Shadow::g_Ctx.Style.Colors[cp_ids[i]];
                                Shadow::ColorPicker(cp_names_en[i], &c.r, &c.g, &c.b, &c.a);
                            }
                        }
                        Shadow::TreePop();
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Style Metrics & Themes")) {
                        static int theme_idx = 1;
                        std::vector<std::string> themes = { "Dark", "Ocean", "Amethyst", "Grey" };
                        if (Shadow::Combo("Theme", &theme_idx, themes)) {
                            switch (theme_idx) {
                            case 0: Shadow::StyleColorsDark(); break;
                            case 1: Shadow::StyleColorsOcean(); break;
                            case 2: Shadow::StyleColorsAmethyst(); break;
                            case 3: Shadow::StyleColorsGrey(); break;
                            }
                        }
                        Shadow::HelpMarker("Theme switch demo using StyleColorsDark / Ocean / Amethyst / Grey.");

                        auto& style = Shadow::GetStyle();
                        Shadow::Slider("WindowPadding.x", &style.WindowPadding.x, 0.f, 64.f, 1.f);
                        Shadow::Slider("WindowPadding.y", &style.WindowPadding.y, 0.f, 64.f, 1.f);
                        Shadow::Slider("FramePadding.x", &style.FramePadding.x, 0.f, 32.f, 1.f);
                        Shadow::Slider("FramePadding.y", &style.FramePadding.y, 0.f, 32.f, 1.f);
                        Shadow::Slider("ItemSpacing.x", &style.ItemSpacing.x, 0.f, 64.f, 1.f);
                        Shadow::Slider("ItemSpacing.y", &style.ItemSpacing.y, 0.f, 64.f, 1.f);
                        Shadow::Slider("ScrollbarSize", &style.ScrollbarSize, 4.f, 32.f, 1.f);
                        Shadow::Slider("ScrollbarMargin", &style.ScrollbarMargin, 0.f, 32.f, 1.f);
                        Shadow::Slider("ResizeGripSize", &style.ResizeGripSize, 4.f, 32.f, 1.f);
                        Shadow::Slider("TabExtraWidth", &style.TabExtraWidth, 0.f, 64.f, 1.f);
                        Shadow::Slider("ControlOffsetMin", &style.ControlOffsetMin, 0.f, 400.f, 1.f);
                        Shadow::Slider("ControlOffsetRatio", &style.ControlOffsetRatio, 0.f, 1.f, 0.01f);
                        Shadow::Slider("FontScaleDpi", &style.FontScaleDpi, 0.5f, 3.f, 0.05f);
                        Shadow::HelpMarker("Style metrics editor using GetStyle().");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Clipboard")) {
                        static std::string clip_text = "Shadow Clipboard";
                        Shadow::InputText("Clipboard Text", clip_text);
                        if (Shadow::Button("Copy to Clipboard")) {
                            Shadow::SetClipboardText(clip_text);
                        }
                        Shadow::SameLine();
                        if (Shadow::Button("Paste from Clipboard")) {
                            clip_text = Shadow::GetClipboardText();
                        }
                        Shadow::HelpMarker("SetClipboardText / GetClipboardText demo.");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Window Pos/Size & Constraints")) {
                        Vec2 pos = Shadow::GetWindowPos();
                        Vec2 size = Shadow::GetWindowSize();
                        Shadow::Text(std::format("Window Pos: {:.1f}, {:.1f}", pos.x, pos.y));
                        Shadow::Text(std::format("Window Size: {:.1f}, {:.1f}", size.x, size.y));

                        if (Shadow::Button("SetWindowPos to 50,50")) {
                            Shadow::SetWindowPos({ 50.f, 50.f });
                        }
                        Shadow::SameLine();
                        if (Shadow::Button("SetNextWindowPos to 200,200")) {
                            Shadow::SetNextWindowPos({ 200.f, 200.f });
                        }
                        if (Shadow::Button("SetNextWindowSize 600x500")) {
                            Shadow::SetNextWindowSize({ 600.f, 500.f });
                        }
                        Shadow::SameLine();
                        if (Shadow::Button("SetNextWindowSizeConstraints 300x300 - 800x700")) {
                            Shadow::SetNextWindowSizeConstraints({ 300.f, 300.f }, { 800.f, 700.f });
                        }
                        Shadow::HelpMarker("Window position/size and constraints demo. Next window calls affect next frame Begin().");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("DrawList Channels")) {
                        Vec2 p = Shadow::g_Ctx.Cursor;
                        Shadow::Dummy({ 220.f, 100.f });

                        ShadowDrawList* dl = Shadow::GetWindowDrawList();
                        dl->ChannelsSplit(2);
                        dl->SetChannel(0);
                        dl->AddRectFilled(p, { 120.f, 80.f }, Shadow::GetColor({ 1.f, 0.f, 0.f, 0.8f }));
                        dl->SetChannel(1);
                        dl->AddRectFilled({ p.x + 60.f, p.y + 20.f }, { 120.f, 80.f }, Shadow::GetColor({ 0.f, 0.f, 1.f, 0.8f }));
                        dl->ChannelsMerge();

                        Shadow::HelpMarker("ChannelsSplit / SetChannel / ChannelsMerge demo.");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("DrawList Primitives")) {
                        Shadow::Vec2 p = Shadow::g_Ctx.Cursor;
                        Shadow::Dummy({ 300.f, 200.f });
                        ShadowDrawList* dl = Shadow::GetWindowDrawList();
                        dl->AddLine({ p.x, p.y }, { p.x + 100.f, p.y + 50.f }, Shadow::GetColor({ 1.f, 0.f, 0.f, 1.f }), 2.f);
                        dl->AddRect({ p.x + 120.f, p.y }, { 80.f, 60.f }, Shadow::GetColor({ 0.f, 1.f, 0.f, 1.f }), 2.f);
                        dl->AddRectFilled({ p.x + 220.f, p.y }, { 60.f, 40.f }, Shadow::GetColor({ 0.f, 0.f, 1.f, 1.f }));
                        dl->AddCircleFilled({ p.x + 50.f, p.y + 120.f }, 30.f, Shadow::GetColor({ 1.f, 1.f, 0.f, 1.f }));
                        dl->AddTriangle({ p.x + 120.f, p.y + 100.f }, { p.x + 180.f, p.y + 100.f }, { p.x + 150.f, p.y + 150.f }, Shadow::GetColor({ 1.f, 0.f, 1.f, 1.f }), 2.f);
                        dl->AddTriangleFilled({ p.x + 200.f, p.y + 100.f }, { p.x + 260.f, p.y + 100.f }, { p.x + 230.f, p.y + 150.f }, Shadow::GetColor({ 0.f, 1.f, 1.f, 1.f }));
                        dl->AddText({ p.x + 10.f, p.y + 180.f }, Shadow::GetColor({ 1.f, 1.f, 1.f, 1.f }), "DrawList Text");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("DrawList Texture & PushTexture")) {
                        SDK::UTexture2D* TitlebarTex = Shadow::LoadTextureFromBuffer(Shadow_Titlebar::Logo, sizeof(Shadow_Titlebar::Logo));

                        Vec2 p = Shadow::g_Ctx.Cursor;
                        Shadow::Dummy({ 200.f, 80.f });

                        ShadowDrawList* dl = Shadow::GetWindowDrawList();
                        if (TitlebarTex) {
                            dl->AddTexture(p, { 64.f, 64.f }, Shadow::GetColor({ 1.f, 1.f, 1.f, 1.f }), TitlebarTex);
                            Shadow::PushTexture(TitlebarTex);
                            dl->AddTexture({ p.x + 80.f, p.y }, { 64.f, 64.f }, Shadow::GetColor({ 1.f, 0.5f, 0.5f, 1.f }));
                            Shadow::PopTexture();
                        }

                        Shadow::HelpMarker("AddTexture explicit texture and via PushTexture / PopTexture.");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Load Texture & Font from File")) {
                        static std::string tex_path = "C:\\path\\to\\texture.png";
                        static std::string font_path = "C:\\path\\to\\font.ttf";
                        Shadow::InputText("Texture Path", tex_path);
                        Shadow::InputText("Font Path", font_path);
                        if (Shadow::Button("Load Texture")) {
                            SDK::UTexture2D* tex = Shadow::LoadTextureFromFile(Shadow::ToWString(tex_path).c_str());
                            if (tex) {
                                Shadow::Text("Texture loaded!");
                            }
                            else {
                                Shadow::Text("Failed to load texture.");
                            }
                        }
                        Shadow::SameLine();
                        if (Shadow::Button("Load Font")) {
                            SDK::UFont* font = Shadow::LoadFontFromFile(Shadow::ToWString(font_path).c_str());
                            if (font) {
                                Shadow::Text("Font loaded!");
                            }
                            else {
                                Shadow::Text("Failed to load font.");
                            }
                        }
                        static std::vector<unsigned char> font_buffer;
                        if (Shadow::Button("Load Font from Buffer")) {
                            if (!font_buffer.empty()) {
                                SDK::UFont* font = Shadow::LoadFontFromBuffer(font_buffer.data(), font_buffer.size());
                                if (font) {
                                    Shadow::Text("Font from buffer loaded!");
                                }
                            }
                        }
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Font, PixelSnap & ID Stack")) {
                        Shadow::PushFont(Shadow::g_Ctx.DefaultFont, 1.5f);
                        Shadow::Text("PushFont 1.5x");
                        Shadow::PopFont();

                        Shadow::PushFontNoSDF(Shadow::g_Ctx.DefaultFont, 1.5f);
                        Shadow::Text("PushFontNoSDF 1.5x");
                        Shadow::PopFont();
                        Shadow::HelpMarker("PushFontNoSDF / PopFont demo.");

                        Shadow::PushTextPixelSnap(true);
                        Shadow::Text("Pixel snapped text");
                        Shadow::PopTextPixelSnap();
                        Shadow::HelpMarker("PushTextPixelSnap / PopTextPixelSnap demo.");

                        Shadow::PushID("id_a");
                        Shadow::Button("Same Label##1");
                        Shadow::PopID();
                        Shadow::PushID("id_b");
                        Shadow::Button("Same Label##1");
                        Shadow::PopID();
                        Shadow::HelpMarker("PushID / PopID demo with same visible label.");
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("Popup Examples")) {
                        static int popup_counter = 0;
                        static std::string popupName = "my_popup##my_popup";

                        if (Shadow::Button("Open Popup")) {
                            Shadow::SetNextWindowPos({ Shadow::g_Ctx.MousePos.x + 20, Shadow::g_Ctx.MousePos.y + 20 });
                            Shadow::OpenPopup(Shadow::HashString(popupName));
                        }
                        Shadow::SameLine();
                        Shadow::TextColored(Shadow::g_Ctx.Style.Colors[Shadow::GuiCol_TextDisabled],
                            "(Click to open a popup at mouse position)");

                        bool is_open = Shadow::IsPopupOpen(Shadow::HashString(popupName));
                        std::string status_text = std::format("Popup is currently: {}", is_open ? "Open" : "Closed");

                        Shadow::Text(status_text);
                        Shadow::SameLine();

                        if (Shadow::Button("Close Popup")) {
                            Shadow::CloseCurrentPopup();
                        }

                        if (Shadow::BeginPopup(popupName, Shadow::ShadowWindowFlags_None)) {
                            Shadow::Text("This is a popup!");

                            std::string counter_text = std::format("Counter: {}", popup_counter);

                            Shadow::Text(counter_text);

                            if (Shadow::Button("Increment")) {
                                popup_counter++;
                            }

                            Shadow::SameLine();

                            if (Shadow::Button("Close")) {
                                Shadow::CloseCurrentPopup();
                            }
                        }
                        Shadow::EndPopup();
                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("ListBox Demos")) {

                        if (Shadow::TreeNode("ListBox with Selectables")) {
                            static int selectedItem = 0;
                            std::vector<std::string> items = { "Apple", "Banana", "Cherry", "Date", "Elderberry", "Fig", "Grape", "Honeydew", "Kiwi", "Lemon" };

                            if (Shadow::BeginListBox("FruitListBox", { 200.f, 150.f })) {
                                for (int i = 0; i < static_cast<int>(items.size()); i++) {
                                    bool isSelected = (selectedItem == i);
                                    if (Shadow::Selectable(items[i], &isSelected)) {
                                        selectedItem = i;
                                    }
                                }
                            }
                            Shadow::EndListBox();
                            Shadow::HelpMarker("ListBox demonstrating internal Selectable usage for item selection.");
                            Shadow::Text(std::string("Selected: ") + items[selectedItem]);
                        }
                        Shadow::TreePop();

                        if (Shadow::TreeNode("ListBox with Color Texts")) {
                            struct ColorEntry {
                                std::string name;
                                Shadow::Color color;
                            };

                            std::vector<ColorEntry> colors = {
                                { "Red",    { 1.0f, 0.0f, 0.0f, 1.0f } },
                                { "Green",  { 0.0f, 1.0f, 0.0f, 1.0f } },
                                { "Blue",   { 0.0f, 0.0f, 1.0f, 1.0f } },
                                { "Yellow", { 1.0f, 1.0f, 0.0f, 1.0f } },
                                { "Cyan",   { 0.0f, 1.0f, 1.0f, 1.0f } },
                                { "Magenta",{ 1.0f, 0.0f, 1.0f, 1.0f } },
                                { "Orange", { 1.0f, 0.5f, 0.0f, 1.0f } }
                            };

                            if (Shadow::BeginListBox("ColorTextListBox", { 200.f, 150.f })) {
                                for (const auto& entry : colors) {
                                    Shadow::TextColored(entry.color, entry.name);
                                }
                            }
                            Shadow::EndListBox();
                            Shadow::HelpMarker("ListBox demonstrating TextColored items with various colors.");
                        }
                        Shadow::TreePop();

                        if (Shadow::TreeNode("ListBox with Color Pickers")) {
                            static std::vector<Shadow::Color> customColors = {
                                { 1.0f, 0.0f, 0.0f, 1.0f },
                                { 0.0f, 1.0f, 0.0f, 1.0f },
                                { 0.0f, 0.0f, 1.0f, 1.0f },
                                { 1.0f, 1.0f, 0.0f, 1.0f },
                                { 0.0f, 1.0f, 1.0f, 1.0f }
                            };

                            if (Shadow::BeginListBox("ColorPickerListBox", { 300.f, 180.f })) {
                                for (int i = 0; i < static_cast<int>(customColors.size()); i++) {
                                    Shadow::ColorPicker(
                                        std::string("Color ") + std::to_string(i + 1) + "##cp",
                                        &customColors[i].r,
                                        &customColors[i].g,
                                        &customColors[i].b,
                                        &customColors[i].a
                                    );
                                }
                            }
                            Shadow::EndListBox();
                            Shadow::HelpMarker("ListBox demonstrating embedded ColorPicker controls.");
                        }
                        Shadow::TreePop();

                    }
                    Shadow::TreePop();

                    if (Shadow::TreeNode("IO")) {
                        Shadow::ShadowIO& IO = Shadow::GetIO();
                        float fps = IO.DeltaTime > 0 ? 1.f / IO.DeltaTime : 0.f;
                        std::string fps_str = std::format("FPS: {:.0f}", fps);
                        Shadow::Text(fps_str);
                        Shadow::HelpMarker("using Shadow::GetIO().DeltaTime to get FPS.");

                        float screen_w = IO.DisplaySize.x;
                        float screen_h = IO.DisplaySize.y;
                        std::string screen_size_str = std::format("Screen: {} / {}", screen_w, screen_h);
                        Shadow::Text(screen_size_str);
                    }
                    Shadow::TreePop();

                }
                Shadow::EndTabItem(); // Unconditional EndTabItem

            }
            Shadow::EndTabBar(); // Unconditional EndTabBar

        }
        Shadow::End(); // Unconditional End
    }
} // namespace Shadow