// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2024  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace ghostpad {

extern void renderPadVisualizer(const PadStateInput& state, float size);

static const char* button_names[] = {
    "Cross", "Circle", "Square", "Triangle",
    "L1", "R1", "L2", "R2",
    "Create", "Options", "L3", "R3",
    "Dpad Up", "Dpad Down", "Dpad Left", "Dpad Right",
    "PS", "Touchpad"
};

static const char* stick_names[] = {"LX", "LY", "RX", "RY"};

static const char* getKeyName(int key) {
    static const struct { int key; const char* name; } names[] = {
        {32, "Space"}, {256, "Esc"}, {257, "Enter"}, {258, "Tab"},
        {259, "Bksp"}, {260, "Ins"}, {261, "Del"}, {262, "Right"},
        {263, "Left"}, {264, "Down"}, {265, "Up"}, {268, "Home"},
        {269, "End"}, {280, "Caps"}, {290, "F1"}, {291, "F2"},
        {292, "F3"}, {293, "F4"}, {294, "F5"}, {295, "F6"},
        {296, "F7"}, {297, "F8"}, {298, "F9"}, {299, "F10"},
        {300, "F11"}, {301, "F12"}, {340, "LShift"}, {344, "RShift"},
        {341, "LCtrl"}, {345, "RCtrl"}, {342, "LAlt"}, {346, "RAlt"},
    };
    for (auto& n : names) {
        if (n.key == key) return n.name;
    }
    if (key >= 65 && key <= 90) {
        static char buf[2] = {};
        buf[0] = (char)key;
        return buf; // Return address of static; fine for immediate use in ImGui
    }
    static char num[8];
    snprintf(num, sizeof(num), "%d", key);
    return num;
}

void renderInputRedirectScreen(App& app) {
    ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f), "INPUT REDIRECTION");
    ImGui::SameLine();
    ImGui::TextUnformatted("- Keyboard, Mouse & Gamepad Mapping");
    ImGui::Separator();

    // Connection status
    auto status = app.ghostpad.getStatus();
    if (!status.is_connected) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                          "Not connected to PS5. Go to Consoles panel to connect.");
        return;
    }

    ImGui::Text("Connected: %s:%d", status.ip.c_str(), status.port);
    ImGui::Separator();
    ImGui::Spacing();

    // Left panel: key bindings
    ImGui::BeginChild("BindingsPanel", ImVec2(ImGui::GetContentRegionAvail().x * 0.45f, 0), true);

    ImGui::TextUnformatted("Button Bindings");
    ImGui::Separator();

    if (ImGui::BeginTable("BindingsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("PS5 Button");
        ImGui::TableSetupColumn("Key");
        ImGui::TableSetupColumn("Rebind");
        ImGui::TableHeadersRow();

        // Rebind popup
        if (app.rebind_button_id >= 0) {
            ImGui::OpenPopup("RebindPopup");
        }

        for (int i = 0; i < 18; i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", button_names[i]);
            ImGui::TableSetColumnIndex(1);

            auto binding = app.keyboard.getButtonBinding(i);
            if (binding.glfw_key != 0) {
                std::string key_str;
                if (binding.ctrl) key_str += "Ctrl+";
                if (binding.shift) key_str += "Shift+";
                if (binding.alt) key_str += "Alt+";
                key_str += getKeyName(binding.glfw_key);
                ImGui::Text("%s", key_str.c_str());
            } else {
                ImGui::TextUnformatted("(unbound)");
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::PushID(i);
            if (ImGui::Button("Rebind", ImVec2(60, 0))) {
                app.rebind_button_id = i;
                app.rebind_button_name = button_names[i];
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (ImGui::BeginPopupModal("RebindPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Press a key for %s...", app.rebind_button_name.c_str());
        ImGui::TextUnformatted("(Press ESC to cancel)");
        ImGui::EndPopup();
    }

    // Stick bindings
    ImGui::Spacing();
    ImGui::TextUnformatted("Stick Bindings");
    ImGui::Separator();

    auto sb = app.keyboard.getStickBindings();
    if (ImGui::BeginTable("StickTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Direction");
        ImGui::TableSetupColumn("Key");
        ImGui::TableHeadersRow();

        struct StickRow { const char* dir; int key; };
        StickRow rows[] = {
            {"LX Right", sb.lx_pos}, {"LX Left", sb.lx_neg},
            {"LY Down", sb.ly_pos}, {"LY Up", sb.ly_neg},
            {"RX Right", sb.rx_pos}, {"RX Left", sb.rx_neg},
            {"RY Down", sb.ry_pos}, {"RY Up", sb.ry_neg},
        };

        for (auto& row : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", row.dir);
            ImGui::TableSetColumnIndex(1);
            if (row.key) ImGui::Text("%s", getKeyName(row.key));
            else ImGui::TextUnformatted("(unbound)");
        }

        ImGui::EndTable();
    }

    // Default bindings
    if (ImGui::Button("Restore Defaults", ImVec2(150, 0))) {
        app.keyboard.loadDefaultBindings();
        app.addStatus("Restored default key bindings");
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // Right panel: options
    ImGui::BeginChild("OptionsPanel", ImVec2(0, 0), true);

    // Mouse look
    ImGui::TextUnformatted("Mouse Look");
    ImGui::Separator();
    static bool mouse_look = false;
    if (ImGui::Checkbox("Enable Mouse Look", &mouse_look)) {
        app.keyboard.setMouseLook(mouse_look);
    }
    static float sensitivity = 3.0f;
    if (ImGui::SliderFloat("Sensitivity", &sensitivity, 0.1f, 50.0f, "%.1f")) {
        app.keyboard.setSensitivity(sensitivity);
    }

    ImGui::Spacing();

    // Auto-clicker
    ImGui::TextUnformatted("Auto Clicker");
    ImGui::Separator();
    static bool auto_clicker = false;
    ImGui::Checkbox("Enable Auto Clicker", &auto_clicker);
    static int ac_button = 0;
    ImGui::Combo("Button", &ac_button, button_names, 18);
    static int ac_hold_ms = 100;
    ImGui::SliderInt("Hold (ms)", &ac_hold_ms, 16, 2000);
    static int ac_gap_ms = 500;
    ImGui::SliderInt("Gap (ms)", &ac_gap_ms, 0, 5000);

    float cps = ac_gap_ms > 0 ? 1000.0f / (ac_hold_ms + ac_gap_ms) : 0.0f;
    ImGui::Text("CPS: %.1f", cps);

    ImGui::Spacing();

    // Gamepad
    ImGui::TextUnformatted("Gamepad");
    ImGui::Separator();
    auto gamepads = app.gamepad_input.listGamepads();
    if (gamepads.empty()) {
        ImGui::TextUnformatted("No gamepads detected");
    } else {
        for (auto& g : gamepads) {
            ImGui::Text("Pad %d: %s", g.index, g.name.c_str());
        }
    }

    if (ImGui::Button("Refresh Gamepads", ImVec2(150, 0))) {
        app.gamepad_input.update();
    }

    ImGui::Spacing();

    // Macro controls
    ImGui::TextUnformatted("Macro Controls");
    ImGui::Separator();
    if (app.macro_engine.isRecording()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "RECORDING");
    }
    if (app.macro_engine.isPlaying()) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "PLAYING");
    }

    ImGui::EndChild();

    // Pad visualizer at the bottom
    ImGui::Spacing();
    ImGui::Separator();

    PadStateInput current_state;
    auto gpads = app.gamepad_input.listGamepads();
    if (!gpads.empty()) {
        current_state = app.gamepad_input.getPadState(0);
    } else {
        current_state = app.keyboard.getPadState();
    }
    if (app.macro_engine.isPlaying()) {
        current_state = app.macro_engine.getPlaybackState();
    }

    renderPadVisualizer(current_state, 180.0f);
}

} // namespace ghostpad
