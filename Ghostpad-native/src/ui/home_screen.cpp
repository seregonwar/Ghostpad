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
    float col_w = (avail_w - 16.0f) * 0.5f;

    // Left Column: Connection Info
    ui::beginCard("StatusCard", ImVec2(col_w, avail_h - 10.0f));
    ui::sectionLabel("Connection Status", ICON_FA_SIGNAL);
    ImGui::Spacing();
    ImGui::Spacing();

    if (status.is_connected) {
        ImGui::TextColored(p.success, "%s  CONNECTED TO PS5", ICON_FA_CIRCLE_CHECK);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextColored(p.muted, "Console IP Address:");
        ImGui::TextColored(p.text, "  %s", status.ip.c_str());
        ImGui::Spacing();

        ImGui::TextColored(p.muted, "Streaming Port:");
        ImGui::TextColored(p.text, "  %d", status.port);
        ImGui::Spacing();
        ImGui::Spacing();
        
        if (ui::dangerButton(ICON_FA_LINK_SLASH "  Disconnect Console", ImVec2(180, 38))) {
            app.ghostpad().disconnect();
            app.deployer.stopKlogWatcher();
            app.selected_console_ip.clear();
            app.addStatus("Disconnected");
        }
        ImGui::Spacing();
        if (ui::dangerButton(ICON_FA_POWER_OFF "  Terminate Payload", ImVec2(180, 38))) {
            if (!app.selected_console_ip.empty()) {
                std::string ip = app.selected_console_ip;
                app.addStatus("Terminating and unpatching payload...");
                std::thread([&app, ip]() {
                    auto r = GhostpadClient::terminatePayload(ip);
                    if (r.ok) {
                        app.addStatus("Payload terminated and unpatched successfully");
                    } else {
                        app.addStatus("Payload termination sent, checking status...", true);
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
        
        ImGui::TextWrapped("No active console stream found. Link a console in the Consoles panel to begin low-latency controller streaming.");
        ImGui::Spacing();
        ImGui::Spacing();
        
        if (ui::primaryButton(ICON_FA_LINK "  Configure Connections", ImVec2(200, 38)))
            app.current_screen = Screen::Consoles;
    }

    auto ds = app.deployer.getStatus();
    if (!ds.phase.empty() && ds.phase != "idle") {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(p.warning, "%s  Payload Deployer State:", ICON_FA_DOWNLOAD);
        ImGui::TextColored(p.muted, "  Phase:   %s", ds.phase.c_str());
        ImGui::TextColored(p.muted, "  Message: %s", ds.message.c_str());
    }
    ui::endCard();

    ImGui::SameLine(0, 16);

    // Right Column: Launcher / Quick Actions
    ui::beginCard("ActionsCard", ImVec2(col_w, avail_h - 10.0f));
    ui::sectionLabel("Quick Actions / Launcher", ICON_FA_BOLT);
    ImGui::Spacing();
    ImGui::Spacing();

    if (ui::softButton(ICON_FA_DESKTOP "  Manage Consoles", ImVec2(col_w - 36, 44)))
        app.current_screen = Screen::Consoles;
    ImGui::Spacing();
    
    if (ui::softButton(ICON_FA_GAMEPAD "  Virtual Controller Panel", ImVec2(col_w - 36, 44)))
        app.current_screen = Screen::Controller;
    ImGui::Spacing();
    
    if (ui::softButton(ICON_FA_KEYBOARD "  Input Redirection Map", ImVec2(col_w - 36, 44)))
        app.current_screen = Screen::InputRedirect;
    ImGui::Spacing();
    
    if (ui::softButton(ICON_FA_VOLUME_HIGH "  Beeper & LED Diagnostics", ImVec2(col_w - 36, 44)))
        app.current_screen = Screen::Beeper;
    ImGui::Spacing();
    
    if (ui::softButton(ICON_FA_MICROCHIP "  System Power State Controls", ImVec2(col_w - 36, 44)))
        app.current_screen = Screen::SystemState;
    ImGui::Spacing();

    if (ui::softButton(ICON_FA_FOLDER_OPEN "  Macro Recording Projects", ImVec2(col_w - 36, 44)))
        app.current_screen = Screen::Projects;

    ui::endCard();
}

} // namespace ghostpad
