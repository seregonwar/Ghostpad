// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2024  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "imgui.h"

namespace ghostpad {

extern void renderInteractivePadVisualizer(PadStateInput& state, float size);

void renderControllerScreen(App& app) {
    auto status = app.ghostpad.getStatus();

    ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f), "VIRTUAL CONTROLLER");
    ImGui::SameLine();
    ImGui::TextUnformatted("- Click, drag & play");

    if (!status.is_connected) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "(Not connected)");
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Use a static mutable pad state for interactive input
    static PadStateInput virtual_pad;

    // If connected, sync the interactive pad state with what's being sent
    // (keyboard/gamepad input is merged in the update loop)
    // The interactive pad overrides any other input when used
    static bool pad_active = false;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float pad_size = std::min(avail.x * 0.45f, avail.y * 0.65f);

    // Check if user is interacting with the pad
    bool interacted = ImGui::IsMouseDown(ImGuiMouseButton_Left);

    renderInteractivePadVisualizer(virtual_pad, pad_size);

    // Reset virtual pad when not interacting so keyboard/gamepad take over
    if (!interacted) {
        // Don't force reset - just send whatever state is active
    }

    // Send the interactive state immediately if we're connected
    if (status.is_connected && interacted) {
        auto gpad = buildGpadState(virtual_pad);
        app.ghostpad.sendPadState(gpad);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Connection info and reset button
    ImGui::Text("Press buttons on the controller to control your PS5 directly.");
    ImGui::TextUnformatted("Drag sticks for analog input, click L3/R3 for stick clicks.");

    ImGui::Spacing();
    if (ImGui::Button("Reset Controller", ImVec2(160, 30))) {
        virtual_pad = {};
    }

    ImGui::SameLine();
    if (!status.is_connected) {
        if (ImGui::Button("Go to Consoles", ImVec2(160, 30))) {
            app.current_screen = Screen::Consoles;
        }
    } else {
        ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f),
                          "Connected to %s:%d", status.ip.c_str(), status.port);
    }
}

} // namespace ghostpad
