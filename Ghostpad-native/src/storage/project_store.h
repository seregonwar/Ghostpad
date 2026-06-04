#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ghostpad {

struct MacroSignal {
    int button_id = 0;
    int value = 255;
    int time_ms = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MacroSignal, button_id, value, time_ms)
};

struct MacroCommand {
    std::string id;
    std::string name;
    std::string type;
    std::vector<MacroSignal> signals;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MacroCommand, id, name, type, signals)
};

struct Project {
    std::string id;
    std::string name;
    std::string description;
    std::vector<MacroCommand> commands;
    std::string game;
    std::string created_at;
    std::string updated_at;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Project, id, name, description, commands, game, created_at, updated_at)
};

class ProjectStore {
public:
    explicit ProjectStore(const std::string& data_dir);
    std::vector<Project> list() const;
    Project get(const std::string& id) const;
    Project add(const std::string& name, const std::string& description, const std::string& game);
    Project update(const std::string& id, const Project& patch);
    bool remove(const std::string& id);
    bool addCommand(const std::string& project_id, const MacroCommand& cmd);
    bool updateCommand(const std::string& project_id, const MacroCommand& cmd);
    bool removeCommand(const std::string& project_id, const std::string& cmd_id);
    std::string exportJson(const std::string& id) const;
    bool importJson(const std::string& json_str);

private:
    std::string file_path_;
    std::vector<Project> readAll() const;
    void writeAll(const std::vector<Project>& entries) const;
};

} // namespace ghostpad
