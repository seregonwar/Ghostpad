// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"

extern "C" void openBrowserURL(const char* url);

#ifndef GHOSTPAD_IOS
#include <cstdlib>
extern "C" void openBrowserURL(const char* url) {
#ifdef _WIN32
    std::string cmd = "start " + std::string(url);
    std::system(cmd.c_str());
#elif defined(__APPLE__)
    std::string cmd = "open " + std::string(url);
    std::system(cmd.c_str());
#else
    std::string cmd = "xdg-open " + std::string(url);
    std::system(cmd.c_str());
#endif
}
#endif

namespace ghostpad {

void renderCreditsScreen(App& app) {
    const auto& p = ui::colors();
    float avail_w = ImGui::GetContentRegionAvail().x;
    float avail_h = ImGui::GetContentRegionAvail().y;

    // Use a single scrolling area for the whole credits screen so that individual cards never show scrollbars
    ImGui::BeginChild("CreditsScrollArea", ImVec2(avail_w, avail_h), false, ImGuiWindowFlags_None);
    float content_w = ImGui::GetContentRegionAvail().x;

    // ─────────────────────────────────────────────────────────────────────────────
    //                            LEAD DEVELOPER (SEREGONWAR)
    // ─────────────────────────────────────────────────────────────────────────────
    ui::beginCard("CreatorCard", ImVec2(content_w, 350), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ui::sectionLabel("Lead Developer & Maintainer", ICON_FA_USER);
    ImGui::Spacing();
    
    ImGui::TextColored(p.primary2, "seregonwar");
    ImGui::SameLine(content_w - 170);
    if (ui::softButton(ICON_FA_UP_RIGHT_FROM_SQUARE "  GitHub Profile", ImVec2(140, 28))) {
        openBrowserURL("https://github.com/seregonwar");
    }
    
    ImGui::Spacing();
    ImGui::TextColored(p.muted, "Contributions:");
    ImGui::BulletText("Native CPP GUI: Ported the original codebase to high-performance C++ using GLFW + Dear ImGui.");
    ImGui::BulletText("ESP32-WROOM-32U bridge firmware (esp32-ghostpad/): WiFi STA/AP with mDNS, BLE HID host, USB HID, Rest API.");
    ImGui::BulletText("PS4 payload port (__ORBIS__ target): SceShellCore injection, runtime VDA patching, and MBus dynamic loading.");
    ImGui::BulletText("Payload-side klog architecture: Capture threads, ring buffers, safe tracking, and prebind loop.");
    ImGui::BulletText("VDA probe (tools/vda_probe/): Fingerprints libScePad VDA patterns and sweeps 20 MBus symbols.");
    ImGui::BulletText("klog parser improvements: Multi-key DeviceId extraction and VDA RemotePlay recognition.");
    ui::endCard();
    
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────────────
    //                            ORIGINAL CREATOR (STONEDMODDER)
    // ─────────────────────────────────────────────────────────────────────────────
    ui::beginCard("OriginalCard", ImVec2(content_w, 250), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ui::sectionLabel("Original Project Creator", ICON_FA_USER_SHIELD);
    ImGui::Spacing();
    
    ImGui::TextColored(p.primary2, "stonedmodder");
    ImGui::SameLine(content_w - 170);
    if (ui::softButton(ICON_FA_UP_RIGHT_FROM_SQUARE "  GitHub Profile", ImVec2(140, 28))) {
        openBrowserURL("https://github.com/stonedmodder");
    }
    
    ImGui::Spacing();
    ImGui::TextColored(p.muted, "Contributions:");
    ImGui::BulletText("Ghostpad-app (Ghostpad-app/): Electron + React desktop GUI for Windows with virtual controller, XInput passthrough.");
    ImGui::BulletText("Original PS5 payload: VDA research, scePadVirtualDeviceAddDevice code-cave, MBus binding flow, ShellCore injection.");
    ImGui::BulletText("hidDumper companion tool for reverse-engineering unknown controllers.");
    ImGui::BulletText("virtualDS5research.md technical write-up.");
    ui::endCard();
    
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────────────
    //                                  LICENSE CARD
    // ─────────────────────────────────────────────────────────────────────────────
    ui::beginCard("LicenseCard", ImVec2(content_w, 160), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ui::sectionLabel("Software License", ICON_FA_FILE_CONTRACT);
    ImGui::Spacing();
    
    ImGui::TextColored(p.primary2, "GNU General Public License v3.0");
    ImGui::SameLine(content_w - 170);
    if (ui::softButton(ICON_FA_UP_RIGHT_FROM_SQUARE "  View Full License", ImVec2(140, 28))) {
        openBrowserURL("https://www.gnu.org/licenses/gpl-3.0.html");
    }
    
    ImGui::TextWrapped("Copyright (C) 2026 seregonwar. This program is free software: you can redistribute it and/or modify it under the terms of the GPLv3 license. See the LICENSE file in the repository for full terms.");
    ui::endCard();

    ImGui::EndChild();
}

} // namespace ghostpad
