// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "ui/file_picker.h"
#include "imgui.h"
#include <cstdio>
#include <memory>
#include <array>
#include <thread>
#include <atomic>
#include <cstring>

namespace ghostpad {

void renderSettingsScreen(App& app) {
    const auto& p = ui::colors();
    float avail_w = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    bool compact = app.is_compact_device;

    static std::atomic<bool> is_browsing{false};
    static std::atomic<bool> settings_loaded{false};
    static Screen last_screen = Screen::Home;
    static bool auto_deploy = true, auto_bind = true, beep_on = false;
    static int beep_type = 1;
    static char elf_path[1024] = {};

    if (app.current_screen != last_screen) {
        app.settings.invalidateCache();
        settings_loaded = false;
        last_screen = app.current_screen;
    }

    if (!settings_loaded.load()) {
        auto s = app.settings.read();
        auto_deploy = s.auto_deploy_on_connect;
        auto_bind = s.auto_bind_via_klog;
        beep_on = s.connect_beep_enabled;
        beep_type = s.connect_beep_type;
        strncpy(elf_path, s.payload_elf_path.c_str(), sizeof(elf_path) - 1);
        settings_loaded = true;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //                          PAYLOAD CONFIGURATION CARD
    // ─────────────────────────────────────────────────────────────────────────────
    ui::beginCard("PayloadCard", ImVec2(avail_w, 0));
    ui::sectionLabel("Payload Configuration");

    bool browsing_active = is_browsing.load();
    if (browsing_active) {
        ImGui::BeginDisabled();
    }

    float card_avail_w = ImGui::GetContentRegionAvail().x;

    if (compact) {
        // Mobile: stack vertically
        ImGui::SetNextItemWidth(card_avail_w);
        ImGui::InputTextWithHint("##ELFPATH", "Path to ghostpad.elf", elf_path, sizeof(elf_path));
        ImGui::Spacing();
        
        float btn_w = (card_avail_w - spacing) * 0.5f;
        if (ui::softButton("Browse", ImVec2(btn_w, 36))) {
            if (!is_browsing.load()) {
                is_browsing = true;
                app.addStatus("Opening file picker...");
                std::thread([&app]() {
                    std::string res = ghostpad::ui::pickFile("Select ghostpad.elf", "ELF Files", "*.elf");
                    if (!res.empty()) {
                        auto s = app.settings.read();
                        s.payload_elf_path = res;
                        app.settings.write(s);
                        app.addStatus("Selected payload: " + res);
                        settings_loaded = false;
                    }
                    is_browsing = false;
                }).detach();
            }
        }
        ImGui::SameLine();
        
        if (ui::primaryButton("Deploy Now", ImVec2(btn_w, 36))) {
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
    } else {
        // Desktop: horizontal layout
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
                    std::string res = ghostpad::ui::pickFile("Select ghostpad.elf", "ELF Files", "*.elf");
                    if (!res.empty()) {
                        auto s = app.settings.read();
                        s.payload_elf_path = res;
                        app.settings.write(s);
                        app.addStatus("Selected payload: " + res);
                        settings_loaded = false;
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
    ui::beginCard("OptionsCard", ImVec2(avail_w, 0));
    ui::sectionLabel("Behavior");

    ImGui::Checkbox("Auto-deploy payload on connect", &auto_deploy);
    ImGui::Checkbox("Auto-bind via klog monitoring", &auto_bind);
    ImGui::Checkbox("Beep when virtual controller connects", &beep_on);
    
    if (compact) {
        ImGui::SetNextItemWidth(avail_w - 36);
    } else {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160);
    }
    ImGui::Combo("Beep Type", &beep_type, "Silent\0Single Beep\0Error Pattern\0Long Beep\0");

    ImGui::Spacing();
    ImGui::Spacing();
    if (ui::primaryButton("Save Settings", ImVec2(compact ? (avail_w - 36) : 140, 36))) {
        auto s = app.settings.read();
        s.payload_elf_path = elf_path;
        s.auto_deploy_on_connect = auto_deploy;
        s.auto_bind_via_klog = auto_bind;
        s.connect_beep_enabled = beep_on;
        s.connect_beep_type = beep_type;
        app.settings.write(s);
        app.addStatus("Settings saved");
    }
    ui::endCard();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────────────
    //                            CONTROLLER TYPE CARD
    // ─────────────────────────────────────────────────────────────────────────────
    ui::beginCard("TypeCard", ImVec2(avail_w, 0));
    ui::sectionLabel("Controller Type");
    static int dev_type = 2;
    
    float send_btn_w = compact ? 100.0f : 120.0f;
    float type_avail_w = ImGui::GetContentRegionAvail().x;
    
    if (compact) {
        ImGui::SetNextItemWidth(type_avail_w);
        ImGui::Combo("##Device", &dev_type, "DualShock 4 (0)\0Alt Pad (1)\0DualSense (3)\0");
        ImGui::Spacing();
        if (ui::softButton("Send TYPE", ImVec2(type_avail_w, 36))) {
            if (!app.selected_console_ip.empty()) {
                int t = dev_type == 0 ? 0 : dev_type == 1 ? 1 : 3;
                auto r = GhostpadClient::sendType(app.selected_console_ip, t);
                app.addStatus(r.ok ? "Type set" : r.error, !r.ok);
            } else {
                app.addStatus("Not connected", true);
            }
        }
    } else {
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
    }
    ui::endCard();
}

} // namespace ghostpad
