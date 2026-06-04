// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"

namespace ghostpad {

extern void renderInteractivePadVisualizer(PadStateInput& state, float size);

static const char* BTN_NAMES[] = {
    "Cross", "Circle", "Square", "Triangle", "L1", "R1", "L2", "R2",
    "Create", "Options", "L3", "R3",
    "Up", "Down", "Left", "Right", "PS", "Touchpad"
};

static const char* keyName(int key) {
    if (key >= 65 && key <= 90) { static char b[2]; b[0] = (char)key; b[1] = 0; return b; }
    switch (key) {
        case 32: return "Space"; case 256: return "Esc"; case 257: return "Enter";
        case 258: return "Tab"; case 259: return "Bksp"; case 260: return "Ins";
        case 261: return "Del"; case 262: return "Right"; case 263: return "Left";
        case 264: return "Down"; case 265: return "Up"; case 340: return "LShift";
        case 344: return "RShift"; case 341: return "LCtrl"; case 345: return "RCtrl";
        default: return "?";
    }
}

void renderInputRedirectScreen(App& app) {
    const auto& p = ui::colors();
    float avail_w = ImGui::GetContentRegionAvail().x;
    auto status = app.ghostpad().getStatus();

    if (!status.is_connected) {
        ImGui::TextColored(p.warning, "%s  Not connected. Go to Consoles first.", ICON_FA_TRIANGLE_EXCLAMATION);
        ImGui::Spacing();
        if (ui::primaryButton(ICON_FA_DESKTOP "  Open Consoles", ImVec2(160, 32)))
            app.current_screen = Screen::Consoles;
        return;
    }

    ImGui::TextColored(p.muted, "%s Connected: %s:%d  |  P%d", ICON_FA_SIGNAL, status.ip.c_str(), status.port, app.activeSlot() + 1);
    ImGui::SameLine(0, 10);
    ImGui::TextColored(p.muted, "Slot:");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(55);
    int slot = app.activeSlot();
    if (ImGui::Combo("##IRSlot", &slot, "P1\0P2\0P3\0P4\0")) {
        app.setActiveSlot(slot);
    }
    ImGui::PopItemWidth();
    ImGui::Spacing();

    auto profileList = app.profiles.list();

    ui::sectionLabel("Keybinding Profiles", ICON_FA_LAYER_GROUP);
    ImGui::Spacing();

    int selectedProfileIdx = -1;
    for (int i = 0; i < (int)profileList.size(); i++) {
        if (profileList[i].id == app.selected_profile_id) {
            selectedProfileIdx = i;
            break;
        }
    }

    const char* previewLabel = selectedProfileIdx >= 0 ? profileList[selectedProfileIdx].name.c_str() : "(none)";
    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("##ProfileCombo", previewLabel)) {
        for (int i = 0; i < (int)profileList.size(); i++) {
            bool isSel = (selectedProfileIdx == i);
            if (ImGui::Selectable(profileList[i].name.c_str(), isSel)) {
                app.keyboard.loadFromProfile(profileList[i]);
                app.selected_profile_id = profileList[i].id;
                auto s = app.settings.read();
                s.active_profile_id = profileList[i].id;
                app.settings.write(s);
                app.addStatus("Loaded profile \"" + profileList[i].name + "\"");
            }
            if (isSel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0, 10);
    if (ui::primaryButton(ICON_FA_PLUS "  Save New", ImVec2(110, 28))) {
        ImGui::OpenPopup("SaveProfilePopup");
    }

    ImGui::SameLine(0, 6);
    if (ui::softButton(ICON_FA_FLOPPY_DISK "  Overwrite", ImVec2(110, 28))) {
        if (!app.selected_profile_id.empty()) {
            auto profile = app.keyboard.saveToProfile("");
            app.profiles.update(app.selected_profile_id, profile);
            app.addStatus("Profile overwritten");
        } else {
            ImGui::OpenPopup("SaveProfilePopup");
        }
    }

    ImGui::SameLine(0, 6);
    if (ui::dangerButton(ICON_FA_TRASH "  Delete", ImVec2(90, 28))) {
        if (!app.selected_profile_id.empty()) {
            app.profiles.remove(app.selected_profile_id);
            app.selected_profile_id.clear();
            auto s = app.settings.read();
            s.active_profile_id = "";
            app.settings.write(s);
            app.keyboard.loadDefaultBindings();
            app.addStatus("Profile deleted, defaults restored");
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("SaveProfilePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(p.primary2, "%s  Save Keybinding Profile", ICON_FA_FLOPPY_DISK);
        ImGui::Separator();
        ImGui::Spacing();
        static char profileNameBuf[128] = {};
        ImGui::Text("Profile Name:");
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("##ProfileName", profileNameBuf, sizeof(profileNameBuf));
        ImGui::Spacing();
        if (ui::primaryButton(ICON_FA_CHECK "  Save", ImVec2(100, 32))) {
            std::string name(profileNameBuf);
            if (!name.empty()) {
                auto entry = app.keyboard.saveToProfile(name);
                auto saved = app.profiles.add(entry);
                app.selected_profile_id = saved.id;
                auto s = app.settings.read();
                s.active_profile_id = saved.id;
                app.settings.write(s);
                app.addStatus("Profile \"" + name + "\" saved");
                memset(profileNameBuf, 0, sizeof(profileNameBuf));
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ui::softButton("Cancel", ImVec2(80, 32))) {
            memset(profileNameBuf, 0, sizeof(profileNameBuf));
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Spacing();

    float col_w = (avail_w - 16.0f) * 0.5f;

    // Left: Button bindings
    ui::beginCard("BindingsCard", ImVec2(col_w, 0));
    ui::sectionLabel("Button Bindings", ICON_FA_KEYBOARD);
    ImGui::Spacing();

    if (ImGui::BeginTable("BindTbl", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 360))) {
        ImGui::TableSetupColumn("PS Button", ImGuiTableColumnFlags_WidthFixed, 100.0f); 
        ImGui::TableSetupColumn("Keyboard Key", ImGuiTableColumnFlags_WidthStretch); 
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < 18; i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); 
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", BTN_NAMES[i]);
            
            ImGui::TableSetColumnIndex(1);
            ImGui::AlignTextToFramePadding();
            auto b = app.keyboard.getButtonBinding(i);
            if (b.glfw_key)
                ImGui::TextColored(p.primary2, "%s", keyName(b.glfw_key));
            else
                ImGui::TextColored(p.dim, "(none)");

            ImGui::TableSetColumnIndex(2);
            ImGui::PushID(i);
            if (ui::softButton(ICON_FA_KEY "  Set", ImVec2(70, 24))) {
                app.rebind_button_id = i;
                app.rebind_button_name = BTN_NAMES[i];
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (app.rebind_button_id >= 0)
        ImGui::OpenPopup("RebindPopup");
        
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("RebindPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(p.primary2, "%s  Rebind %s", ICON_FA_KEY, app.rebind_button_name.c_str());
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Press any keyboard key to bind...");
        ImGui::TextColored(p.muted, "(Press ESC to cancel/clear binding)");
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Spacing();
    if (ui::softButton(ICON_FA_ARROW_ROTATE_LEFT "  Restore Defaults", ImVec2(col_w - 36, 34))) {
        app.keyboard.loadDefaultBindings();
        app.addStatus("Default bindings restored");
    }
    ui::endCard();

    ImGui::SameLine(0, 16);

    // Right: Options
    ui::beginCard("OptionsCard", ImVec2(col_w, 0));
    ui::sectionLabel("Control Settings", ICON_FA_GEAR);
    ImGui::Spacing();

    // Mouse look
    static bool ml = false;
    if (ImGui::Checkbox("Enable Mouse Look", &ml)) app.keyboard.setMouseLook(ml);
    
    static float sens = 3.0f;
    ImGui::TextColored(p.muted, "Mouse Sensitivity:");
    ImGui::SliderFloat("##Sensitivity", &sens, 0.1f, 50.0f, "%.1f");
    if (sens != app.keyboard.getSensitivity()) app.keyboard.setSensitivity(sens);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Auto clicker
    static bool ac = false;
    ImGui::Checkbox("Enable Auto Clicker", &ac);
    
    static int ac_btn = 0, ac_hold = 100, ac_gap = 500;
    ImGui::TextColored(p.muted, "Target Button:");
    ImGui::SetNextItemWidth(col_w - 36);
    ImGui::Combo("##Button", &ac_btn, BTN_NAMES, 18);
    
    ImGui::TextColored(p.muted, "Hold duration (ms):");
    ImGui::SliderInt("##HoldMs", &ac_hold, 16, 2000);
    
    ImGui::TextColored(p.muted, "Interval gap (ms):");
    ImGui::SliderInt("##GapMs", &ac_gap, 0, 5000);
    
    float cps = ac_gap > 0 ? 1000.0f / (ac_hold + ac_gap) : 0;
    ImGui::TextColored(p.primary2, "%s  %.1f clicks/second", ICON_FA_BOLT, cps);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Gamepad
    ImGui::TextColored(p.muted, "%s  Physical Gamepad Passthrough:", ICON_FA_GAMEPAD);
    ImGui::SameLine(col_w - 180);
    if (ui::softButton(ICON_FA_ARROW_ROTATE_RIGHT "  Scan gamepads", ImVec2(140, 26))) app.gamepad_input.update();
    
    ImGui::Spacing();
    auto pads = app.gamepad_input.listGamepads();
    if (pads.empty())
        ImGui::TextColored(p.dim, "No physical controllers detected.");
    else
        for (auto& g : pads)
            ImGui::TextColored(p.success, "  %s  ID %d: %s", ICON_FA_CIRCLE_CHECK, g.index, g.name.c_str());

    ui::endCard();
}

} // namespace ghostpad
