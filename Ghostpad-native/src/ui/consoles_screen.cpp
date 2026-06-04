// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"
#include <thread>

namespace ghostpad {

void renderConsolesScreen(App& app) {
    const auto& p = ui::colors();
    float avail_w = ImGui::GetContentRegionAvail().x;

    // Connection bar
    ui::beginCard("ConnectBar", ImVec2(avail_w, 110));
    ui::sectionLabel("Direct Connect", ICON_FA_PLUG);
    ImGui::Spacing();

    static char ip_buf[64] = {};
    static int port_buf = 6967;

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(p.muted, "%s IP:", ICON_FA_DESKTOP);
    ImGui::SameLine();
    ImGui::PushItemWidth(140);
    ImGui::InputTextWithHint("##IP", "0.0.0.0", ip_buf, sizeof(ip_buf));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(p.muted, "Port:");
    ImGui::SameLine();
    ImGui::PushItemWidth(70);
    ImGui::InputInt("##Port", &port_buf, 0, 0);
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(p.muted, "Slot:");
    ImGui::SameLine();
    ImGui::PushItemWidth(50);
    static const char* slotNames[] = { "P1", "P2", "P3", "P4" };
    int currentSlot = app.activeSlot();
    if (ImGui::Combo("##Slot", &currentSlot, slotNames, App::MAX_CONTROLLER_SLOTS)) {
        app.setActiveSlot(currentSlot);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    bool connecting = app.is_connecting_.load();

    if (connecting) {
        ImGui::BeginDisabled();
        ui::softButton(ICON_FA_SPINNER "  Connecting...", ImVec2(130, 32));
        ImGui::EndDisabled();
    } else {
        if (ui::primaryButton(ICON_FA_LINK "  Connect", ImVec2(110, 32))) {
            std::string ip(ip_buf);
            if (!ip.empty()) {
                app.addStatus("Connecting to " + ip + "...");
                app.is_connecting_ = true;
                
                std::thread([&app, ip, port = port_buf]() {
                    auto settings = app.settings.read();
                    std::string elf = app.settings.resolvePayloadPath();

                    if (settings.auto_deploy_on_connect && !elf.empty()) {
                        PayloadDeployer::Options opts;
                        opts.elf_path = elf;
                        opts.auto_bind_via_klog = settings.auto_bind_via_klog;
                        opts.status_callback = [&app](const DeployStatus& s) {
                            app.addStatus(s.message, s.phase == "error");
                        };
                        app.deployer.ensurePayloadRunning(ip, opts);
                    }

                    if (app.ghostpad().connect(ip, port)) {
                        app.selected_console_ip = ip;
                        app.selected_console_port = port;
                        app.addStatus("Connected P" + std::to_string(app.activeSlot() + 1) + " to " + ip);
                        if (settings.connect_beep_enabled && app.deployer.auto_adopted)
                            BeeperClient::buzz(ip, settings.connect_beep_type);
                    } else {
                        app.addStatus("Connection failed", true);
                    }
                    app.is_connecting_ = false;
                }).detach();
            }
        }
    }

    ImGui::SameLine();
    if (ui::dangerButton(ICON_FA_LINK_SLASH "  Disconnect", ImVec2(120, 32))) {
        app.disconnectAllGhostpad();
        app.deployer.stopKlogWatcher();
        app.selected_console_ip.clear();
        app.addStatus("Disconnected all controllers");
    }

    ImGui::SameLine();
    if (ui::dangerButton(ICON_FA_POWER_OFF "  Terminate Payload", ImVec2(150, 32))) {
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


    ImGui::SameLine();
    if (ui::softButton(ICON_FA_WIFI "  Scan Network", ImVec2(140, 32))) {
        app.addStatus("Scanning subnet...");
        std::thread([&app]() {
            auto r = GhostpadClient::scanNetwork();
            app.addStatus(std::to_string(r.size()) + " host(s) found");
        }).detach();
    }

    ui::endCard();

    ImGui::Spacing();

    // Saved consoles
    ui::beginCard("ConsolesList", ImVec2(avail_w, 0));
    /*
     *    [ HEADER ]                   [ + ADD CONSOLE ]
     *    ──────────────────────────────────────────────
     */
    const auto& p_hdr = ui::colors();
    ImGui::PushStyleColor(ImGuiCol_Text, p_hdr.muted);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
    ImGui::Text("%s  Saved Consoles", ICON_FA_DATABASE);
    ImGui::PopStyleColor();

    ImGui::SameLine(ImGui::GetWindowWidth() - 130 - 18);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
    
    if (ui::primaryButton(ICON_FA_PLUS "  Add Console", ImVec2(130, 30))) {
        ImGui::OpenPopup("AddConsolePopup");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto consoles = app.consoles.list();
    if (consoles.empty()) {
        ImGui::TextColored(p.muted, "No saved consoles. Add one or scan the network.");
    }

    for (auto& c : consoles) {
        ImGui::PushID(c.id.c_str());
        float item_h = 58.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, p.bg1);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("ConsoleRow", ImVec2(avail_w - 36, item_h), true,
                          ImGuiWindowFlags_NoScrollbar);

        ImGui::SetCursorPos(ImVec2(16, 14));
        ImGui::TextColored(p.text, "%s  %s", ICON_FA_DESKTOP, c.name.c_str());
        
        ImGui::SameLine(avail_w * 0.26f);
        ImGui::SetCursorPosY(14);
        ImGui::TextColored(p.muted, "%s  %s:%d", ICON_FA_SIGNAL, c.ip.c_str(), c.port);
        
        ImGui::SameLine(avail_w * 0.48f);
        ImGui::TextColored(p.dim, "%s  Loader: %d", ICON_FA_DOWNLOAD, c.elf_loader_port);
        
        ImGui::SameLine(avail_w * 0.66f);
        ImGui::TextColored(p.dim, "%s  %s", ICON_FA_CALENDAR, c.updated_at.substr(0, 10).c_str());

        ImGui::SameLine(avail_w - 240);
        ImGui::SetCursorPosY(13);
        if (connecting) {
            ImGui::BeginDisabled();
            ui::softButton(ICON_FA_LINK "  Connect", ImVec2(100, 30));
            ImGui::EndDisabled();
        } else {
            if (ui::primaryButton(ICON_FA_LINK "  Connect", ImVec2(100, 30))) {
                app.addStatus("Connecting to " + c.name + "...");
                app.is_connecting_ = true;
                
                std::thread([&app, ip = c.ip, port = c.port, name = c.name, elf_loader_port = c.elf_loader_port]() {
                    auto settings = app.settings.read();
                    std::string elf = app.settings.resolvePayloadPath();
                    
                    if (settings.auto_deploy_on_connect && !elf.empty()) {
                        PayloadDeployer::Options opts;
                        opts.elf_path = elf;
                        opts.elf_loader_port = elf_loader_port;
                        opts.auto_bind_via_klog = settings.auto_bind_via_klog;
                        opts.status_callback = [&app](const DeployStatus& s) {
                            app.addStatus(s.message, s.phase == "error");
                        };
                        app.deployer.ensurePayloadRunning(ip, opts);
                    }
                    
                    if (app.ghostpad().connect(ip, port)) {
                        app.selected_console_ip = ip;
                        app.selected_console_port = port;
                        app.addStatus("Connected P" + std::to_string(app.activeSlot() + 1) + " to " + name);
                        if (settings.connect_beep_enabled && app.deployer.auto_adopted)
                            BeeperClient::buzz(ip, settings.connect_beep_type);
                    } else {
                        app.addStatus("Connection failed", true);
                    }
                    app.is_connecting_ = false;
                }).detach();
            }
        }
        ImGui::SameLine();
        if (ui::dangerButton(ICON_FA_TRASH "  Delete", ImVec2(90, 30))) {
            app.consoles.remove(c.id);
            app.addStatus("Console removed");
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopID();
        ImGui::Spacing();
    }

    /*
     *  ┌──────────────────────────────────────────────────────────┐
     *  │                   ADD CONSOLE POPUP                      │
     *  └──────────────────────────────────────────────────────────┘
     */
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("AddConsolePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(p.primary2, "%s  Add New Console", ICON_FA_PLUS);
        ImGui::Separator();
        ImGui::Spacing();

        static char name[64] = {};
        static char ip[64] = {};
        static int port = 6967;
        static int elf_port = 9021;

        ImGui::TextColored(p.muted, "Console Name:");
        ImGui::InputText("##Name", name, sizeof(name));
        
        ImGui::TextColored(p.muted, "IP Address:");
        ImGui::InputText("##IP", ip, sizeof(ip));
        
        ImGui::TextColored(p.muted, "Port:");
        ImGui::InputInt("##Port", &port, 0, 0);
        
        ImGui::TextColored(p.muted, "ELF Port (Loader):");
        ImGui::InputInt("##ElfPort", &elf_port, 0, 0);
        
        ImGui::Spacing();
        ImGui::Spacing();

        if (ui::primaryButton(ICON_FA_CHECK "  Save", ImVec2(110, 32))) {
            if (strlen(ip) > 0) {
                app.consoles.add(name, ip, port, elf_port);
                app.addStatus("Console saved");
                name[0] = ip[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ui::softButton(ICON_FA_XMARK "  Cancel", ImVec2(110, 32))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    
    ui::endCard();
}

} // namespace ghostpad
