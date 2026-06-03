// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2024  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "imgui.h"

namespace ghostpad {

void renderSystemStateScreen(App& app) {
    ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f), "SYSTEM STATE");
    ImGui::SameLine();
    ImGui::TextUnformatted("- PS5 Power Control");
    ImGui::Separator();
    ImGui::Spacing();

    if (app.selected_console_ip.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                          "Not connected. Connect to a PS5 first.");
        return;
    }

    ImGui::Text("Target: %s", app.selected_console_ip.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("SSMSection", ImVec2(0, 0), true);

    // Status
    ImGui::TextUnformatted("System Status");
    ImGui::Separator();
    if (ImGui::Button("Get Status", ImVec2(200, 40))) {
        auto r = SsmClient::status(app.selected_console_ip);
        app.addStatus(r.response, !r.ok);
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Power controls
    ImGui::TextUnformatted("Power Controls");
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "WARNING: These actions cannot be undone!");

    ImGui::Spacing();

    if (ImGui::Button("Reboot PS5", ImVec2(200, 45))) {
        ImGui::OpenPopup("ConfirmReboot");
    }
    if (ImGui::Button("Shutdown PS5", ImVec2(200, 45))) {
        ImGui::OpenPopup("ConfirmShutdown");
    }
    if (ImGui::Button("Rest Mode", ImVec2(200, 45))) {
        ImGui::OpenPopup("ConfirmRestMode");
    }
    if (ImGui::Button("Eject Disc", ImVec2(200, 45))) {
        auto r = SsmClient::ejectDisc(app.selected_console_ip);
        app.addStatus(r.response, !r.ok);
    }

    // Confirm popups
    auto confirmPopup = [&](const char* name, const char* action, auto func) {
        if (ImGui::BeginPopupModal(name, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to %s the PS5?", action);
            if (ImGui::Button("Yes", ImVec2(100, 0))) {
                auto r = func(app.selected_console_ip);
                app.addStatus(r.response, !r.ok);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(100, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    };

    confirmPopup("ConfirmReboot", "reboot", [](const std::string& ip) { return SsmClient::reboot(ip); });
    confirmPopup("ConfirmShutdown", "shutdown", [](const std::string& ip) { return SsmClient::shutdown(ip); });
    confirmPopup("ConfirmRestMode", "put into rest mode", [](const std::string& ip) { return SsmClient::restMode(ip); });

    ImGui::Spacing();
    ImGui::Spacing();

    // Deploy SSM ELF
    ImGui::TextUnformatted("Deploy SSM ELF");
    ImGui::Separator();
    static char ssm_elf[1024] = {};
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120);
    ImGui::InputText("##SSMELF", ssm_elf, sizeof(ssm_elf));
    ImGui::SameLine();
    if (ImGui::Button("Deploy", ImVec2(100, 0))) {
        if (strlen(ssm_elf) > 0) {
            auto r = SsmClient::deployElf(app.selected_console_ip, ssm_elf, 9021);
            app.addStatus(r.response, !r.ok);
        }
    }

    ImGui::EndChild();
}

} // namespace ghostpad
