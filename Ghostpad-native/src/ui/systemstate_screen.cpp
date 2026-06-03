// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"

namespace ghostpad {

static void confirmSSM(App& app, const char* icon, const char* label, const char* action,
                       std::function<SsmResult(const std::string&)> fn) {
    const auto& p = ui::colors();
    
    // Create button label with icon
    std::string btn_label = std::string(icon) + "  " + label;
    
    if (ui::dangerButton(btn_label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 36, 44)))
        ImGui::OpenPopup(action);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal(action, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(p.warning, "%s  System Control Warning", ICON_FA_TRIANGLE_EXCLAMATION);
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text("Are you sure you want to: %s?", label);
        ImGui::TextColored(p.muted, "This will immediately interrupt the PS5 console state.");
        ImGui::Spacing();
        ImGui::Spacing();

        if (ui::dangerButton(ICON_FA_CHECK "  Yes, Confirm", ImVec2(120, 32))) {
            auto r = fn(app.selected_console_ip);
            app.addStatus(r.response, !r.ok);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ui::softButton(ICON_FA_XMARK "  No, Cancel", ImVec2(120, 32))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

void renderSystemStateScreen(App& app) {
    const auto& p = ui::colors();
    float avail_w = ImGui::GetContentRegionAvail().x;

    if (app.selected_console_ip.empty()) {
        ImGui::TextColored(p.warning, "%s  Not connected. Connect to a PS5 first.", ICON_FA_TRIANGLE_EXCLAMATION);
        return;
    }

    ImGui::TextColored(p.muted, "%s Target: %s", ICON_FA_SIGNAL, app.selected_console_ip.c_str());
    ImGui::Spacing();

    ui::beginCard("SSMCard", ImVec2(avail_w, 0));
    ui::sectionLabel("Power Controls", ICON_FA_MICROCHIP);
    ImGui::Spacing();

    if (ui::softButton(ICON_FA_CIRCLE_INFO "  Get System Status", ImVec2(avail_w - 36, 42))) {
        auto r = SsmClient::status(app.selected_console_ip);
        app.addStatus(r.response, !r.ok);
    }

    ImGui::Spacing();
    ImGui::TextColored(p.warning, "%s  Destructive operations:", ICON_FA_TRIANGLE_EXCLAMATION);
    ImGui::Spacing();

    confirmSSM(app, ICON_FA_ROTATE, "Reboot PS5", "reboot",
               [](const std::string& ip) { return SsmClient::reboot(ip); });
    ImGui::Spacing();
    confirmSSM(app, ICON_FA_POWER_OFF, "Shutdown PS5", "shutdown",
               [](const std::string& ip) { return SsmClient::shutdown(ip); });
    ImGui::Spacing();
    confirmSSM(app, ICON_FA_MOON, "Enter Rest Mode", "restmode",
               [](const std::string& ip) { return SsmClient::restMode(ip); });

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ui::softButton(ICON_FA_EJECT "  Eject Disc", ImVec2(avail_w - 36, 42))) {
        auto r = SsmClient::ejectDisc(app.selected_console_ip);
        app.addStatus(r.response, !r.ok);
    }

    ui::endCard();
}

} // namespace ghostpad
