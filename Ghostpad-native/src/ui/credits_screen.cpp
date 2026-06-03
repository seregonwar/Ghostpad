// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2024  seregonwar
// Based on original Ghostpad by stonedmodder
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "imgui.h"

namespace ghostpad {

void renderCreditsScreen(App& app) {
    ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f), "CREDITS & LICENSE");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("CreditsContent", ImVec2(0, 0), true);

    // Creator
    ImGui::TextColored(ImVec4(0.6f, 0.85f, 0.7f, 1.0f), "Created by");
    ImGui::Separator();

    ImVec2 btn_size(ImGui::GetContentRegionAvail().x * 0.8f, 50);
    ImGui::BeginChild("CreatorCard", ImVec2(btn_size.x + 20, 100), true);
    ImGui::TextUnformatted("seregonwar");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
    if (ImGui::Button("GitHub", ImVec2(90, 0))) {
        system("open https://github.com/seregonwar");
    }
    ImGui::TextUnformatted("Creator and maintainer of Ghostpad Native");
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Spacing();

    // Original
    ImGui::TextColored(ImVec4(0.6f, 0.85f, 0.7f, 1.0f), "Original Ghostpad");
    ImGui::Separator();

    ImGui::BeginChild("OriginalCard", ImVec2(btn_size.x + 20, 100), true);
    ImGui::TextUnformatted("stonedmodder");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
    if (ImGui::Button("GitHub", ImVec2(90, 0))) {
        system("open https://github.com/stonedmodder");
    }
    ImGui::TextUnformatted("Creator of the original Electron-based Ghostpad application");
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Spacing();

    // License
    ImGui::TextColored(ImVec4(0.6f, 0.85f, 0.7f, 1.0f), "License");
    ImGui::Separator();

    ImGui::BeginChild("LicenseCard", ImVec2(0, 0), true);
    ImGui::TextUnformatted("GNU General Public License v3.0");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Copyright (C) 2024  seregonwar\n\n"
        "This program is free software: you can redistribute it and/or modify "
        "it under the terms of the GNU General Public License as published by "
        "the Free Software Foundation, either version 3 of the License, or "
        "(at your option) any later version.\n\n"
        "This program is distributed in the hope that it will be useful, "
        "but WITHOUT ANY WARRANTY; without even the implied warranty of "
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
        "GNU General Public License for more details.\n\n"
        "You should have received a copy of the GNU General Public License "
        "along with this program.  If not, see <https://www.gnu.org/licenses/>."
    );

    ImGui::EndChild();

    ImGui::EndChild();
}

} // namespace ghostpad
