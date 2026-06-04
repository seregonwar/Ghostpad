// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"

namespace ghostpad {

extern void renderPadVisualizer(App& app, const PadStateInput& state, float size);

void renderProjectsScreen(App& app) {
    const auto& p = ui::colors();
    float avail_w = ImGui::GetContentRegionAvail().x;
    bool recording = app.macro_engine.isRecording();
    bool playing = app.macro_engine.isPlaying();

    // Controls bar
    if (recording) {
        if (ui::dangerButton(ICON_FA_STOP "  Stop Recording", ImVec2(160, 34))) {
            app.macro_engine.stopRecording();
            auto sigs = app.macro_engine.getRecordedSignals();
            app.addStatus("Recorded " + std::to_string(sigs.size()) + " signals");
        }
    } else if (!playing) {
        if (ui::primaryButton(ICON_FA_CIRCLE "  Start Recording", ImVec2(160, 34)))
            app.macro_engine.startRecording();
    }
    
    ImGui::SameLine();
    if (playing && ui::dangerButton(ICON_FA_STOP "  Stop Playback", ImVec2(140, 34)))
        app.macro_engine.stopPlayback();
        
    ImGui::SameLine();
    if (ui::softButton(ICON_FA_PLUS "  New Project", ImVec2(140, 34)))
        ImGui::OpenPopup("NewProjectPopup");

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("NewProjectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(p.primary2, "%s  Create Macro Project", ICON_FA_FOLDER);
        ImGui::Separator();
        ImGui::Spacing();

        static char pname[64] = {}, pdesc[128] = {}, pgame[64] = {};
        
        ImGui::TextColored(p.muted, "Project Name:");
        ImGui::InputText("##Name", pname, sizeof(pname));
        
        ImGui::TextColored(p.muted, "Game Target:");
        ImGui::InputText("##Game", pgame, sizeof(pgame));
        
        ImGui::TextColored(p.muted, "Description:");
        ImGui::InputText("##Desc", pdesc, sizeof(pdesc));
        
        ImGui::Spacing();
        ImGui::Spacing();

        if (ui::primaryButton(ICON_FA_CHECK "  Create", ImVec2(110, 32)) && strlen(pname) > 0) {
            app.projects.add(pname, pdesc, pgame);
            app.addStatus("Project created");
            pname[0] = pdesc[0] = pgame[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ui::softButton(ICON_FA_XMARK "  Cancel", ImVec2(110, 32))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    ImGui::Spacing();

    // Projects list
    ui::beginCard("ProjectsList", ImVec2(avail_w, 0));
    ui::sectionLabel("Macro Projects", ICON_FA_FOLDER_OPEN);
    ImGui::Spacing();

    auto all = app.projects.list();
    if (all.empty()) {
        ImGui::TextColored(p.muted, "No projects yet. Create one above.");
    } else {
        for (auto& proj : all) {
            ImGui::PushID(proj.id.c_str());
            
            ImGui::PushStyleColor(ImGuiCol_ChildBg, p.bg1);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::BeginChild("ProjRow", ImVec2(avail_w - 36, 54), true,
                              ImGuiWindowFlags_NoScrollbar);

            ImGui::SetCursorPos(ImVec2(16, 12));
            bool sel = (app.selected_project_id == proj.id);
            std::string proj_lbl = std::string(ICON_FA_FOLDER) + "  " + proj.name;
            if (ImGui::Selectable(proj_lbl.c_str(), &sel, 0, ImVec2(avail_w * 0.25f, 30))) {
                app.selected_project_id = proj.id;
            }
            
            ImGui::SameLine(avail_w * 0.3f);
            ImGui::SetCursorPosY(14);
            ImGui::TextColored(p.muted, "%s  %s", ICON_FA_GAMEPAD, proj.game.c_str());
            
            ImGui::SameLine(avail_w * 0.5f);
            ImGui::TextColored(p.dim, "%s  %zu commands", ICON_FA_CODE, proj.commands.size());
            
            ImGui::SameLine(avail_w * 0.68f);
            ImGui::TextColored(p.dim, "%s  %s", ICON_FA_CALENDAR, proj.updated_at.substr(0, 10).c_str());

            ImGui::SameLine(avail_w - 200);
            ImGui::SetCursorPosY(11);
            if (ui::primaryButton(ICON_FA_FOLDER_OPEN "  Open", ImVec2(80, 30))) {
                app.selected_project_id = proj.id;
                app.current_screen = Screen::ProjectDetail;
            }
            ImGui::SameLine();
            if (ui::dangerButton(ICON_FA_TRASH "  Del", ImVec2(70, 30))) {
                app.projects.remove(proj.id);
                if (app.selected_project_id == proj.id) app.selected_project_id.clear();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::PopID();
            ImGui::Spacing();
        }
    }
    ui::endCard();
}

void renderProjectDetailScreen(App& app) {
    const auto& p = ui::colors();
    float avail_w = ImGui::GetContentRegionAvail().x;
    auto proj = app.projects.get(app.selected_project_id);

    if (proj.id.empty()) {
        ImGui::TextColored(p.warning, "Project not found.");
        if (ui::softButton(ICON_FA_ARROW_LEFT "  Back", ImVec2(100, 30))) app.current_screen = Screen::Projects;
        return;
    }

    if (ui::softButton(ICON_FA_ARROW_LEFT "  Back", ImVec2(100, 30))) app.current_screen = Screen::Projects;
    ImGui::SameLine();
    ImGui::TextColored(p.primary2, "%s  %s", ICON_FA_FOLDER_OPEN, proj.name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(p.muted, "|   %s  %s   |   %zu commands", ICON_FA_GAMEPAD, proj.game.c_str(), proj.commands.size());

    bool rec = app.macro_engine.isRecording();
    bool play = app.macro_engine.isPlaying();

    ImGui::SameLine(avail_w - 360);
    if (rec) {
        if (ui::dangerButton(ICON_FA_STOP "  Stop Recording", ImVec2(150, 30))) {
            app.macro_engine.stopRecording();
            auto sigs = app.macro_engine.getRecordedSignals();
            MacroCommand cmd;
            cmd.name = "Recording " + std::to_string(proj.commands.size() + 1);
            cmd.type = "macro";
            cmd.signals = sigs;
            app.projects.addCommand(proj.id, cmd);
            app.addStatus("Saved " + std::to_string(sigs.size()) + " signals");
        }
    } else if (!play) {
        if (ui::primaryButton(ICON_FA_CIRCLE "  Record Command", ImVec2(160, 30)))
            app.macro_engine.startRecording();
    }
    
    ImGui::SameLine();
    if (play && ui::dangerButton(ICON_FA_STOP "  Stop", ImVec2(80, 30)))
        app.macro_engine.stopPlayback();

    ImGui::Spacing();

    // Commands table
    ui::beginCard("CmdTable", ImVec2(avail_w, 0));
    ui::sectionLabel("Project Commands", ICON_FA_LIST);
    ImGui::Spacing();
    
    if (proj.commands.empty()) {
        ImGui::TextColored(p.muted, "No commands recorded for this project. Start recording above!");
    } else if (ImGui::BeginTable("Cmds", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Command Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Signal Count", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableHeadersRow();

        for (auto& cmd : proj.commands) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); 
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", cmd.name.c_str());
            
            ImGui::TableSetColumnIndex(1); 
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%zu", cmd.signals.size());

            int max_ms = 0;
            for (auto& s : cmd.signals) if (s.time_ms > max_ms) max_ms = s.time_ms;
            ImGui::TableSetColumnIndex(2);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%.1fs", max_ms / 1000.0);

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(cmd.id.c_str());
            
            if (ui::primaryButton(ICON_FA_PLAY "  Play", ImVec2(70, 26))) {
                app.macro_engine.startPlayback(cmd.signals);
                app.addStatus("Playing: " + cmd.name);
            }
            ImGui::SameLine();
            if (ui::softButton(ICON_FA_FILE_EXPORT "  Export", ImVec2(80, 26))) {
                std::string py = MacroEngine::exportAsPython(cmd.signals, proj.name + "_" + cmd.name);
                printf("=== PYTHON EXPORT ===\n%s\n=== END ===\n", py.c_str());
                app.addStatus("Exported to console logs");
            }
            ImGui::SameLine();
            if (ui::dangerButton(ICON_FA_TRASH "  Del", ImVec2(60, 26))) {
                app.projects.removeCommand(proj.id, cmd.id);
                app.addStatus("Command deleted");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (app.macro_engine.isPlaying()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(p.success, "%s  Playing...", ICON_FA_PLAY);
        renderPadVisualizer(app, app.macro_engine.getPlaybackState(), 120.0f);
    }
    ui::endCard();
}

} // namespace ghostpad
