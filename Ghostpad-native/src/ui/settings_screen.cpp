// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"
#include <cstdio>
#include <memory>
#include <array>
#include <thread>
#include <atomic>
#include <cstring>

namespace ghostpad {

/*
 *  ┌──────────────────────────────────────────────────────────┐
 *  │             SHELL EXECUTION HELPER (macOS)               │
 *  └──────────────────────────────────────────────────────────┘
 */
static std::string execCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

void renderSettingsScreen(App& app) {
    const auto& p = ui::colors();
    float avail_w = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;

    static std::atomic<bool> is_browsing{false};

    // ─────────────────────────────────────────────────────────────────────────────
    //                          PAYLOAD CONFIGURATION CARD
    // ─────────────────────────────────────────────────────────────────────────────
    ui::beginCard("PayloadCard", ImVec2(avail_w, 120), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ui::sectionLabel("Payload Configuration");

    static char elf_path[1024] = {};
    if (elf_path[0] == '\0') {
        auto s = app.settings.read();
        if (!s.payload_elf_path.empty())
            strncpy(elf_path, s.payload_elf_path.c_str(), sizeof(elf_path) - 1);
    }

    bool browsing_active = is_browsing.load();
    if (browsing_active) {
        ImGui::BeginDisabled();
    }

    float card_avail_w = ImGui::GetContentRegionAvail().x;
    float browse_btn_w = 80.0f;
    float deploy_btn_w = 110.0f;
    float input_w = card_avail_w - browse_btn_w - deploy_btn_w - spacing * 2.0f;

    ImGui::SetNextItemWidth(input_w);
    ImGui::InputTextWithHint("##ELFPATH", "Path to ghostpad.elf", elf_path, sizeof(elf_path));
    ImGui::SameLine();
    
    if (ui::softButton("Browse", ImVec2(browse_btn_w, 30))) {
        if (!is_browsing.load()) {
            is_browsing = true;
            app.addStatus("Opening file picker...");
            std::thread([&app]() {
                std::string res = execCommand("osascript -e 'POSIX path of (choose file with prompt \"Select ghostpad.elf\")' 2>/dev/null");
                if (!res.empty()) {
                    auto s = app.settings.read();
                    s.payload_elf_path = res;
                    app.settings.write(s);
                    app.addStatus("Selected payload: " + res);
                    // Clear the buffer to force reload on the next frame
                    elf_path[0] = '\0';
                }
                is_browsing = false;
            }).detach();
        }
    }
    ImGui::SameLine();
    
    if (ui::primaryButton("Deploy Now", ImVec2(deploy_btn_w, 30))) {
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

    if (browsing_active) {
        ImGui::EndDisabled();
    }

    std::string resolved = app.settings.resolvePayloadPath();
    if (!resolved.empty())
        ImGui::TextColored(p.success, "Resolved: %s", resolved.c_str());
    else
        ImGui::TextColored(p.warning, "Place ghostpad.elf in Ghostpad/payload/");

    ui::endCard();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────────────
    //                              BEHAVIOR OPTIONS CARD
    // ─────────────────────────────────────────────────────────────────────────────
    ui::beginCard("OptionsCard", ImVec2(avail_w, 230), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
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

    // ─────────────────────────────────────────────────────────────────────────────
    //                            CONTROLLER TYPE CARD
    // ─────────────────────────────────────────────────────────────────────────────
    ui::beginCard("TypeCard", ImVec2(avail_w, 100), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ui::sectionLabel("Controller Type");
    static int dev_type = 2;
    
    float send_btn_w = 120.0f;
    float type_avail_w = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(type_avail_w - send_btn_w - spacing);
    ImGui::Combo("##Device", &dev_type, "DualShock 4 (0)\0Alt Pad (1)\0DualSense (3)\0");
    ImGui::SameLine();
    if (ui::softButton("Send TYPE", ImVec2(send_btn_w, 30))) {
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
