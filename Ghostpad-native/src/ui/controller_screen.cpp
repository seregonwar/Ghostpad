// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"
#include <algorithm>

namespace ghostpad {

extern void renderInteractivePadVisualizer(PadStateInput& state, float size);

void renderControllerScreen(App& app) {
    const auto& p = ui::colors();
    auto status = app.ghostpad.getStatus();
    float avail_w = ImGui::GetContentRegionAvail().x;
    float avail_h = ImGui::GetContentRegionAvail().y;

    // Top status indicators
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(p.primary2, "%s  Virtual DualSense", ICON_FA_GAMEPAD);
    ImGui::SameLine();
    if (status.is_connected)
        ImGui::TextColored(p.success, "%s (streaming to %s:%d)", ICON_FA_SIGNAL, status.ip.c_str(), status.port);
    else
        ImGui::TextColored(p.danger, "%s (not connected)", ICON_FA_CIRCLE_XMARK);

    ImGui::SameLine(avail_w - 210);
    if (ui::softButton(ICON_FA_ARROW_ROTATE_LEFT "  Reset", ImVec2(90, 30)))
        app.virtual_pad = {};
    ImGui::SameLine();
    if (!status.is_connected && ui::primaryButton(ICON_FA_LINK "  Connect", ImVec2(100, 30)))
        app.current_screen = Screen::Consoles;

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Interactive pad - center it in remaining workspace
    float pad_size = std::min(avail_w * 0.42f, (avail_h - 100.0f) * 0.70f);
    float pad_w = pad_size * 2.0f;
    float pad_h = pad_size * 1.2f;
    
    float offset_x = (avail_w - pad_w) * 0.5f;
    float offset_y = std::max((avail_h - 90.0f - pad_h) * 0.4f, 10.0f);
    
    if (offset_y > 0) ImGui::Dummy(ImVec2(0, offset_y));
    if (offset_x > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);

    // Active input mirroring logic:
    // If the user drags/clicks with the mouse, we bind the visualizer to app.virtual_pad.
    // Otherwise, we bind it to a copy of the active system state so it shows real-time inputs.
    PadStateInput active_state = app.getCurrentPadState();
    bool mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    
    PadStateInput temp_state = active_state;
    PadStateInput& render_state = mouse_down ? app.virtual_pad : temp_state;

    renderInteractivePadVisualizer(render_state, pad_size);

    if (!mouse_down && !ImGui::IsAnyItemActive()) {
        // Return virtual sticks to center when not being dragged
        for (int i = 0; i < 4; i++)
            app.virtual_pad.stick_states[i] = 128;
        app.virtual_pad.trigger_l2 = 0;
        app.virtual_pad.trigger_r2 = 0;
        for (int i = 0; i < 22; i++)
            app.virtual_pad.button_states[i] = false;
    }

    // Centered footer tips
    ImGui::Dummy(ImVec2(0, 16));
    float text_w = ImGui::CalcTextSize("Drag sticks/triggers. Click buttons. Leave this tab open for direct touch control.").x;
    ImGui::SetCursorPosX((avail_w - text_w) * 0.5f);
    ImGui::TextColored(p.muted, "%s  Press buttons on physical controller to see them light up in real-time.", ICON_FA_CIRCLE_INFO);
    
    text_w = ImGui::CalcTextSize("Tip: Click & drag on the layout above to send virtual controller inputs.").x;
    ImGui::SetCursorPosX((avail_w - text_w) * 0.5f);
    ImGui::TextColored(p.dim, "Tip: Click & drag on the layout above to send virtual controller inputs.");
}

} // namespace ghostpad
