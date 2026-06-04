// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "ui/gif_export.h"
#include "imgui.h"
#include <cstdio>
#include <memory>
#include <array>
#include <thread>
#include <atomic>
#include <cstring>
#include <fstream>

namespace ghostpad {

extern void renderPadVisualizer(App& app, const PadStateInput& state, float size);

static std::string execCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

static void browseFilePath(char* buf, size_t bufsz, const char* prompt, const char* types) {
    char cmd[512];
    if (types && types[0]) {
        snprintf(cmd, sizeof(cmd), "osascript -e 'POSIX path of (choose file with prompt \"%s\" of type {\"%s\"})' 2>/dev/null", prompt, types);
    } else {
        snprintf(cmd, sizeof(cmd), "osascript -e 'POSIX path of (choose file with prompt \"%s\")' 2>/dev/null", prompt);
    }
    std::string res = execCommand(cmd);
    if (!res.empty()) {
        strncpy(buf, res.c_str(), bufsz - 1);
        buf[bufsz - 1] = '\0';
    }
}

static void browseSavePath(char* buf, size_t bufsz, const char* prompt, const char* defName) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "osascript -e 'POSIX path of (choose file name with prompt \"%s\" default name \"%s\")' 2>/dev/null", prompt, defName);
    std::string res = execCommand(cmd);
    if (!res.empty()) {
        strncpy(buf, res.c_str(), bufsz - 1);
        buf[bufsz - 1] = '\0';
    }
}

static bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << content;
    return out.good();
}

static bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path);
    if (!in.is_open()) return false;
    out.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return true;
}

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

    ImGui::SameLine();
    if (ui::softButton(ICON_FA_FILE_IMPORT "  Import Project", ImVec2(160, 34)))
        ImGui::OpenPopup("ImportProjectPopup");

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

    // Import Project popup
    if (ImGui::BeginPopupModal("ImportProjectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(p.primary2, "%s  Import Macro Project", ICON_FA_FILE_IMPORT);
        ImGui::Separator();
        ImGui::Spacing();

        static char importPath[512] = {};
        static std::string importPreview;
        static bool pathHasChanged = false;

        ImGui::TextColored(p.muted, "JSON File Path:");
        ImGui::SetNextItemWidth(380);
        if (ImGui::InputText("##ImportPath", importPath, sizeof(importPath))) {
            pathHasChanged = true;
        }
        ImGui::SameLine();
        if (ui::softButton("Browse...", ImVec2(80, 28))) {
            browseFilePath(importPath, sizeof(importPath), "Select macro project JSON file", "public.json");
            pathHasChanged = true;
        }

        if (pathHasChanged && strlen(importPath) > 0) {
            std::string content;
            if (readFile(importPath, content)) {
                importPreview = content;
            } else {
                importPreview = "(Could not read file)";
            }
            pathHasChanged = false;
        }

        if (!importPreview.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(p.muted, "Preview (%zu bytes):", importPreview.size());
            ImGui::PushStyleColor(ImGuiCol_ChildBg, p.bg1);
            ImGui::BeginChild("PreviewBox", ImVec2(460, 100), true);
            ImGui::TextWrapped("%s", importPreview.substr(0, 600).c_str());
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        if (ui::primaryButton(ICON_FA_CHECK "  Import", ImVec2(110, 32))) {
            if (!importPreview.empty() && app.projects.importJson(importPreview)) {
                app.addStatus("Project imported successfully");
                importPreview.clear();
                importPath[0] = '\0';
                ImGui::CloseCurrentPopup();
            } else {
                app.addStatus("Failed to import project", true);
            }
        }
        ImGui::SameLine();
        if (ui::softButton(ICON_FA_XMARK "  Cancel", ImVec2(110, 32))) {
            importPreview.clear();
            importPath[0] = '\0';
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

            ImGui::SameLine(avail_w - 280);
            ImGui::SetCursorPosY(11);
            if (ui::primaryButton(ICON_FA_FOLDER_OPEN "  Open", ImVec2(80, 30))) {
                app.selected_project_id = proj.id;
                app.current_screen = Screen::ProjectDetail;
            }
            ImGui::SameLine();
            if (ui::softButton(ICON_FA_FILE_EXPORT, ImVec2(36, 30))) {
                std::string json = app.projects.exportJson(proj.id);
                if (!json.empty()) {
                    char savePath[512] = {};
                    snprintf(savePath, sizeof(savePath), "%s.json", proj.name.c_str());
                    browseSavePath(savePath, sizeof(savePath), "Export Project JSON", savePath);
                    if (savePath[0] && writeFile(savePath, json)) {
                        app.addStatus("Exported to " + std::string(savePath));
                    }
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Export JSON");
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

/*
 *  +--------------------------------------------------------+
 *  |           IMPORT SIGNALS POPUP (shared)                |
 *  +--------------------------------------------------------+
 */
static void renderImportSignalsPopup(App& app, std::vector<MacroSignal>& targetSignals) {
    const auto& p = ui::colors();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("ImportSignalsPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(p.primary2, "%s  Import Signals", ICON_FA_FILE_IMPORT);
        ImGui::Separator();
        ImGui::Spacing();

        static char pathBuf[512] = {};
        static std::vector<MacroSignal> parsedSignals;
        static bool parsed = false;

        ImGui::TextColored(p.muted, "File Path (.json or .py):");
        ImGui::SetNextItemWidth(380);
        ImGui::InputText("##ImportSigPath", pathBuf, sizeof(pathBuf));
        ImGui::SameLine();
        if (ui::softButton("Browse...", ImVec2(80, 28))) {
            browseFilePath(pathBuf, sizeof(pathBuf), "Select macro signals file", "public.json");
        }

        ImGui::Spacing();
        if (ui::primaryButton("Parse JSON", ImVec2(130, 30))) {
            std::string content;
            if (readFile(pathBuf, content)) {
                parsedSignals = MacroEngine::importSignalsFromJson(content);
                parsed = true;
                app.addStatus("Parsed " + std::to_string(parsedSignals.size()) + " signals from JSON");
            } else {
                app.addStatus("Failed to read file", true);
            }
        }
        ImGui::SameLine();
        if (ui::softButton("Parse Python", ImVec2(130, 30))) {
            std::string content;
            if (readFile(pathBuf, content)) {
                parsedSignals = MacroEngine::importSignalsFromPython(content);
                parsed = true;
                app.addStatus("Parsed " + std::to_string(parsedSignals.size()) + " signals from Python");
            } else {
                app.addStatus("Failed to read file", true);
            }
        }

        if (parsed) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(p.success, "Found %zu signals. Duration: %.2fs",
                parsedSignals.size(),
                parsedSignals.empty() ? 0.0 : parsedSignals.back().time_ms / 1000.0);
            ImGui::Spacing();

            if (ui::primaryButton(ICON_FA_CHECK "  Import into Timeline", ImVec2(200, 32))) {
                int lastTime = targetSignals.empty() ? 0 : targetSignals.back().time_ms;
                for (auto& sig : parsedSignals) {
                    MacroSignal s = sig;
                    s.time_ms += lastTime;
                    targetSignals.push_back(s);
                }
                app.addStatus("Imported " + std::to_string(parsedSignals.size()) + " signals");
                parsedSignals.clear();
                parsed = false;
                pathBuf[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::Spacing();
        if (ui::softButton(ICON_FA_XMARK "  Cancel", ImVec2(100, 28))) {
            parsedSignals.clear();
            parsed = false;
            pathBuf[0] = '\0';
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

/*
 *  +--------------------------------------------------------+
 *  |      PYTHON EXPORT POPUP (shared)                      |
 *  +--------------------------------------------------------+
 */
static void renderPythonExportPopup(App& app, const std::string& pyCode, const std::string& defName) {
    const auto& p = ui::colors();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("PythonExportPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(p.primary2, "%s  Python Export", ICON_FA_FILE_EXPORT);
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, p.bg1);
        ImGui::BeginChild("PyPreview", ImVec2(500, 180), true);
        ImGui::TextWrapped("%s", pyCode.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing();
        static char savePath2[512] = {};
        if (savePath2[0] == '\0') {
            snprintf(savePath2, sizeof(savePath2), "%s.py", defName.c_str());
        }

        ImGui::TextColored(p.muted, "Save to:");
        ImGui::SetNextItemWidth(380);
        ImGui::InputText("##PySavePath2", savePath2, sizeof(savePath2));
        ImGui::SameLine();
        if (ui::softButton("Browse...", ImVec2(80, 28))) {
            browseSavePath(savePath2, sizeof(savePath2), "Save Python Script As", savePath2);
        }

        ImGui::Spacing();
        if (ui::primaryButton(ICON_FA_CHECK "  Save to File", ImVec2(140, 32))) {
            if (savePath2[0] && writeFile(savePath2, pyCode)) {
                app.addStatus("Saved to " + std::string(savePath2));
                savePath2[0] = '\0';
                ImGui::CloseCurrentPopup();
            } else {
                app.addStatus("Failed to save file", true);
            }
        }
        ImGui::SameLine();
        if (ui::softButton(ICON_FA_XMARK "  Close", ImVec2(100, 32))) {
            savePath2[0] = '\0';
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

/*
 *  +--------------------------------------------------------+
 *  |           VISUAL MACRO SIGNAL TIMELINE EDITOR          |
 *  +--------------------------------------------------------+
 */
static void renderSignalEditor(App& app, const Project& proj) {
    const auto& p = ui::colors();
    float avail_w = ImGui::GetContentRegionAvail().x;

    static std::string last_edited_cmd_id;
    static MacroCommand editing_cmd;
    static char name_buf[128] = {};

    // Load command if selected command ID changed
    if (app.selected_command_id != last_edited_cmd_id) {
        last_edited_cmd_id = app.selected_command_id;
        bool found = false;
        for (auto& c : proj.commands) {
            if (c.id == app.selected_command_id) {
                editing_cmd = c;
                strncpy(name_buf, editing_cmd.name.c_str(), sizeof(name_buf) - 1);
                found = true;
                break;
            }
        }
        if (!found) {
            app.selected_command_id.clear();
            last_edited_cmd_id.clear();
            return;
        }
    }

    if (ui::softButton(ICON_FA_ARROW_LEFT "  Back to Commands", ImVec2(180, 30))) {
        app.selected_command_id.clear();
        last_edited_cmd_id.clear();
        return;
    }

    ImGui::SameLine();
    ImGui::TextColored(p.primary2, "|   %s  %s   |   Timeline Editor", ICON_FA_FOLDER_OPEN, proj.name.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ui::beginCard("EditorCard", ImVec2(avail_w, 0));
    ui::sectionLabel("Command Timeline Editor", ICON_FA_PEN_TO_SQUARE);
    ImGui::Spacing();

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(p.muted, "Macro Name:");
    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    if (ImGui::InputText("##MacroName", name_buf, sizeof(name_buf))) {
        editing_cmd.name = name_buf;
    }
    ImGui::PopItemWidth();

    ImGui::SameLine(avail_w - 660);
    if (ui::primaryButton(ICON_FA_CHECK "  Save Changes", ImVec2(130, 32))) {
        if (app.projects.updateCommand(proj.id, editing_cmd)) {
            app.addStatus("Macro command updated successfully");
        } else {
            app.addStatus("Failed to update macro command", true);
        }
    }
    ImGui::SameLine();
    if (ui::softButton(ICON_FA_PLAY "  Play Preview", ImVec2(130, 32))) {
        app.macro_engine.startPlayback(editing_cmd.signals);
        app.addStatus("Playing preview: " + editing_cmd.name);
    }
    ImGui::SameLine();
    if (ui::softButton(ICON_FA_ARROW_UP_1_9 "  Sort by Time", ImVec2(130, 32))) {
        editing_cmd.signals = MacroEngine::normalizeSignals(editing_cmd.signals);
        app.addStatus("Signals sorted chronologically");
    }
    ImGui::SameLine();
    if (ui::softButton(ICON_FA_PLUS "  Add Step", ImVec2(110, 32))) {
        MacroSignal s;
        s.button_id = 0;
        s.value = 255;
        s.time_ms = editing_cmd.signals.empty() ? 0 : editing_cmd.signals.back().time_ms + 100;
        editing_cmd.signals.push_back(s);
        app.addStatus("Added timeline step");
    }
    ImGui::SameLine();
    if (ui::softButton(ICON_FA_FILE_IMPORT "  Import Signals", ImVec2(140, 32))) {
        ImGui::OpenPopup("ImportSignalsPopup");
    }
    renderImportSignalsPopup(app, editing_cmd.signals);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    static const char* CONTROL_NAMES[] = {
        "Button: Cross (X)",
        "Button: Circle (O)",
        "Button: Square ([])",
        "Button: Triangle (/\\)",
        "Button: L1",
        "Button: R1",
        "Button: L2 (Digital)",
        "Button: R2 (Digital)",
        "Button: Create",
        "Button: Options",
        "Button: L3",
        "Button: R3",
        "Button: D-pad Up",
        "Button: D-pad Down",
        "Button: D-pad Left",
        "Button: D-pad Right",
        "Button: PS Home",
        "Button: Touchpad",
        "Stick: Left X",
        "Stick: Left Y",
        "Stick: Right X",
        "Stick: Right Y",
        "Trigger: L2 (Analog)",
        "Trigger: R2 (Analog)"
    };

    if (editing_cmd.signals.empty()) {
        ImGui::TextColored(p.muted, "No signals in this macro timeline. Click 'Add Step' to add one!");
    } else if (ImGui::BeginTable("SignalTimeline", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 320))) {
        ImGui::TableSetupColumn("Step", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Timestamp (ms)", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Target Input Control", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Value / State", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        int del_idx = -1;
        for (size_t i = 0; i < editing_cmd.signals.size(); i++) {
            auto& sig = editing_cmd.signals[i];
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(i));

            // Column 0: Index
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("#%zu", i + 1);

            // Column 1: Timestamp
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(120);
            ImGui::InputInt("##Time", &sig.time_ms, 10, 100);
            if (sig.time_ms < 0) sig.time_ms = 0;
            ImGui::PopItemWidth();

            // Column 2: Target Control
            ImGui::TableSetColumnIndex(2);
            int current_control = sig.button_id;
            if (current_control < 0 || current_control > 23) current_control = 0;
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo("##Control", &current_control, CONTROL_NAMES, 24)) {
                sig.button_id = current_control;
                if (current_control < 18) {
                    sig.value = (sig.value > 0) ? 255 : 0;
                } else if (current_control >= 18 && current_control <= 21) {
                    if (sig.value == 0 || sig.value == 255) {
                        sig.value = 128;
                    }
                }
            }
            ImGui::PopItemWidth();

            // Column 3: Value/State
            ImGui::TableSetColumnIndex(3);
            if (sig.button_id < 18) {
                bool is_pressed = (sig.value > 0);
                if (ImGui::Checkbox("Pressed", &is_pressed)) {
                    sig.value = is_pressed ? 255 : 0;
                }
            } else if (sig.button_id >= 18 && sig.button_id <= 21) {
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 65.0f);
                ImGui::SliderInt("##Val", &sig.value, 0, 255);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                if (ui::softButton("Center", ImVec2(55, 24))) {
                    sig.value = 128;
                }
            } else {
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 65.0f);
                ImGui::SliderInt("##Val", &sig.value, 0, 255);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                if (ui::softButton("Zero", ImVec2(55, 24))) {
                    sig.value = 0;
                }
            }

            // Column 4: Delete button
            ImGui::TableSetColumnIndex(4);
            if (ui::dangerButton(ICON_FA_TRASH "  Del", ImVec2(70, 24))) {
                del_idx = static_cast<int>(i);
            }

            ImGui::PopID();
        }

        if (del_idx >= 0) {
            editing_cmd.signals.erase(editing_cmd.signals.begin() + del_idx);
            app.addStatus("Signal removed from timeline");
        }

        ImGui::EndTable();
    }

    if (app.macro_engine.isPlaying()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(p.success, "%s  Playing Preview...", ICON_FA_PLAY);
        renderPadVisualizer(app, app.macro_engine.getPlaybackState(), 120.0f);
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

    // Redirect to Timeline Signal Editor if a command is selected
    if (!app.selected_command_id.empty()) {
        renderSignalEditor(app, proj);
        return;
    }

    if (ui::softButton(ICON_FA_ARROW_LEFT "  Back", ImVec2(100, 30))) app.current_screen = Screen::Projects;
    ImGui::SameLine();
    ImGui::TextColored(p.primary2, "%s  %s", ICON_FA_FOLDER_OPEN, proj.name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(p.muted, "|   %s  %s   |   %zu commands", ICON_FA_GAMEPAD, proj.game.c_str(), proj.commands.size());

    bool rec = app.macro_engine.isRecording();
    bool play = app.macro_engine.isPlaying();

    ImGui::SameLine(avail_w - 500);
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

    ImGui::SameLine();
    if (ui::softButton(ICON_FA_FILE_EXPORT "  Export Project JSON", ImVec2(170, 30))) {
        std::string json = app.projects.exportJson(proj.id);
        if (!json.empty()) {
            char savePath[512] = {};
            snprintf(savePath, sizeof(savePath), "%s.json", proj.name.c_str());
            browseSavePath(savePath, sizeof(savePath), "Export Project JSON", savePath);
            if (savePath[0] && writeFile(savePath, json)) {
                app.addStatus("Exported to " + std::string(savePath));
            }
        }
    }

    ImGui::Spacing();

    // Commands table
    ui::beginCard("CmdTable", ImVec2(avail_w, 0));
    ui::sectionLabel("Project Commands", ICON_FA_LIST);
    ImGui::Spacing();

    // Import signals button at top of commands section
    if (ui::softButton(ICON_FA_FILE_IMPORT "  Import Signals to New Command", ImVec2(240, 30))) {
        ImGui::OpenPopup("ImportSignalsPopup");
    }

    ImGui::Spacing();
    
    if (proj.commands.empty()) {
        ImGui::TextColored(p.muted, "No commands recorded for this project. Start recording above! Or import signals from a file.");
    } else {
        static std::string s_pyExportCode;
        static std::string s_pyExportName;

        ImGui::BeginTable("Cmds", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
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
            
            if (ui::primaryButton(ICON_FA_PLAY, ImVec2(32, 26))) {
                app.macro_engine.startPlayback(cmd.signals);
                app.addStatus("Playing: " + cmd.name);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play Macro");
            ImGui::SameLine();
            
            if (ui::softButton(ICON_FA_PEN_TO_SQUARE, ImVec2(32, 26))) {
                app.selected_command_id = cmd.id;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Edit Timeline");
            ImGui::SameLine();
            
            if (ui::softButton(ICON_FA_FILE_EXPORT, ImVec2(32, 26))) {
                s_pyExportCode = MacroEngine::exportAsPython(cmd.signals, proj.name + "_" + cmd.name);
                s_pyExportName = proj.name + "_" + cmd.name;
                ImGui::OpenPopup("PythonExportPopup");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Export to Python");
            ImGui::SameLine();
            
            if (ui::softButton(ICON_FA_FILE_VIDEO, ImVec2(32, 26))) {
                if (!app.isGifExportActive()) {
                    char savePath[512] = {};
                    std::string defName = proj.name + "_" + cmd.name + ".gif";
                    snprintf(savePath, sizeof(savePath), "%s", defName.c_str());
                    browseSavePath(savePath, sizeof(savePath), "Export Macro as GIF", savePath);
                    if (savePath[0]) {
                        app.macro_engine.startPlayback(cmd.signals);
                        app.startGifExport(std::string(savePath), 180.0f, 30);
                        app.addStatus("Exporting GIF: " + cmd.name);
                    }
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Export as GIF");
            ImGui::SameLine();
            
            if (ui::dangerButton(ICON_FA_TRASH, ImVec2(32, 26))) {
                app.projects.removeCommand(proj.id, cmd.id);
                app.addStatus("Command deleted");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete Command");
            ImGui::PopID();
        }
        ImGui::EndTable();

        renderPythonExportPopup(app, s_pyExportCode, s_pyExportName);
    }

    // Import signals popup for new command
    {
        static std::vector<MacroSignal> s_newCmdSignals;
        if (!s_newCmdSignals.empty()) {
            MacroCommand importedCmd;
            importedCmd.name = "Imported " + std::to_string(proj.commands.size() + 1);
            importedCmd.type = "macro";
            importedCmd.signals = s_newCmdSignals;
            app.projects.addCommand(proj.id, importedCmd);
            app.addStatus("Imported " + std::to_string(s_newCmdSignals.size()) + " signals as new command");
            s_newCmdSignals.clear();
        }
        renderImportSignalsPopup(app, s_newCmdSignals);
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
