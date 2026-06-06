// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"
#include <algorithm>

namespace ghostpad {

extern void renderInteractivePadVisualizer(App& app, PadStateInput& state, float size);

void renderControllerScreen(App& app) {
    const auto& p = ui::colors();
    auto status = app.ghostpad().getStatus();
    float avail_w = ImGui::GetContentRegionAvail().x;
    float avail_h = ImGui::GetContentRegionAvail().y;

    // Top status indicators
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(p.primary2, "%s  Virtual DualSense", ICON_FA_GAMEPAD);
    ImGui::SameLine();
    if (status.is_connected)
        ImGui::TextColored(p.success, "%s (P%d streaming to %s:%d)", ICON_FA_SIGNAL, app.activeSlot() + 1, status.ip.c_str(), status.port);
    else
        ImGui::TextColored(p.danger, "%s (P%d not connected)", ICON_FA_CIRCLE_XMARK, app.activeSlot() + 1);

    /*
     *  ┌──────────────────────────────────────────────────────────┐
     *  │                  ACTION BAR CONTROLS                     │
     *  └──────────────────────────────────────────────────────────┘
     */
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    bool compact = app.is_compact_device;

    if (compact) {
        float half_w = (avail_w - spacing) * 0.5f;

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(p.muted, "%s", ICON_FA_USER);
        ImGui::SameLine();
        ImGui::PushItemWidth(half_w - 30.0f);
        int slot = app.activeSlot();
        if (ImGui::Combo("##CtrlSlot", &slot, "P1\0P2\0P3\0P4\0")) {
            app.setActiveSlot(slot);
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();

        const char* pencil_icon = app.is_layout_edit_mode ? ICON_FA_XMARK : ICON_FA_PEN_TO_SQUARE;
        ImU32 pencil_col = app.is_layout_edit_mode ? ui::u32(p.danger) : ui::u32(p.primary2);
        ImGui::PushStyleColor(ImGuiCol_Text, pencil_col);
        if (ui::softButton(pencil_icon, ImVec2(40.0f, 30))) {
            if (!app.is_layout_edit_mode) {
                app.is_layout_edit_mode = true;
                app.temp_layout = app.settings.read().pad_layout;
                app.selected_layout_component = 0;
            } else {
                app.is_layout_edit_mode = false;
            }
        }
        ImGui::PopStyleColor();

        if (app.is_layout_edit_mode) {
            if (ui::dangerButton(ICON_FA_ARROW_ROTATE_LEFT "  Reset", ImVec2(half_w, 30))) {
                app.temp_layout = PadLayoutSettings{};
                app.addStatus("Layout reset to defaults");
            }
            ImGui::SameLine();
            if (ui::primaryButton(ICON_FA_CHECK "  Save", ImVec2(half_w, 30))) {
                auto s = app.settings.read();
                s.pad_layout = app.temp_layout;
                app.settings.write(s);
                app.is_layout_edit_mode = false;
                app.addStatus("Controller layout saved");
            }
        } else {
            if (ui::softButton(ICON_FA_ARROW_ROTATE_LEFT "  Reset", ImVec2(half_w, 30))) {
                app.virtual_pad = {};
            }
            ImGui::SameLine();
            if (!status.is_connected) {
                if (ui::primaryButton(ICON_FA_LINK "  Connect", ImVec2(half_w, 30))) {
                    app.current_screen = Screen::Consoles;
                }
            } else {
                ImGui::Dummy(ImVec2(half_w, 0));
            }
        }
    } else {
        float edit_pencil_w = 40.0f;
        float total_width = 0.0f;
        
        float slot_sel_w = 14.0f + spacing + 75.0f;
        total_width += slot_sel_w;
        total_width += spacing + edit_pencil_w;

        if (app.is_layout_edit_mode) {
            total_width += spacing + 130.0f + spacing + 120.0f;
        } else {
            total_width += spacing + 90.0f;
            if (!status.is_connected) {
                total_width += spacing + 100.0f;
            }
        }

        ImGui::SameLine(avail_w - total_width);

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(p.muted, "%s", ICON_FA_USER);
        ImGui::SameLine();
        ImGui::PushItemWidth(75);
        int slot = app.activeSlot();
        if (ImGui::Combo("##CtrlSlot", &slot, "P1\0P2\0P3\0P4\0")) {
            app.setActiveSlot(slot);
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();

        const char* pencil_icon = app.is_layout_edit_mode ? ICON_FA_XMARK : ICON_FA_PEN_TO_SQUARE;
        ImU32 pencil_col = app.is_layout_edit_mode ? ui::u32(p.danger) : ui::u32(p.primary2);
        ImGui::PushStyleColor(ImGuiCol_Text, pencil_col);
        if (ui::softButton(pencil_icon, ImVec2(edit_pencil_w, 30))) {
            if (!app.is_layout_edit_mode) {
                app.is_layout_edit_mode = true;
                app.temp_layout = app.settings.read().pad_layout;
                app.selected_layout_component = 0;
            } else {
                app.is_layout_edit_mode = false;
            }
        }
        ImGui::PopStyleColor();

        if (app.is_layout_edit_mode) {
            ImGui::SameLine();
            if (ui::dangerButton(ICON_FA_ARROW_ROTATE_LEFT "  Reset Defaults", ImVec2(130, 30))) {
                app.temp_layout = PadLayoutSettings{};
                app.addStatus("Layout reset to defaults");
            }
            ImGui::SameLine();
            if (ui::primaryButton(ICON_FA_CHECK "  Save Layout", ImVec2(120, 30))) {
                auto s = app.settings.read();
                s.pad_layout = app.temp_layout;
                app.settings.write(s);
                app.is_layout_edit_mode = false;
                app.addStatus("Controller layout saved");
            }
        } else {
            ImGui::SameLine();
            if (ui::softButton(ICON_FA_ARROW_ROTATE_LEFT "  Reset", ImVec2(90, 30))) {
                app.virtual_pad = {};
            }
            if (!status.is_connected) {
                ImGui::SameLine();
                if (ui::primaryButton(ICON_FA_LINK "  Connect", ImVec2(100, 30))) {
                    app.current_screen = Screen::Consoles;
                }
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────────────
    //                          CONTROLLER GRAPHICS AREA
    // ─────────────────────────────────────────────────────────────────────────────
    float panel_w = avail_w * 0.3f;
    if (panel_w > 320.0f) panel_w = 320.0f;
    if (panel_w < 200.0f) panel_w = 200.0f;
    
    bool show_panel_side = app.is_layout_edit_mode && !compact;
    float pad_area_w = show_panel_side ? (avail_w - panel_w - 20.0f) : avail_w;
    
    float pad_size = (std::min)(pad_area_w * 0.42f, (avail_h - 100.0f) * 0.70f);
    if (compact) {
        // Mobile: smaller controller to fit screen
        pad_size = (std::min)(pad_area_w * 0.42f, (avail_h - 140.0f) * 0.45f);
        if (pad_size < 100.0f) pad_size = 100.0f;
        if (pad_size > 160.0f) pad_size = 160.0f; // cap max size on mobile
    }
    float pad_w = pad_size * 2.0f;
    float pad_h = pad_size * 1.2f;
    
    float offset_x = (pad_area_w - pad_w) * 0.5f;
    float offset_y = (std::max)((avail_h - 90.0f - pad_h) * 0.4f, 10.0f);
    if (compact) offset_y = 10.0f;
    
    ImGui::BeginGroup();
    if (offset_y > 0) ImGui::Dummy(ImVec2(0, offset_y));
    if (offset_x > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);

    PadStateInput active_state = app.getCurrentPadState();
    bool mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    
    PadStateInput temp_state = active_state;
    PadStateInput& render_state = mouse_down ? app.virtual_pad : temp_state;

    renderInteractivePadVisualizer(app, render_state, pad_size);

    if (!app.is_layout_edit_mode && !mouse_down && !ImGui::IsAnyItemActive()) {
        for (int i = 0; i < 4; i++)
            app.virtual_pad.stick_states[i] = 128;
        app.virtual_pad.trigger_l2 = 0;
        app.virtual_pad.trigger_r2 = 0;
        for (int i = 0; i < 22; i++)
            app.virtual_pad.button_states[i] = false;
    }

    ImGui::Dummy(ImVec2(0, 16));
    
    // Use TextWrapped for mobile to prevent truncation
    if (compact) {
        ImGui::TextWrapped("%s  Press buttons on physical controller to see them light up in real-time.", ICON_FA_CIRCLE_INFO);
        ImGui::TextWrapped("Tip: Click & drag on the layout above to send virtual controller inputs.");
    } else {
        const char* hint1 = "Press buttons on physical controller to see them light up in real-time.";
        float text_w = ImGui::CalcTextSize(hint1).x;
        float text_x = (pad_area_w - text_w) * 0.5f;
        if (text_x < 8.0f) text_x = 8.0f;
        ImGui::SetCursorPosX(text_x);
        ImGui::TextColored(p.muted, "%s  %s", ICON_FA_CIRCLE_INFO, hint1);
        
        const char* hint2 = "Tip: Click & drag on the layout above to send virtual controller inputs.";
        text_w = ImGui::CalcTextSize(hint2).x;
        text_x = (pad_area_w - text_w) * 0.5f;
        if (text_x < 8.0f) text_x = 8.0f;
        ImGui::SetCursorPosX(text_x);
        ImGui::TextColored(p.dim, "%s", hint2);
    }
    ImGui::EndGroup();

    // ─────────────────────────────────────────────────────────────────────────────
    //                         PROPERTIES SIDEBAR PANEL
    // ─────────────────────────────────────────────────────────────────────────────
    if (app.is_layout_edit_mode) {
        if (show_panel_side) {
            ImGui::SameLine();
            ImGui::SetCursorPosX(pad_area_w + 20.0f);
        } else {
            ImGui::Spacing();
        }
        
        float panel_h = show_panel_side ? (avail_h - 90.0f) : (avail_h * 0.4f);
        ui::beginCard("EditPanel", ImVec2(compact ? avail_w : panel_w, panel_h));
        ui::sectionLabel("Properties", ICON_FA_PEN_TO_SQUARE);
        ImGui::Spacing();
        
        static const char* comp_names[] = {
            "None - Click a component",
            "Left Analog Stick",
            "Right Analog Stick",
            "D-pad",
            "Face Buttons",
            "Left Shoulders (L1/L2)",
            "Right Shoulders (R1/R2)",
            "Touchpad",
            "Create Button",
            "Options Button",
            "PS Button"
        };
        
        ImGui::TextColored(p.muted, "Select Component:");
        ImGui::PushItemWidth(-1);
        ImGui::Combo("##edit_comp", &app.selected_layout_component, comp_names, 11);
        ImGui::PopItemWidth();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (app.selected_layout_component > 0 && app.selected_layout_component < 11) {
            ComponentLayout* comp_layout = nullptr;
            switch (app.selected_layout_component) {
                case 1: comp_layout = &app.temp_layout.l_stick; break;
                case 2: comp_layout = &app.temp_layout.r_stick; break;
                case 3: {
                    // D-pad selection has multiple sub-modules
                    ImGui::TextColored(p.muted, "D-pad Module to Edit:");
                    static int dpad_module = 0; // 0 = Whole, 1 = Up, 2 = Down, 3 = Left, 4 = Right
                    static const char* dpad_modules[] = { "Whole D-pad", "Up Button", "Down Button", "Left Button", "Right Button" };
                    ImGui::PushItemWidth(-1);
                    ImGui::Combo("##dpad_mod", &dpad_module, dpad_modules, 5);
                    ImGui::PopItemWidth();
                    ImGui::Spacing();
                    
                    if (dpad_module == 0) {
                        ComponentLayout* ref = &app.temp_layout.dpad_up;
                        
                        ImGui::TextColored(p.primary2, "Whole D-pad Group");
                        ImGui::Spacing();
                        
                        ImGui::TextColored(p.muted, "Horizontal Position:");
                        float x_val = ref->x_offset;
                        if (ImGui::SliderFloat("##pos_x", &x_val, -1.5f, 1.5f, "%.3f")) {
                            float dx = x_val - ref->x_offset;
                            app.temp_layout.dpad_up.x_offset += dx;
                            app.temp_layout.dpad_down.x_offset += dx;
                            app.temp_layout.dpad_left.x_offset += dx;
                            app.temp_layout.dpad_right.x_offset += dx;
                        }
                        
                        ImGui::TextColored(p.muted, "Vertical Position:");
                        float y_val = ref->y_offset;
                        if (ImGui::SliderFloat("##pos_y", &y_val, -1.5f, 1.5f, "%.3f")) {
                            float dy = y_val - ref->y_offset;
                            app.temp_layout.dpad_up.y_offset += dy;
                            app.temp_layout.dpad_down.y_offset += dy;
                            app.temp_layout.dpad_left.y_offset += dy;
                            app.temp_layout.dpad_right.y_offset += dy;
                        }
                        
                        ImGui::TextColored(p.muted, "Scale:");
                        float scale_val = ref->scale;
                        if (ImGui::SliderFloat("##scale", &scale_val, 0.4f, 2.5f, "%.2fx")) {
                            app.temp_layout.dpad_up.scale = scale_val;
                            app.temp_layout.dpad_down.scale = scale_val;
                            app.temp_layout.dpad_left.scale = scale_val;
                            app.temp_layout.dpad_right.scale = scale_val;
                        }
                        
                        ImGui::TextColored(p.muted, "Button Casing Color:");
                        if (ImGui::ColorEdit4("##comp_color", ref->color.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
                            app.temp_layout.dpad_down.color = ref->color;
                            app.temp_layout.dpad_left.color = ref->color;
                            app.temp_layout.dpad_right.color = ref->color;
                        }
                        
                        ImGui::TextColored(p.muted, "Arrow Symbols Color:");
                        if (ImGui::ColorEdit4("##comp_sec_color", ref->secondary_color.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
                            app.temp_layout.dpad_down.secondary_color = ref->secondary_color;
                            app.temp_layout.dpad_left.secondary_color = ref->secondary_color;
                            app.temp_layout.dpad_right.secondary_color = ref->secondary_color;
                        }
                        
                        ImGui::Spacing();
                        ImGui::Spacing();
                        
                        if (ui::softButton(ICON_FA_ARROW_ROTATE_LEFT "  Reset D-pad Group", ImVec2(180, 30))) {
                            app.temp_layout.dpad_up = ComponentLayout();
                            app.temp_layout.dpad_down = ComponentLayout();
                            app.temp_layout.dpad_left = ComponentLayout();
                            app.temp_layout.dpad_right = ComponentLayout();
                            app.addStatus("D-pad group reset to default");
                        }
                    } else {
                        ComponentLayout* target = nullptr;
                        const char* mod_name = "";
                        if (dpad_module == 1) { target = &app.temp_layout.dpad_up; mod_name = "D-pad Up Button"; }
                        else if (dpad_module == 2) { target = &app.temp_layout.dpad_down; mod_name = "D-pad Down Button"; }
                        else if (dpad_module == 3) { target = &app.temp_layout.dpad_left; mod_name = "D-pad Left Button"; }
                        else if (dpad_module == 4) { target = &app.temp_layout.dpad_right; mod_name = "D-pad Right Button"; }
                        
                        if (target) {
                            ImGui::TextColored(p.primary2, "%s", mod_name);
                            ImGui::Spacing();
                            
                            ImGui::TextColored(p.muted, "Horizontal Position:");
                            ImGui::SliderFloat("##pos_x", &target->x_offset, -1.5f, 1.5f, "%.3f");
                            
                            ImGui::TextColored(p.muted, "Vertical Position:");
                            ImGui::SliderFloat("##pos_y", &target->y_offset, -1.5f, 1.5f, "%.3f");
                            
                            ImGui::TextColored(p.muted, "Scale:");
                            ImGui::SliderFloat("##scale", &target->scale, 0.4f, 2.5f, "%.2fx");
                            
                            ImGui::TextColored(p.muted, "Button Casing Color:");
                            ImGui::ColorEdit4("##comp_color", target->color.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                            
                            ImGui::TextColored(p.muted, "Arrow Symbol Color:");
                            ImGui::ColorEdit4("##comp_sec_color", target->secondary_color.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                            
                            ImGui::Spacing();
                            ImGui::Spacing();
                            
                            if (ui::softButton(ICON_FA_ARROW_ROTATE_LEFT "  Reset Button", ImVec2(180, 30))) {
                                *target = ComponentLayout();
                                app.addStatus(std::string(mod_name) + " reset to default");
                            }
                        }
                    }
                    break;
                }
                case 4: comp_layout = &app.temp_layout.face_buttons; break;
                case 5: comp_layout = &app.temp_layout.shoulders_l; break;
                case 6: comp_layout = &app.temp_layout.shoulders_r; break;
                case 7: comp_layout = &app.temp_layout.touchpad; break;
                case 8: comp_layout = &app.temp_layout.create_btn; break;
                case 9: comp_layout = &app.temp_layout.options_btn; break;
                case 10: comp_layout = &app.temp_layout.ps_btn; break;
            }
            
            if (comp_layout) {
                ImGui::TextColored(p.primary2, "%s", comp_names[app.selected_layout_component]);
                ImGui::Spacing();
                
                ImGui::TextColored(p.muted, "Horizontal Position:");
                ImGui::SliderFloat("##pos_x", &comp_layout->x_offset, -1.5f, 1.5f, "%.3f");
                
                ImGui::TextColored(p.muted, "Vertical Position:");
                ImGui::SliderFloat("##pos_y", &comp_layout->y_offset, -1.5f, 1.5f, "%.3f");
                
                ImGui::TextColored(p.muted, "Scale:");
                ImGui::SliderFloat("##scale", &comp_layout->scale, 0.4f, 2.5f, "%.2fx");
                
                ImGui::TextColored(p.muted, "Button Color:");
                ImGui::ColorEdit4("##comp_color", comp_layout->color.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                
                // Secondary symbol/icon color picker for components containing shapes
                if (app.selected_layout_component == 4 || app.selected_layout_component == 8 || 
                    app.selected_layout_component == 9 || app.selected_layout_component == 10) {
                    ImGui::TextColored(p.muted, "Symbol/Text Color:");
                    ImGui::ColorEdit4("##comp_sec_color", comp_layout->secondary_color.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                }
                
                ImGui::Spacing();
                ImGui::Spacing();
                
                if (ui::softButton(ICON_FA_ARROW_ROTATE_LEFT "  Reset Component", ImVec2(180, 30))) {
                    comp_layout->x_offset = 0.0f;
                    comp_layout->y_offset = 0.0f;
                    comp_layout->scale = 1.0f;
                    comp_layout->color = {0.725f, 0.549f, 1.0f, 1.0f};
                    comp_layout->secondary_color = {1.0f, 1.0f, 1.0f, 1.0f};
                    app.addStatus("Component reset to default");
                }
            }
        } else {
            ImGui::TextWrapped("Click on any button group on the controller to edit its position and size, or drag it directly on the screen.");
        }
        
        ui::endCard();
    }
}

} // namespace ghostpad
