// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"

namespace ghostpad {

void renderCreditsScreen(App& app) {
    const auto& p = ui::colors();
    float avail_w = ImGui::GetContentRegionAvail().x;
    float avail_h = ImGui::GetContentRegionAvail().y;

    // Calculate height dynamically to give contributors 80% of space and license a compact bottom card
    float contrib_h = (avail_h - 170.0f) * 0.5f;

    // Creator Card
    ui::beginCard("CreatorCard", ImVec2(avail_w, contrib_h));
    ui::sectionLabel("Lead Developer & Maintainer", ICON_FA_USER);
    ImGui::Spacing();
    
    ImGui::TextColored(p.primary2, "seregonwar");
    ImGui::SameLine(avail_w - 170);
    if (ui::softButton(ICON_FA_UP_RIGHT_FROM_SQUARE "  GitHub Profile", ImVec2(140, 28))) {
        system("open https://github.com/seregonwar");
    }
    
    ImGui::Spacing();
    ImGui::TextColored(p.muted, "Contributions:");
    ImGui::BulletText("Ported the original codebase to native high-performance C++.");
    ImGui::BulletText("Designed and built the GLFW + Dear ImGui visual user interface.");
    ImGui::BulletText("Implemented low-overhead TCP controller streaming under <1ms latency.");
    ImGui::BulletText("Wrote the ESP32 firmware for remote controller emulation (supporting any controller).");
    ui::endCard();
    
    ImGui::Spacing();

    // Original Creator Card
    ui::beginCard("OriginalCard", ImVec2(avail_w, contrib_h));
    ui::sectionLabel("Original Project Creator", ICON_FA_USER_SHIELD);
    ImGui::Spacing();
    
    ImGui::TextColored(p.primary2, "stonedmodder");
    ImGui::SameLine(avail_w - 170);
    if (ui::softButton(ICON_FA_UP_RIGHT_FROM_SQUARE "  GitHub Profile", ImVec2(140, 28))) {
        system("open https://github.com/stonedmodder");
    }
    
    ImGui::Spacing();
    ImGui::TextColored(p.muted, "Contributions:");
    ImGui::BulletText("Designed the original Ghostpad core Electron application architecture.");
    ImGui::BulletText("Established the initial controller network packet formatting protocol.");
    ImGui::BulletText("Created the macro recording engine and Python packet exporter.");
    ui::endCard();
    
    ImGui::Spacing();

    // Compact License Card
    ui::beginCard("LicenseCard", ImVec2(avail_w, 130));
    ui::sectionLabel("Software License", ICON_FA_FILE_CONTRACT);
    ImGui::Spacing();
    
    ImGui::TextColored(p.primary2, "GNU General Public License v3.0");
    ImGui::SameLine(avail_w - 170);
    if (ui::softButton(ICON_FA_UP_RIGHT_FROM_SQUARE "  View Full License", ImVec2(140, 28))) {
        system("open https://www.gnu.org/licenses/gpl-3.0.html");
    }
    
    ImGui::TextWrapped("Copyright (C) 2026 seregonwar. This program is free software: you can redistribute it and/or modify it under the terms of the GPLv3 license. See the LICENSE file in the repository for full terms.");
    ui::endCard();
}

} // namespace ghostpad
