// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2024  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "imgui.h"

namespace ghostpad {

void renderSettingsScreen(App& app) {
    ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f), "SETTINGS");
    ImGui::SameLine();
    ImGui::TextUnformatted("- Ghostpad Configuration");
    ImGui::Separator();
    ImGui::Spacing();

    auto current = app.settings.read();

    ImGui::BeginChild("SettingsPanel", ImVec2(0, 0), true);

    // Payload ELF path
    ImGui::TextUnformatted("Payload Configuration");
    ImGui::Separator();

    static char elf_path_buf[1024] = {};
    if (elf_path_buf[0] == '\0' && !current.payload_elf_path.empty()) {
        strncpy(elf_path_buf, current.payload_elf_path.c_str(), sizeof(elf_path_buf) - 1);
    }

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120);
    ImGui::InputText("ELF Path", elf_path_buf, sizeof(elf_path_buf));
    ImGui::SameLine();
    if (ImGui::Button("Browse", ImVec2(100, 0))) {
        // Note: File dialog not implemented in this version
        // Use TinyFileDialog or platform-native dialog
        app.addStatus("File dialog not available in this build");
    }

    std::string resolved = app.settings.resolvePayloadPath();
    if (!resolved.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 0.5f, 1.0f), "Resolved: %s", resolved.c_str());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "No payload found. Download ghostpad.elf to Ghostpad/payload/");
    }

    ImGui::Spacing();

    // Auto-deploy
    ImGui::TextUnformatted("Deployment");
    ImGui::Separator();

    static bool auto_deploy = current.auto_deploy_on_connect;
    ImGui::Checkbox("Auto-deploy payload on connect", &auto_deploy);

    static bool auto_bind = current.auto_bind_via_klog;
    ImGui::Checkbox("Auto-bind via klog monitoring", &auto_bind);

    ImGui::TextUnformatted("(Recommended: leave both enabled)");

    ImGui::Spacing();

    // Connect beep
    ImGui::TextUnformatted("Connect Beep");
    ImGui::Separator();

    static bool beep_enabled = current.connect_beep_enabled;
    ImGui::Checkbox("Beep when virtual controller connects", &beep_enabled);

    static int beep_type = current.connect_beep_type;
    const char* beep_types[] = {"Silent (0)", "Single Beep (1)", "Error Pattern (2)", "Long Beep (3)"};
    ImGui::Combo("Beep Type", &beep_type, beep_types, 4);
    beep_type = beep_type; // 0-3

    ImGui::Spacing();

    // Controller type
    ImGui::TextUnformatted("Controller Type");
    ImGui::Separator();

    static bool auto_deploy_2 = current.auto_deploy_on_connect; // placeholder for device type
    const char* dev_types[] = {"DualShock 4 (0)", "Alternate Pad (1)", "DualSense/PS5 (3)"};
    static int dev_type = 2; // default DualSense
    ImGui::Combo("Device Type", &dev_type, dev_types, 3);

    if (ImGui::Button("Send TYPE Command", ImVec2(180, 0))) {
        if (!app.selected_console_ip.empty()) {
            int actual_type = (dev_type == 0) ? 0 : (dev_type == 1) ? 1 : 3;
            auto result = GhostpadClient::sendType(app.selected_console_ip, actual_type);
            app.addStatus(result.ok ? "Type set" : ("Failed: " + result.error));
        } else {
            app.addStatus("Not connected to any console", true);
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Save button
    if (ImGui::Button("Save Settings", ImVec2(150, 35))) {
        AppSettings patch;
        patch.payload_elf_path = elf_path_buf;
        patch.auto_deploy_on_connect = auto_deploy;
        patch.auto_bind_via_klog = auto_bind;
        patch.connect_beep_enabled = beep_enabled;
        patch.connect_beep_type = beep_type;
        app.settings.write(patch);
        app.addStatus("Settings saved");
    }

    ImGui::SameLine();
    if (ImGui::Button("Deploy Payload Now", ImVec2(180, 35))) {
        std::string elf = app.settings.resolvePayloadPath();
        if (elf.empty()) {
            app.addStatus("No payload found", true);
        } else if (app.selected_console_ip.empty()) {
            app.addStatus("Not connected to any console", true);
        } else {
            PayloadDeployer::Options opts;
            opts.elf_path = elf;
            opts.auto_bind_via_klog = auto_bind;
            opts.status_callback = [&app](const DeployStatus& s) {
                app.addStatus(s.phase + ": " + s.message, s.phase == "error");
            };
            auto result = app.deployer.ensurePayloadRunning(app.selected_console_ip, opts);
            app.addStatus(result.ok ? "Payload deployed" : ("Deploy failed: " + result.message),
                         !result.ok);
        }
    }

    ImGui::EndChild();
}

} // namespace ghostpad
