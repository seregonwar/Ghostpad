#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.


/*
 *  ________  ___  ___  ________  ________  _________  ________  ________  ________     
 * |\   ____\|\  \|\  \|\   __  \|\   ____\|\___   ___\\   __  \|\   __  \|\   ___ \    
 * \ \  \___|\ \  \\\  \ \  \|\  \ \  \___|\|___ \  \_\ \  \|\  \ \  \|\  \ \  \_|\ \   
 *  \ \  \  __\ \   __  \ \  \\\  \ \_____  \   \ \  \ \ \   ____\ \   __  \ \  \ \\ \  
 *   \ \  \|\  \ \  \ \  \ \  \\\  \|____|\  \   \ \  \ \ \  \___|\ \  \ \  \ \  \_\\ \ 
 *    \ \_______\ \__\ \__\ \_______\____\_\  \   \ \__\ \ \__\    \ \__\ \__\ \_______\
 *     \|_______|\|__|\|__|\|_______|\|_________|   \|__|   \|__|     \|__|\|__|\|_______|
 *                                  \|_________|                                        
 */

#include "imgui.h"
#include "IconsFontAwesome6.h"
#include <string>

namespace ghostpad::ui {

// ============================================================================
// [Color Utilities]
// ============================================================================

inline ImVec4 rgba(int r, int g, int b, int a = 255) {
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

inline ImU32 u32(const ImVec4& c) {
    return ImGui::ColorConvertFloat4ToU32(c);
}

inline ImVec4 withAlpha(ImVec4 c, float alpha) {
    c.w = alpha;
    return c;
}

// ============================================================================
// [Visual Theme Palette]
// ============================================================================

struct Palette {
    ImVec4 bg0       = rgba(11, 11, 14);
    ImVec4 bg1       = rgba(18, 17, 22);
    ImVec4 bg2       = rgba(28, 26, 34);
    ImVec4 bg3       = rgba(45, 42, 54);
    ImVec4 panel     = rgba(17, 16, 22, 220);
    ImVec4 panel2    = rgba(24, 22, 29, 235);
    ImVec4 border    = rgba(48, 44, 58, 180);
    ImVec4 borderHot = rgba(110, 80, 150, 180);
    ImVec4 text      = rgba(240, 238, 245);
    ImVec4 muted     = rgba(170, 165, 185);
    ImVec4 dim       = rgba(110, 105, 125);
    ImVec4 primary   = rgba(145, 71, 225);
    ImVec4 primary2  = rgba(185, 140, 255);
    ImVec4 link      = rgba(50, 160, 240);
    ImVec4 success   = rgba(40, 200, 120);
    ImVec4 warning   = rgba(255, 160, 50);
    ImVec4 danger    = rgba(240, 70, 90);
};

inline const Palette& colors() {
    static Palette p;
    return p;
}

// ============================================================================
// [ImGui Style Application]
// ============================================================================

inline void applyGhostpadTheme() {
    const auto& p = colors();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* c = style.Colors;

    c[ImGuiCol_Text]                  = p.text;
    c[ImGuiCol_TextDisabled]          = p.dim;
    c[ImGuiCol_WindowBg]              = p.bg0;
    c[ImGuiCol_ChildBg]               = p.panel;
    c[ImGuiCol_PopupBg]               = rgba(14, 13, 18, 250);
    c[ImGuiCol_Border]                = p.border;
    c[ImGuiCol_BorderShadow]          = rgba(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]               = p.bg2;
    c[ImGuiCol_FrameBgHovered]        = rgba(48, 38, 62);
    c[ImGuiCol_FrameBgActive]         = rgba(62, 45, 84);
    c[ImGuiCol_TitleBg]               = p.bg0;
    c[ImGuiCol_TitleBgActive]         = p.bg1;
    c[ImGuiCol_TitleBgCollapsed]      = p.bg0;
    c[ImGuiCol_MenuBarBg]             = p.bg1;
    c[ImGuiCol_ScrollbarBg]           = rgba(11, 11, 14, 160);
    c[ImGuiCol_ScrollbarGrab]         = rgba(62, 55, 78);
    c[ImGuiCol_ScrollbarGrabHovered]  = rgba(88, 76, 110);
    c[ImGuiCol_ScrollbarGrabActive]   = p.primary;
    c[ImGuiCol_CheckMark]             = p.primary2;
    c[ImGuiCol_SliderGrab]            = p.primary;
    c[ImGuiCol_SliderGrabActive]      = p.primary2;
    c[ImGuiCol_Button]                = rgba(38, 34, 48);
    c[ImGuiCol_ButtonHovered]         = rgba(68, 48, 92);
    c[ImGuiCol_ButtonActive]          = rgba(94, 58, 128);
    c[ImGuiCol_Header]                = rgba(38, 30, 50);
    c[ImGuiCol_HeaderHovered]         = rgba(72, 45, 100);
    c[ImGuiCol_HeaderActive]          = rgba(94, 55, 130);
    c[ImGuiCol_Separator]             = p.border;
    c[ImGuiCol_SeparatorHovered]      = p.primary;
    c[ImGuiCol_SeparatorActive]       = p.primary2;
    c[ImGuiCol_ResizeGrip]            = rgba(95, 72, 116, 120);
    c[ImGuiCol_ResizeGripHovered]     = rgba(145, 71, 225, 180);
    c[ImGuiCol_ResizeGripActive]      = p.primary;
    c[ImGuiCol_Tab]                   = p.bg1;
    c[ImGuiCol_TabHovered]            = rgba(68, 48, 92);
    c[ImGuiCol_TabActive]             = rgba(48, 34, 68);
    c[ImGuiCol_PlotLines]             = p.primary2;
    c[ImGuiCol_PlotHistogram]         = p.success;
    c[ImGuiCol_TableHeaderBg]         = rgba(38, 32, 48);
    c[ImGuiCol_TableBorderStrong]     = p.borderHot;
    c[ImGuiCol_TableBorderLight]      = p.border;
    c[ImGuiCol_TableRowBg]            = rgba(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]         = rgba(255, 255, 255, 6);
    c[ImGuiCol_TextSelectedBg]        = rgba(145, 71, 225, 90);
    c[ImGuiCol_NavHighlight]          = p.primary;
    c[ImGuiCol_ModalWindowDimBg]      = rgba(0, 0, 0, 180);

    style.WindowPadding     = ImVec2(0, 0);
    style.FramePadding      = ImVec2(12, 8);
    style.CellPadding       = ImVec2(10, 8);
    style.ItemSpacing       = ImVec2(12, 10);
    style.ItemInnerSpacing  = ImVec2(8, 7);
    style.TouchExtraPadding = ImVec2(0, 0);
    style.IndentSpacing     = 20.0f;
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 12.0f;
    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.WindowRounding    = 0.0f;
    style.ChildRounding     = 12.0f;
    style.FrameRounding     = 8.0f;
    style.PopupRounding     = 12.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding      = 10.0f;
    style.TabRounding       = 8.0f;
}

// ============================================================================
// [Background Grid Rendering]
// ============================================================================

inline void drawBackground(ImDrawList* dl, ImVec2 pos, ImVec2 size) {
    const auto& p = colors();
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    dl->AddRectFilled(pos, max, u32(p.bg0));
    dl->AddRectFilledMultiColor(
        pos, max,
        u32(rgba(24, 14, 38, 245)), u32(rgba(11, 11, 14, 255)),
        u32(rgba(11, 11, 14, 255)), u32(rgba(11, 11, 14, 255)));

    const float grid = 48.0f;
    ImU32 grid_col = IM_COL32(145, 71, 225, 12);
    for (float x = pos.x; x < max.x; x += grid) {
        dl->AddLine(ImVec2(x, pos.y), ImVec2(x, max.y), grid_col, 1.0f);
    }
    for (float y = pos.y; y < max.y; y += grid) {
        dl->AddLine(ImVec2(pos.x, y), ImVec2(max.x, y), grid_col, 1.0f);
    }
}

// ============================================================================
// [Typography & Text Styling]
// ============================================================================

inline void textMuted(const char* text) {
    ImGui::TextColored(colors().muted, "%s", text);
}

inline void textAccent(const char* text) {
    ImGui::TextColored(colors().primary2, "%s", text);
}

inline void pageTitle(const char* title, const char* subtitle = nullptr) {
    const auto& p = colors();
    ImGui::TextColored(p.primary2, "%s", title);
    if (subtitle && subtitle[0]) {
        ImGui::SameLine();
        ImGui::TextColored(p.muted, "%s", subtitle);
    }
    ImGui::Spacing();
}

inline void sectionLabel(const char* label, const char* icon = nullptr) {
    const auto& p = colors();
    ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
    if (icon && icon[0]) {
        ImGui::Text("%s  %s", icon, label);
    } else {
        ImGui::TextUnformatted(label);
    }
    ImGui::PopStyleColor();
    ImGui::Separator();
}

// ============================================================================
// [Layout Container Cards]
// ============================================================================

inline void beginCard(const char* id, ImVec2 size = ImVec2(0, 0), bool border = true) {
    const auto& p = colors();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.panel2);
    ImGui::PushStyleColor(ImGuiCol_Border, border ? p.border : withAlpha(p.border, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 16));
    ImGui::BeginChild(id, size, border);
}

inline void endCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// ============================================================================
// [UI Component Elements]
// ============================================================================

inline void statusPill(const char* label, bool active) {
    const auto& p = colors();
    ImVec4 bg = active ? rgba(20, 48, 30, 230) : rgba(48, 20, 25, 230);
    ImVec4 border = active ? withAlpha(p.success, 0.7f) : withAlpha(p.danger, 0.7f);
    ImVec4 text = active ? p.success : p.danger;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 ts = ImGui::CalcTextSize(label);
    ImVec2 size(ts.x + 28.0f, 28.0f);
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), u32(bg), 14.0f);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), u32(border), 14.0f, 0, 1.0f);
    dl->AddCircleFilled(ImVec2(pos.x + 14.0f, pos.y + size.y * 0.5f), 4.0f, u32(text));
    dl->AddText(ImVec2(pos.x + 22.0f, pos.y + (size.y - ts.y) * 0.5f), u32(text), label);
    ImGui::Dummy(size);
}

inline bool primaryButton(const char* label, ImVec2 size = ImVec2(0, 0)) {
    const auto& p = colors();
    ImGui::PushStyleColor(ImGuiCol_Button, p.primary);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgba(165, 85, 245));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgba(125, 55, 195));
    ImGui::PushStyleColor(ImGuiCol_Text, rgba(255, 255, 255));
    bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return pressed;
}

inline bool dangerButton(const char* label, ImVec2 size = ImVec2(0, 0)) {
    const auto& p = colors();
    ImGui::PushStyleColor(ImGuiCol_Button, withAlpha(p.danger, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, p.danger);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgba(195, 50, 70));
    bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return pressed;
}

inline bool softButton(const char* label, ImVec2 size = ImVec2(0, 0)) {
    const auto& p = colors();
    ImGui::PushStyleColor(ImGuiCol_Button, rgba(38, 34, 48));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgba(55, 45, 74));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgba(68, 50, 92));
    ImGui::PushStyleColor(ImGuiCol_Border, p.borderHot);
    bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return pressed;
}

inline bool navItem(const char* icon, const char* label, const char* hint, bool selected, ImVec4 accent, ImVec2 size = ImVec2(0, 54)) {
    const auto& p = colors();
    ImGui::PushID(label);
    ImGui::PushStyleColor(ImGuiCol_Button, selected ? withAlpha(accent, 0.22f) : withAlpha(p.bg2, 0.30f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, withAlpha(accent, 0.16f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, withAlpha(accent, 0.26f));
    ImGui::PushStyleColor(ImGuiCol_Border, selected ? withAlpha(accent, 0.85f) : withAlpha(p.border, 0.70f));
    bool pressed = ImGui::Button("##nav_button", size);
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (selected) {
        dl->AddRectFilled(min, ImVec2(min.x + 4.0f, max.y), u32(accent), 8.0f);
    }
    
    // Draw icon if present
    float text_offset = 18.0f;
    if (icon && icon[0]) {
        dl->AddText(ImVec2(min.x + 18.0f, min.y + 18.0f), u32(selected ? p.text : p.muted), icon);
        text_offset = 42.0f;
    }

    dl->AddText(ImVec2(min.x + text_offset, min.y + 9.0f), u32(selected ? p.text : p.muted), label);
    dl->AddText(ImVec2(min.x + text_offset, min.y + 29.0f), u32(withAlpha(p.muted, 0.80f)), hint);
    ImGui::PopStyleColor(4);
    ImGui::PopID();
    return pressed;
}

} // namespace ghostpad::ui
