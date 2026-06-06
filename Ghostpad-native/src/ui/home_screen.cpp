// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"
#include <thread>


namespace ghostpad {

void renderHomeScreen(App& app) {
    const auto& p = ui::colors();
    auto status = app.ghostpad().getStatus();

    float avail_w = ImGui::GetContentRegionAvail().x;
    float avail_h = ImGui::GetContentRegionAvail().y;

    bool compact = app.is_compact_device;

    if (compact) {
        // Compact: stack cards vertically, each auto-sized
        ui::beginCard("StatusCard", ImVec2(avail_w, 0));
        ui::sectionLabel("Connection Status", ICON_FA_SIGNAL);
        ImGui::Spacing();

        if (status.is_connected) {
            ImGui::TextColored(p.success, "%s  CONNECTED TO PS5", ICON_FA_CIRCLE_CHECK);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::TextColored(p.muted, "Console IP:");
            ImGui::SameLine();
            ImGui::TextColored(p.text, "%s:%d", status.ip.c_str(), status.port);
            ImGui::Spacing();
            
            float btn_w = avail_w - 36.0f;
            if (ui::dangerButton(ICON_FA_LINK_SLASH "  Disconnect", ImVec2(btn_w, 42))) {
                app.ghostpad().disconnect();
                app.deployer.stopKlogWatcher();
                app.selected_console_ip.clear();
                app.addStatus("Disconnected");
            }
            ImGui::Spacing();
            if (ui::dangerButton(ICON_FA_POWER_OFF "  Terminate Payload", ImVec2(btn_w, 42))) {
                if (!app.selected_console_ip.empty()) {
                    std::string ip = app.selected_console_ip;
                    app.addStatus("Terminating payload...");
                    std::thread([&app, ip]() {
                        auto r = GhostpadClient::terminatePayload(ip);
                        if (r.ok) {
                            app.addStatus("Payload terminated");
                        } else {
                            app.addStatus("Payload termination sent", true);
                        }
                        app.ghostpad().disconnect();
                        app.deployer.stopKlogWatcher();
                        app.selected_console_ip.clear();
                    }).detach();
                }
            }
        } else {
            ImGui::TextColored(p.danger, "%s  NOT CONNECTED", ICON_FA_CIRCLE_XMARK);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::TextWrapped("No active console stream. Link a console to begin low-latency controller streaming.");
            ImGui::Spacing();
            
            float btn_w = avail_w - 36.0f;
            if (ui::primaryButton(ICON_FA_LINK "  Configure Connections", ImVec2(btn_w, 42)))
                app.current_screen = Screen::Consoles;
        }

        auto ds = app.deployer.getStatus();
        if (!ds.phase.empty() && ds.phase != "idle") {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(p.warning, "%s  Deployer: %s", ICON_FA_DOWNLOAD, ds.message.c_str());
        }
        ui::endCard();

        ImGui::Spacing();

        ui::beginCard("ActionsCard", ImVec2(avail_w, 0));
        ui::sectionLabel("Quick Actions", ICON_FA_BOLT);
        ImGui::Spacing();

        float btn_w = avail_w - 36.0f;
        float btn_h = 42.0f;

        if (ui::softButton(ICON_FA_DESKTOP "  Consoles", ImVec2(btn_w, btn_h)))
            app.current_screen = Screen::Consoles;
        ImGui::Spacing();
        
        if (ui::softButton(ICON_FA_GAMEPAD "  Controller", ImVec2(btn_w, btn_h)))
            app.current_screen = Screen::Controller;
        ImGui::Spacing();
        
        if (ui::softButton(ICON_FA_VOLUME_HIGH "  Beeper & LED", ImVec2(btn_w, btn_h)))
            app.current_screen = Screen::Beeper;
        ImGui::Spacing();
        
        if (ui::softButton(ICON_FA_MICROCHIP "  System State", ImVec2(btn_w, btn_h)))
            app.current_screen = Screen::SystemState;
        ImGui::Spacing();

        if (ui::softButton(ICON_FA_FOLDER_OPEN "  Projects", ImVec2(btn_w, btn_h)))
            app.current_screen = Screen::Projects;

        ui::endCard();
    } else {
        // Desktop/iPad: two-column layout
        float col_w = (avail_w - 16.0f) * 0.5f;

        ui::beginCard("StatusCard", ImVec2(col_w, avail_h - 10.0f));
        ui::sectionLabel("Connection Status", ICON_FA_SIGNAL);
        ImGui::Spacing();

        if (status.is_connected) {
            ImGui::TextColored(p.success, "%s  CONNECTED TO PS5", ICON_FA_CIRCLE_CHECK);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::TextColored(p.muted, "Console IP:");
            ImGui::SameLine();
            ImGui::TextColored(p.text, "%s:%d", status.ip.c_str(), status.port);
            ImGui::Spacing();
            
            float btn_w = 180.0f;
            if (ui::dangerButton(ICON_FA_LINK_SLASH "  Disconnect", ImVec2(btn_w, 38))) {
                app.ghostpad().disconnect();
                app.deployer.stopKlogWatcher();
                app.selected_console_ip.clear();
                app.addStatus("Disconnected");
            }
            ImGui::Spacing();
            if (ui::dangerButton(ICON_FA_POWER_OFF "  Terminate Payload", ImVec2(btn_w, 38))) {
                if (!app.selected_console_ip.empty()) {
                    std::string ip = app.selected_console_ip;
                    app.addStatus("Terminating payload...");
                    std::thread([&app, ip]() {
                        auto r = GhostpadClient::terminatePayload(ip);
                        if (r.ok) {
                            app.addStatus("Payload terminated");
                        } else {
                            app.addStatus("Payload termination sent", true);
                        }
                        app.ghostpad().disconnect();
                        app.deployer.stopKlogWatcher();
                        app.selected_console_ip.clear();
                    }).detach();
                }
            }
        } else {
            ImGui::TextColored(p.danger, "%s  NOT CONNECTED", ICON_FA_CIRCLE_XMARK);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::TextWrapped("No active console stream. Link a console to begin low-latency controller streaming.");
            ImGui::Spacing();
            
            float btn_w = 200.0f;
            if (ui::primaryButton(ICON_FA_LINK "  Configure Connections", ImVec2(btn_w, 38)))
                app.current_screen = Screen::Consoles;
        }

        auto ds = app.deployer.getStatus();
        if (!ds.phase.empty() && ds.phase != "idle") {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(p.warning, "%s  Deployer: %s", ICON_FA_DOWNLOAD, ds.message.c_str());
        }
        ui::endCard();

        ImGui::SameLine(0, 16);

        ui::beginCard("ActionsCard", ImVec2(col_w, 0));
        ui::sectionLabel("Quick Actions", ICON_FA_BOLT);
        ImGui::Spacing();

        float btn_w = col_w - 36.0f;
        float btn_h = 44.0f;

        if (ui::softButton(ICON_FA_DESKTOP "  Consoles", ImVec2(btn_w, btn_h)))
            app.current_screen = Screen::Consoles;
        ImGui::Spacing();
        
        if (ui::softButton(ICON_FA_GAMEPAD "  Controller", ImVec2(btn_w, btn_h)))
            app.current_screen = Screen::Controller;
        ImGui::Spacing();
        
        if (ui::softButton(ICON_FA_KEYBOARD "  Input Redirect", ImVec2(btn_w, btn_h)))
            app.current_screen = Screen::InputRedirect;
        ImGui::Spacing();
        
        if (ui::softButton(ICON_FA_VOLUME_HIGH "  Beeper & LED", ImVec2(btn_w, btn_h)))
            app.current_screen = Screen::Beeper;
        ImGui::Spacing();
        
        if (ui::softButton(ICON_FA_MICROCHIP "  System State", ImVec2(btn_w, btn_h)))
            app.current_screen = Screen::SystemState;
        ImGui::Spacing();

        if (ui::softButton(ICON_FA_FOLDER_OPEN "  Projects", ImVec2(btn_w, btn_h)))
            app.current_screen = Screen::Projects;

        ui::endCard();
    }
}

} // namespace ghostpad
