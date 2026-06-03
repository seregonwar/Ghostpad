// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"

namespace ghostpad {

void renderSettingsScreen(App& app) {
    const auto& p = ui::colors();
    float avail_w = ImGui::GetContentRegionAvail().x;

    // Payload
    ui::beginCard("PayloadCard", ImVec2(avail_w, 160));
    ui::sectionLabel("Payload Configuration");

    static char elf_path[1024] = {};
    if (elf_path[0] == '\0') {
        auto s = app.settings.read();
        if (!s.payload_elf_path.empty())
            strncpy(elf_path, s.payload_elf_path.c_str(), sizeof(elf_path) - 1);
    }

    ImGui::SetNextItemWidth(avail_w - 160);
    ImGui::InputTextWithHint("##ELFPATH", "Path to ghostpad.elf", elf_path, sizeof(elf_path));
    ImGui::SameLine();
    if (ui::softButton("Browse", ImVec2(80, 30)))
        app.addStatus("File picker not available in this build");
    ImGui::SameLine();
    if (ui::primaryButton("Deploy Now", ImVec2(110, 30))) {
        std::string elf = app.settings.resolvePayloadPath();
        if (elf.empty())
            app.addStatus("No payload found", true);
        else if (app.selected_console_ip.empty())
            app.addStatus("Not connected", true);
        else {
            auto s = app.settings.read();
            PayloadDeployer::Options opts;
            opts.elf_path = elf;
            opts.auto_bind_via_klog = s.auto_bind_via_klog;
            opts.status_callback = [&app](const DeployStatus& ds) {
                app.addStatus(ds.message, ds.phase == "error");
            };
            app.deployer.ensurePayloadRunning(app.selected_console_ip, opts);
        }
    }

    std::string resolved = app.settings.resolvePayloadPath();
    if (!resolved.empty())
        ImGui::TextColored(p.success, "Resolved: %s", resolved.c_str());
    else
        ImGui::TextColored(p.warning, "Place ghostpad.elf in Ghostpad/payload/");

    ui::endCard();
    ImGui::Spacing();

    // Options
    ui::beginCard("OptionsCard", ImVec2(avail_w, 220));
    ui::sectionLabel("Behavior");

    static bool auto_deploy = true, auto_bind = true, beep_on = false;
    static int beep_type = 1;
    {
        auto s = app.settings.read();
        auto_deploy = s.auto_deploy_on_connect;
        auto_bind = s.auto_bind_via_klog;
        beep_on = s.connect_beep_enabled;
        beep_type = s.connect_beep_type;
    }

    ImGui::Checkbox("Auto-deploy payload on connect", &auto_deploy);
    ImGui::Checkbox("Auto-bind via klog monitoring", &auto_bind);
    ImGui::Checkbox("Beep when virtual controller connects", &beep_on);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    ImGui::Combo("Beep Type", &beep_type, "Silent\0Single Beep\0Error Pattern\0Long Beep\0");

    ImGui::Spacing();
    if (ui::primaryButton("Save Settings", ImVec2(140, 34))) {
        AppSettings patch;
        patch.payload_elf_path = elf_path;
        patch.auto_deploy_on_connect = auto_deploy;
        patch.auto_bind_via_klog = auto_bind;
        patch.connect_beep_enabled = beep_on;
        patch.connect_beep_type = beep_type;
        app.settings.write(patch);
        app.addStatus("Settings saved");
    }
    ui::endCard();
    ImGui::Spacing();

    // Controller type
    ui::beginCard("TypeCard", ImVec2(avail_w, 90));
    ui::sectionLabel("Controller Type");
    static int dev_type = 2;
    ImGui::Combo("Device", &dev_type, "DualShock 4 (0)\0Alt Pad (1)\0DualSense (3)\0");
    ImGui::SameLine();
    if (ui::softButton("Send TYPE", ImVec2(120, 30))) {
        if (!app.selected_console_ip.empty()) {
            int t = dev_type == 0 ? 0 : dev_type == 1 ? 1 : 3;
            auto r = GhostpadClient::sendType(app.selected_console_ip, t);
            app.addStatus(r.ok ? "Type set" : r.error, !r.ok);
        } else {
            app.addStatus("Not connected", true);
        }
    }
    ui::endCard();
}

} // namespace ghostpad
