// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2024  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "imgui.h"

namespace ghostpad {

extern void renderPadVisualizer(const PadStateInput& state, float size);

void renderProjectsScreen(App& app) {
    ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f), "MACRO PROJECTS");
    ImGui::SameLine();
    ImGui::TextUnformatted("- Create & Manage Macros");
    ImGui::Separator();
    ImGui::Spacing();

    // Macro recording controls
    bool recording = app.macro_engine.isRecording();
    bool playing = app.macro_engine.isPlaying();

    if (!recording && !playing) {
        if (ImGui::Button("Start Recording", ImVec2(160, 35))) {
            app.macro_engine.startRecording();
            app.addStatus("Macro recording started");
        }
    }
    ImGui::SameLine();
    if (recording) {
        if (ImGui::Button("Stop Recording", ImVec2(160, 35))) {
            app.macro_engine.stopRecording();
            auto signals = app.macro_engine.getRecordedSignals();
            char buf[128];
            snprintf(buf, sizeof(buf), "Recorded %zu signals", signals.size());
            app.addStatus(buf);

            // Save as new command in selected project
            if (!app.selected_project_id.empty()) {
                MacroCommand cmd;
                cmd.name = "Recording";
                cmd.type = "macro";
                cmd.signals = signals;
                app.projects.addCommand(app.selected_project_id, cmd);
            }
        }
    }
    ImGui::SameLine();
    if (playing) {
        if (ImGui::Button("Stop Playback", ImVec2(160, 35))) {
            app.macro_engine.stopPlayback();
            app.addStatus("Playback stopped");
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("New Project", ImVec2(140, 35))) {
        ImGui::OpenPopup("NewProjectPopup");
    }

    if (ImGui::BeginPopupModal("NewProjectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char proj_name[64] = {};
        static char proj_desc[128] = {};
        static char proj_game[64] = {};

        ImGui::InputText("Name", proj_name, sizeof(proj_name));
        ImGui::InputText("Description", proj_desc, sizeof(proj_desc));
        ImGui::InputText("Game", proj_game, sizeof(proj_game));

        if (ImGui::Button("Create", ImVec2(100, 0))) {
            if (strlen(proj_name) > 0) {
                app.projects.add(proj_name, proj_desc, proj_game);
                app.addStatus("Project created");
            }
            proj_name[0] = '\0';
            proj_desc[0] = '\0';
            proj_game[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Projects list
    auto all_projects = app.projects.list();

    if (all_projects.empty()) {
        ImGui::TextUnformatted("No projects yet. Create one to start recording macros.");
    }

    for (auto& p : all_projects) {
        ImGui::PushID(p.id.c_str());
        ImGui::BeginChild("ProjectEntry", ImVec2(ImGui::GetContentRegionAvail().x, 70), true,
                          ImGuiWindowFlags_NoScrollbar);

        bool selected = (app.selected_project_id == p.id);
        if (ImGui::Selectable(p.name.c_str(), &selected, 0, ImVec2(200, 0))) {
            app.selected_project_id = p.id;
        }

        ImGui::SameLine(220);
        ImGui::Text("%s", p.game.c_str());
        ImGui::SameLine(400);
        ImGui::Text("%zu commands", p.commands.size());
        ImGui::SameLine(550);
        ImGui::Text("%s", p.updated_at.c_str());

        ImGui::SameLine(750);
        if (ImGui::Button("Open", ImVec2(60, 0))) {
            app.selected_project_id = p.id;
            app.current_screen = Screen::ProjectDetail;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(60, 0))) {
            app.projects.remove(p.id);
            if (app.selected_project_id == p.id) app.selected_project_id.clear();
            app.addStatus("Project deleted");
        }

        ImGui::EndChild();
        ImGui::PopID();
    }
}

// Project detail screen (opened when viewing a specific project)
void renderProjectDetailScreen(App& app) {
    auto project = app.projects.get(app.selected_project_id);
    if (project.id.empty()) {
        ImGui::TextUnformatted("Project not found.");
        if (ImGui::Button("Back to Projects")) {
            app.current_screen = Screen::Projects;
        }
        return;
    }

    ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f), "PROJECT: %s", project.name.c_str());
    if (ImGui::Button("Back")) {
        app.current_screen = Screen::Projects;
    }

    ImGui::SameLine();
    if (app.macro_engine.isRecording()) {
        if (ImGui::Button("Stop Recording", ImVec2(140, 0))) {
            app.macro_engine.stopRecording();
            auto signals = app.macro_engine.getRecordedSignals();
            char buf[128];
            snprintf(buf, sizeof(buf), "Recorded %zu signals", signals.size());
            app.addStatus(buf);

            MacroCommand cmd;
            cmd.name = "Recording " + std::to_string(project.commands.size() + 1);
            cmd.type = "macro";
            cmd.signals = signals;
            app.projects.addCommand(project.id, cmd);
        }
    } else if (!app.macro_engine.isPlaying()) {
        if (ImGui::Button("Start Recording", ImVec2(140, 0))) {
            app.macro_engine.startRecording();
            app.addStatus("Recording to " + project.name);
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Description: %s", project.description.c_str());
    ImGui::Text("Game: %s", project.game.c_str());
    ImGui::Text("Created: %s", project.created_at.c_str());
    ImGui::Text("Commands: %zu", project.commands.size());

    ImGui::Spacing();
    ImGui::Separator();

    // Commands table
    if (ImGui::BeginTable("CommandsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Signals");
        ImGui::TableSetupColumn("Actions");
        ImGui::TableHeadersRow();

        for (auto& cmd : project.commands) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", cmd.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%zu signals", cmd.signals.size());
            ImGui::TableSetColumnIndex(2);

            ImGui::PushID(cmd.id.c_str());
            if (ImGui::Button("Play", ImVec2(60, 0))) {
                app.macro_engine.startPlayback(cmd.signals);
                app.addStatus("Playing: " + cmd.name);
            }
            ImGui::SameLine();
            if (ImGui::Button("Export", ImVec2(60, 0))) {
                std::string py = MacroEngine::exportAsPython(cmd.signals, project.name + "_" + cmd.name);
                app.addStatus("Exported " + std::to_string(cmd.signals.size()) + " signals (see console)");
                printf("=== PYTHON EXPORT ===\n%s\n=== END ===\n", py.c_str());
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete", ImVec2(60, 0))) {
                app.projects.removeCommand(project.id, cmd.id);
                app.addStatus("Command deleted");
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    // Pad visualizer during playback
    if (app.macro_engine.isPlaying()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Playback State:");
        renderPadVisualizer(app.macro_engine.getPlaybackState(), 150.0f);
    }
}

} // namespace ghostpad
