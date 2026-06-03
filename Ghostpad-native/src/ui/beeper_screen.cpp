// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"

namespace ghostpad {

void renderBeeperScreen(App& app) {
    const auto& p = ui::colors();
    float avail_w = ImGui::GetContentRegionAvail().x;

    if (app.selected_console_ip.empty()) {
        ImGui::TextColored(p.warning, "%s  Not connected. Connect to a PS5 first.", ICON_FA_TRIANGLE_EXCLAMATION);
        return;
    }

    ImGui::TextColored(p.muted, "%s Target: %s", ICON_FA_SIGNAL, app.selected_console_ip.c_str());
    ImGui::Spacing();

    float col_w = (avail_w - 16.0f) * 0.5f;

    // Beeper card
    ui::beginCard("BeeperSection", ImVec2(col_w, 290));
    ui::sectionLabel("Beeper Commands", ICON_FA_VOLUME_HIGH);
    ImGui::Spacing();

    if (ui::softButton(ICON_FA_BELL "  Single Beep", ImVec2(col_w - 36, 42))) {
        auto r = BeeperClient::buzz(app.selected_console_ip, 1);
        app.addStatus(r.response, !r.ok);
    }
    if (ui::softButton(ICON_FA_BELL "  Long Beep", ImVec2(col_w - 36, 42))) {
        auto r = BeeperClient::buzz(app.selected_console_ip, 3);
        app.addStatus(r.response, !r.ok);
    }
    if (ui::softButton(ICON_FA_TRIANGLE_EXCLAMATION "  Error Pattern", ImVec2(col_w - 36, 42))) {
        auto r = BeeperClient::buzz(app.selected_console_ip, 2);
        app.addStatus(r.response, !r.ok);
    }

    ImGui::Spacing();
    static int spam_n = 5;
    
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(p.muted, "Count:");
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::InputInt("##SpamCount", &spam_n, 0, 0);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ui::softButton(ICON_FA_FIRE "  Spam", ImVec2(80, 32))) {
        for (int i = 0; i < spam_n && i < 50; i++)
            BeeperClient::buzz(app.selected_console_ip, 1);
        app.addStatus("Sent " + std::to_string(spam_n) + " beeps");
    }
    ui::endCard();

    ImGui::SameLine(0, 16);

    // Controls card
    ui::beginCard("ControlsSection", ImVec2(col_w, 290));
    ui::sectionLabel("Controller hardware", ICON_FA_SLIDERS);
    ImGui::Spacing();

    static int vol = 0, mute = 0, led = 0;
    
    ImGui::TextColored(p.muted, "Beeper Volume:");
    ImGui::Combo("##Volume", &vol, "High (0)\0Medium (1)\0Low (2)\0");
    
    ImGui::TextColored(p.muted, "Beeper Mute:");
    ImGui::Combo("##Mute", &mute, "Unmute (0)\0Mute (1)\0");
    
    ImGui::TextColored(p.muted, "LED Brightness:");
    ImGui::Combo("##LED", &led, "High (0)\0Medium (1)\0Low (2)\0");
    
    ImGui::Spacing();
    ImGui::Spacing();

    if (ui::softButton(ICON_FA_CHECK "  Apply All Settings", ImVec2(col_w - 36, 38))) {
        BeeperClient::setVolume(app.selected_console_ip, vol);
        BeeperClient::setMute(app.selected_console_ip, mute);
        BeeperClient::setLed(app.selected_console_ip, led);
        app.addStatus("Settings applied");
    }
    ui::endCard();
}

} // namespace ghostpad
