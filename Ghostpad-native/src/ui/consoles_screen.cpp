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
    bool compact = app.is_compact_device;

    float bar_h = compact ? 160.0f : 110.0f;
    ui::beginCard("ConnectBar", ImVec2(avail_w, bar_h));
    ui::sectionLabel("Direct Connect", ICON_FA_PLUG);
    ImGui::Spacing();

    static char ip_buf[64] = {};
    static int port_buf = 6967;

    if (compact) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(p.muted, "%s IP:", ICON_FA_DESKTOP);
        ImGui::SameLine();
        ImGui::PushItemWidth(avail_w * 0.35f);
        ImGui::InputTextWithHint("##IP", "0.0.0.0", ip_buf, sizeof(ip_buf));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(p.muted, "Port:");
        ImGui::SameLine();
        ImGui::PushItemWidth(60);
        ImGui::InputInt("##Port", &port_buf, 0, 0);
        ImGui::PopItemWidth();
        ImGui::Spacing();

        bool connecting = app.is_connecting_.load();
        
        // Mobile: stack buttons vertically for better touch targets
        float btn_w = avail_w - 36.0f;
        
        if (connecting) {
            ImGui::BeginDisabled();
            ui::softButton(ICON_FA_SPINNER "  Connecting...", ImVec2(btn_w, 36));
            ImGui::EndDisabled();
        } else {
            if (ui::primaryButton(ICON_FA_LINK "  Connect", ImVec2(btn_w, 36))) {
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
        ImGui::Spacing();
        if (ui::dangerButton(ICON_FA_LINK_SLASH "  Disconnect", ImVec2(btn_w, 36))) {
            app.disconnectAllGhostpad();
            app.deployer.stopKlogWatcher();
            app.selected_console_ip.clear();
            app.addStatus("Disconnected all controllers");
        }
        ImGui::Spacing();
        if (ui::softButton(ICON_FA_WIFI "  Scan Network", ImVec2(btn_w, 36))) {
            app.addStatus("Scanning subnet...");
            std::thread([&app]() {
                auto r = GhostpadClient::scanNetwork();
                app.addStatus(std::to_string(r.size()) + " host(s) found");
            }).detach();
        }
    } else {
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
        if (ui::dangerButton(ICON_FA_POWER_OFF "  Terminate", ImVec2(120, 32))) {
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

        ImGui::SameLine();
        if (ui::softButton(ICON_FA_WIFI "  Scan Network", ImVec2(140, 32))) {
            app.addStatus("Scanning subnet...");
            std::thread([&app]() {
                auto r = GhostpadClient::scanNetwork();
                app.addStatus(std::to_string(r.size()) + " host(s) found");
            }).detach();
        }
    }

    ui::endCard();

    ImGui::Spacing();

    ui::beginCard("ConsolesList", ImVec2(avail_w, 0));
    const auto& p_hdr = ui::colors();
    ImGui::PushStyleColor(ImGuiCol_Text, p_hdr.muted);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
    ImGui::Text("%s  Saved Consoles", ICON_FA_DATABASE);
    ImGui::PopStyleColor();

    if (compact) {
        // Mobile: button below title, full width
        ImGui::Spacing();
        if (ui::primaryButton(ICON_FA_PLUS "  Add Console", ImVec2(avail_w - 36.0f, 36))) {
            ImGui::OpenPopup("AddConsolePopup");
        }
    } else {
        // Desktop: button on the right
        ImGui::SameLine(ImGui::GetWindowWidth() - 130 - 18);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
        
        if (ui::primaryButton(ICON_FA_PLUS "  Add Console", ImVec2(130, 30))) {
            ImGui::OpenPopup("AddConsolePopup");
        }
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
        float item_h = compact ? 60.0f : 58.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, p.bg1);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("ConsoleRow", ImVec2(avail_w - 36, item_h), true,
                          ImGuiWindowFlags_NoScrollbar);

        float child_w = ImGui::GetContentRegionAvail().x;

        if (compact) {
            // Compact: stack info vertically
            ImGui::SetCursorPos(ImVec2(12, 8));
            ImGui::TextColored(p.text, "%s  %s", ICON_FA_DESKTOP, c.name.c_str());
            
            ImGui::SetCursorPos(ImVec2(12, 28));
            ImGui::TextColored(p.muted, "%s:%d", c.ip.c_str(), c.port);

            // Buttons on the right
            float btn_x = child_w - 180.0f;
            if (btn_x < child_w * 0.5f) btn_x = child_w * 0.5f;
            ImGui::SetCursorPos(ImVec2(btn_x, 15));
            
            bool connecting = app.is_connecting_.load();
            if (connecting) {
                ImGui::BeginDisabled();
                ui::softButton(ICON_FA_LINK "  Connect", ImVec2(90, 32));
                ImGui::EndDisabled();
            } else {
                if (ui::primaryButton(ICON_FA_LINK "  Connect", ImVec2(90, 32))) {
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
            if (ui::dangerButton(ICON_FA_TRASH "  Del", ImVec2(70, 32))) {
                app.consoles.remove(c.id);
                app.addStatus("Console removed");
            }
        } else {
            // Desktop/iPad: horizontal layout
            float actions_x = child_w - 200.0f;
            if (actions_x < child_w * 0.5f) actions_x = child_w * 0.5f;

            float date_x = child_w - 320.0f;
            if (date_x < child_w * 0.35f) date_x = child_w * 0.35f;

            float loader_x = child_w - 460.0f;
            if (loader_x < child_w * 0.22f) loader_x = child_w * 0.22f;

            float connection_x = child_w - 600.0f;
            if (connection_x < child_w * 0.12f) connection_x = child_w * 0.12f;

            ImGui::SetCursorPos(ImVec2(12, 12));
            ImGui::TextColored(p.text, "%s  %s", ICON_FA_DESKTOP, c.name.c_str());
            
            ImGui::SetCursorPos(ImVec2(connection_x, 14));
            ImGui::TextColored(p.muted, "%s  %s:%d", ICON_FA_SIGNAL, c.ip.c_str(), c.port);
            
            ImGui::SetCursorPos(ImVec2(loader_x, 14));
            ImGui::TextColored(p.dim, "%s  Loader: %d", ICON_FA_DOWNLOAD, c.elf_loader_port);
            
            ImGui::SetCursorPos(ImVec2(date_x, 14));
            ImGui::TextColored(p.dim, "%s  %s", ICON_FA_CALENDAR, c.updated_at.substr(0, 10).c_str());

            ImGui::SetCursorPos(ImVec2(actions_x, 13));
            bool connecting = app.is_connecting_.load();
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
            if (ui::dangerButton(ICON_FA_TRASH "  Del", ImVec2(70, 30))) {
                app.consoles.remove(c.id);
                app.addStatus("Console removed");
            }
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
