// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2024  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "imgui.h"

namespace ghostpad {

void renderBeeperScreen(App& app) {
    ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f), "BEEPER CONTROL");
    ImGui::SameLine();
    ImGui::TextUnformatted("- PS5 Beeper & LED");
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

    // Beeper
    ImGui::BeginChild("BeeperSection", ImVec2(ImGui::GetContentRegionAvail().x * 0.48f, 0), true);
    ImGui::TextUnformatted("Beeper");
    ImGui::Separator();

    if (ImGui::Button("Single Beep", ImVec2(200, 40))) {
        auto r = BeeperClient::buzz(app.selected_console_ip, 1);
        app.addStatus(r.response, !r.ok);
    }
    if (ImGui::Button("Long Beep", ImVec2(200, 40))) {
        auto r = BeeperClient::buzz(app.selected_console_ip, 3);
        app.addStatus(r.response, !r.ok);
    }
    if (ImGui::Button("Error Pattern", ImVec2(200, 40))) {
        auto r = BeeperClient::buzz(app.selected_console_ip, 2);
        app.addStatus(r.response, !r.ok);
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Spam Test");
    static int spam_count = 10;
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Beeps", &spam_count);
    ImGui::SameLine();
    if (ImGui::Button("Spam", ImVec2(100, 0))) {
        for (int i = 0; i < spam_count && i < 50; i++) {
            BeeperClient::buzz(app.selected_console_ip, 1);
        }
        app.addStatus("Sent " + std::to_string(spam_count) + " beeps");
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // Controls
    ImGui::BeginChild("ControlsSection", ImVec2(0, 0), true);

    // Volume
    ImGui::TextUnformatted("Volume");
    ImGui::Separator();
    static int volume = 0;
    const char* vol_types[] = {"High (0)", "Medium (1)", "Low (2)"};
    ImGui::Combo("##Vol", &volume, vol_types, 3);
    if (ImGui::Button("Set Volume", ImVec2(200, 0))) {
        auto r = BeeperClient::setVolume(app.selected_console_ip, volume);
        app.addStatus(r.response, !r.ok);
    }

    ImGui::Spacing();

    // Mute
    ImGui::TextUnformatted("Mute");
    ImGui::Separator();
    static int mute_state = 0;
    const char* mute_types[] = {"Unmute (0)", "Mute (1)"};
    ImGui::Combo("##Mute", &mute_state, mute_types, 2);
    if (ImGui::Button("Set Mute", ImVec2(200, 0))) {
        auto r = BeeperClient::setMute(app.selected_console_ip, mute_state);
        app.addStatus(r.response, !r.ok);
    }

    ImGui::Spacing();

    // LED
    ImGui::TextUnformatted("LED Control");
    ImGui::Separator();
    static int led = 0;
    const char* led_types[] = {"High (0)", "Medium (1)", "Low (2)"};
    ImGui::Combo("##LED", &led, led_types, 3);
    if (ImGui::Button("Set LED", ImVec2(200, 0))) {
        auto r = BeeperClient::setLed(app.selected_console_ip, led);
        app.addStatus(r.response, !r.ok);
    }

    ImGui::Spacing();

    // Deploy beeper ELF
    ImGui::TextUnformatted("Deploy Beeper ELF");
    ImGui::Separator();
    static char beeper_elf[1024] = {};
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120);
    ImGui::InputText("##BeeperELF", beeper_elf, sizeof(beeper_elf));
    ImGui::SameLine();
    if (ImGui::Button("Deploy", ImVec2(100, 0))) {
        if (strlen(beeper_elf) > 0) {
            auto r = BeeperClient::deployElf(app.selected_console_ip, beeper_elf, 9021);
            app.addStatus(r.response, !r.ok);
        }
    }

    ImGui::EndChild();
}

} // namespace ghostpad
