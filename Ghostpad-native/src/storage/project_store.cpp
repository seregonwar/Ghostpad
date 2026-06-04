// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "storage/project_store.h"
#include <fstream>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>

namespace ghostpad {

static std::string generateUUID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    uint64_t a = dis(gen);
    uint64_t b = dis(gen);

    oss << std::setw(8) << ((a >> 32) & 0xFFFFFFFF) << "-"
        << std::setw(4) << ((a >> 16) & 0xFFFF) << "-"
        << std::setw(4) << (a & 0xFFFF) << "-"
        << std::setw(4) << ((b >> 48) & 0xFFFF) << "-"
        << std::setw(12) << (b & 0xFFFFFFFFFFFF);

    return oss.str();
}

static std::string nowISO() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

ProjectStore::ProjectStore(const std::string& data_dir) {
    mkdir(data_dir.c_str(), 0755);
    file_path_ = data_dir + "/ghostpad-projects.json";
}

std::vector<Project> ProjectStore::readAll() const {
    std::ifstream file(file_path_);
    if (!file.is_open()) return {};

    try {
        nlohmann::json j;
        file >> j;
        if (j.is_array()) {
            return j.get<std::vector<Project>>();
        }
    } catch (...) {}

    return {};
}

void ProjectStore::writeAll(const std::vector<Project>& entries) const {
    nlohmann::json j = entries;
    std::ofstream file(file_path_);
    file << j.dump(2);
}

std::vector<Project> ProjectStore::list() const {
    return readAll();
}

Project ProjectStore::get(const std::string& id) const {
    auto entries = readAll();
    for (auto& e : entries) {
        if (e.id == id) return e;
    }
    return {};
}

Project ProjectStore::add(const std::string& name, const std::string& description, const std::string& game) {
    auto entries = readAll();
    Project p;
    p.id = generateUUID();
    p.name = name;
    p.description = description;
    p.game = game;
    p.created_at = nowISO();
    p.updated_at = p.created_at;
    entries.push_back(p);
    writeAll(entries);
    return p;
}

Project ProjectStore::update(const std::string& id, const Project& patch) {
    auto entries = readAll();
    for (auto& e : entries) {
        if (e.id == id) {
            if (!patch.name.empty()) e.name = patch.name;
            if (!patch.description.empty()) e.description = patch.description;
            if (!patch.game.empty()) e.game = patch.game;
            e.updated_at = nowISO();
            writeAll(entries);
            return e;
        }
    }
    return {};
}

bool ProjectStore::remove(const std::string& id) {
    auto entries = readAll();
    auto it = std::remove_if(entries.begin(), entries.end(),
        [&](const Project& e) { return e.id == id; });
    if (it != entries.end()) {
        entries.erase(it, entries.end());
        writeAll(entries);
        return true;
    }
    return false;
}

bool ProjectStore::addCommand(const std::string& project_id, const MacroCommand& cmd) {
    auto entries = readAll();
    for (auto& e : entries) {
        if (e.id == project_id) {
            MacroCommand c = cmd;
            if (c.id.empty()) c.id = generateUUID();
            e.commands.push_back(c);
            e.updated_at = nowISO();
            writeAll(entries);
            return true;
        }
    }
    return false;
}

/*
 *  +--------------------------------------------------------+
 *  |                 UPDATE MACRO SIGNAL LIST               |
 *  +--------------------------------------------------------+
 */
bool ProjectStore::updateCommand(const std::string& project_id, const MacroCommand& cmd) {
    auto entries = readAll();
    for (auto& e : entries) {
        if (e.id == project_id) {
            for (auto& c : e.commands) {
                if (c.id == cmd.id) {
                    c = cmd;
                    e.updated_at = nowISO();
                    writeAll(entries);
                    return true;
                }
            }
        }
    }
    return false;
}

bool ProjectStore::removeCommand(const std::string& project_id, const std::string& cmd_id) {
    auto entries = readAll();
    for (auto& e : entries) {
        if (e.id == project_id) {
            auto it = std::remove_if(e.commands.begin(), e.commands.end(),
                [&](const MacroCommand& c) { return c.id == cmd_id; });
            if (it != e.commands.end()) {
                e.commands.erase(it, e.commands.end());
                e.updated_at = nowISO();
                writeAll(entries);
                return true;
            }
        }
    }
    return false;
}

std::string ProjectStore::exportJson(const std::string& id) const {
    Project p = get(id);
    if (p.id.empty()) return "";
    nlohmann::json j = p;
    return j.dump(2);
}

bool ProjectStore::importJson(const std::string& json_str) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_str);
        if (j.is_object()) {
            Project p = j.get<Project>();
            p.id = generateUUID();
            p.created_at = nowISO();
            p.updated_at = p.created_at;
            auto entries = readAll();
            entries.push_back(p);
            writeAll(entries);
            return true;
        } else if (j.is_array()) {
            auto entries = readAll();
            for (auto& item : j) {
                Project p = item.get<Project>();
                p.id = generateUUID();
                p.created_at = nowISO();
                p.updated_at = p.created_at;
                entries.push_back(p);
            }
            writeAll(entries);
            return true;
        }
    } catch (...) {}
    return false;
}

} // namespace ghostpad
