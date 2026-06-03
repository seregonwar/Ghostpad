// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2024  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "imgui.h"

namespace ghostpad {

void renderHomeScreen(App& app) {
    ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f), "GHOSTPAD");
    ImGui::SameLine();
    ImGui::TextUnformatted(" - PS5 Remote Controller (Native C++)");
    ImGui::Separator();
    ImGui::Spacing();

    // Connection status card
    auto status = app.ghostpad.getStatus();
    ImGui::BeginChild("StatusCard", ImVec2(ImGui::GetContentRegionAvail().x * 0.4f, 200), true);
    ImGui::TextUnformatted("Connection Status");
    ImGui::Separator();

    if (status.is_connected) {
        ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f), "Connected");
        ImGui::Text("IP: %s", status.ip.c_str());
        ImGui::Text("Port: %d", status.port);

        if (ImGui::Button("Disconnect", ImVec2(120, 30))) {
            app.ghostpad.disconnect();
            app.deployer.stopKlogWatcher();
            app.addStatus("Disconnected");
        }
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.3f, 0.3f, 1.0f), "Not Connected");
        ImGui::TextUnformatted("Connect to a PS5 in the Consoles panel");
    }

    auto deploy = app.deployer.getStatus();
    if (!deploy.phase.empty() && deploy.phase != "idle") {
        ImGui::Text("Deploy: %s - %s", deploy.phase.c_str(), deploy.message.c_str());
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // Quick actions
    ImGui::BeginChild("ActionsCard", ImVec2(0, 200), true);
    ImGui::TextUnformatted("Quick Actions");
    ImGui::Separator();

    if (ImGui::Button("Manage Consoles", ImVec2(200, 35))) {
        app.current_screen = Screen::Consoles;
    }
    if (ImGui::Button("Settings", ImVec2(200, 35))) {
        app.current_screen = Screen::Settings;
    }
    if (ImGui::Button("Input Redirect", ImVec2(200, 35))) {
        app.current_screen = Screen::InputRedirect;
    }
    if (ImGui::Button("Beeper Control", ImVec2(200, 35))) {
        app.current_screen = Screen::Beeper;
    }
    if (ImGui::Button("System State", ImVec2(200, 35))) {
        app.current_screen = Screen::SystemState;
    }

    ImGui::EndChild();

    ImGui::Spacing();

    // Info cards
    ImGui::BeginChild("InfoSection", ImVec2(0, 0), true);

    ImGui::TextUnformatted("About Ghostpad");
    ImGui::Separator();
    ImGui::TextWrapped(
        "Ghostpad creates a virtual DualSense controller on a jailbroken PS5, "
        "allowing you to control your console over the local network from your PC "
        "using keyboard, mouse, or a physical controller.\n\n"
        "This native C++ version eliminates Electron overhead for maximum performance "
        "and minimal latency."
    );

    ImGui::Spacing();
    ImGui::TextUnformatted("Features");
    ImGui::Separator();

    ImGui::BulletText("Ultra-low latency input streaming (<1ms per packet)");
    ImGui::BulletText("Keyboard-to-gamepad mapping with configurable bindings");
    ImGui::BulletText("Physical controller pass-through (GLFW gamepad API)");
    ImGui::BulletText("Mouse look with adjustable sensitivity");
    ImGui::BulletText("Macro recording and playback at 60fps");
    ImGui::BulletText("Auto-deploy payload via klog auto-bind");
    ImGui::BulletText("Network scanning to find PS5 on LAN");
    ImGui::BulletText("Beeper and LED control");
    ImGui::BulletText("PS5 system state management (reboot, shutdown, etc.)");
    ImGui::BulletText("Console bookmarking and management");

    ImGui::Spacing();
    ImGui::TextUnformatted("Wire Protocol");
    ImGui::Separator();
    if (ImGui::BeginTable("PortsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Port");
        ImGui::TableSetupColumn("Purpose");
        ImGui::TableSetupColumn("Protocol");
        ImGui::TableHeadersRow();

        auto addRow = [](const char* port, const char* purpose, const char* proto) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(port);
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(purpose);
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(proto);
        };

        addRow("6967", "GPAD Virtual Controller", "16-byte binary");
        addRow("6970", "Control (TYPE/DISC/GBND)", "Binary command");
        addRow("9021", "ELF Payload Deployment", "Raw binary");
        addRow("9090", "Alt ELF Loader", "Raw binary");
        addRow("3434", "Klog Debug Monitoring", "Text lines");
        addRow("9111", "Beeper/LED Control", "Text commands");
        addRow("9112", "System State Manager", "Text commands");

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

} // namespace ghostpad
