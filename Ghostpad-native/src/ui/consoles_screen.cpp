// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2024  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <thread>

namespace ghostpad {

void renderConsolesScreen(App& app) {
    ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f), "CONSOLES");
    ImGui::SameLine();
    ImGui::TextUnformatted("- Manage PS5 Consoles");
    ImGui::Separator();
    ImGui::Spacing();

    // Connection bar
    static char ip_buf[64] = {};
    static int port_buf = 6967;

    ImGui::TextUnformatted("Quick Connect");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::InputText("##IP", ip_buf, sizeof(ip_buf));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::InputInt("Port", &port_buf);
    ImGui::SameLine();

    if (ImGui::Button("Connect", ImVec2(100, 0))) {
        std::string ip(ip_buf);
        if (!ip.empty()) {
            app.addStatus("Connecting to " + ip + "...");
            auto settings = app.settings.read();
            std::string elf_path = app.settings.resolvePayloadPath();

            // Deploy payload
            if (settings.auto_deploy_on_connect && !elf_path.empty()) {
                PayloadDeployer::Options opts;
                opts.elf_path = elf_path;
                opts.force_deploy = false;
                opts.auto_bind_via_klog = settings.auto_bind_via_klog;
                opts.status_callback = [&app](const DeployStatus& s) {
                    app.addStatus(s.phase + ": " + s.message, s.phase == "error");
                };

                auto result = app.deployer.ensurePayloadRunning(ip, opts);
                if (!result.ok) {
                    app.addStatus("Deploy failed: " + result.message, true);
                }
            }

            if (app.ghostpad.connect(ip, port_buf)) {
                app.selected_console_ip = ip;
                app.selected_console_port = port_buf;
                app.addStatus("Connected to " + ip);

                // Connect beep
                if (settings.connect_beep_enabled && app.deployer.auto_adopted) {
                    BeeperClient::buzz(ip, settings.connect_beep_type);
                }
            } else {
                app.addStatus("Failed to connect to " + ip, true);
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Disconnect", ImVec2(100, 0))) {
        app.ghostpad.disconnect();
        app.deployer.stopKlogWatcher();
        app.selected_console_ip.clear();
        app.addStatus("Disconnected");
    }

    ImGui::SameLine();
    if (ImGui::Button("Scan Network", ImVec2(140, 0))) {
        app.addStatus("Scanning network (this may take a while)...");
        std::thread([&app]() {
            auto results = GhostpadClient::scanNetwork();
            char buf[128];
            snprintf(buf, sizeof(buf), "Scan complete: %zu PS5(s) found", results.size());
            app.addStatus(buf);
        }).detach();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Saved consoles list
    ImGui::TextUnformatted("Saved Consoles");
    ImGui::SameLine();
    if (ImGui::Button("Add Console", ImVec2(120, 0))) {
        ImGui::OpenPopup("AddConsolePopup");
    }

    ImGui::Spacing();

    auto consoles = app.consoles.list();
    if (consoles.empty()) {
        ImGui::TextUnformatted("No consoles saved. Add one or scan the network.");
    }

    for (auto& c : consoles) {
        ImGui::PushID(c.id.c_str());
        ImGui::BeginChild("ConsoleEntry", ImVec2(ImGui::GetContentRegionAvail().x, 60), true,
                          ImGuiWindowFlags_NoScrollbar);

        ImGui::Text("%s", c.name.c_str());
        ImGui::SameLine(200);
        ImGui::Text("IP: %s:%d", c.ip.c_str(), c.port);
        ImGui::SameLine(400);
        ImGui::Text("ELF: %d", c.elf_loader_port);

        ImGui::SameLine(550);
        if (ImGui::Button("Connect", ImVec2(80, 0))) {
            auto settings = app.settings.read();
            std::string elf_path = app.settings.resolvePayloadPath();

            if (settings.auto_deploy_on_connect && !elf_path.empty()) {
                PayloadDeployer::Options opts;
                opts.elf_path = elf_path;
                opts.elf_loader_port = c.elf_loader_port;
                opts.auto_bind_via_klog = settings.auto_bind_via_klog;
                opts.status_callback = [&app](const DeployStatus& s) {
                    app.addStatus(s.phase + ": " + s.message, s.phase == "error");
                };
                app.deployer.ensurePayloadRunning(c.ip, opts);
            }

            if (app.ghostpad.connect(c.ip, c.port)) {
                app.selected_console_ip = c.ip;
                app.selected_console_port = c.port;
                app.addStatus("Connected to " + c.name);
            } else {
                app.addStatus("Failed to connect", true);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(60, 0))) {
            ImGui::OpenPopup("DeleteConfirm");
        }

        if (ImGui::BeginPopupModal("DeleteConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete console '%s'?", c.name.c_str());
            if (ImGui::Button("Yes", ImVec2(80, 0))) {
                app.consoles.remove(c.id);
                app.addStatus("Console deleted");
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(80, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndChild();
        ImGui::PopID();
    }

    // Add console popup
    if (ImGui::BeginPopupModal("AddConsolePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char name_buf[64] = {};
        static char add_ip_buf[64] = {};
        static int add_port = 6967;
        static int add_elf_port = 9021;

        ImGui::InputText("Name", name_buf, sizeof(name_buf));
        ImGui::InputText("IP Address", add_ip_buf, sizeof(add_ip_buf));
        ImGui::InputInt("Port", &add_port);
        ImGui::InputInt("ELF Loader Port", &add_elf_port);

        if (ImGui::Button("Save", ImVec2(100, 0))) {
            std::string name(name_buf);
            std::string ip(add_ip_buf);
            if (!ip.empty()) {
                app.consoles.add(name, ip, add_port, add_elf_port);
                app.addStatus("Console added: " + ip);
            }
            name_buf[0] = '\0';
            add_ip_buf[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

} // namespace ghostpad
