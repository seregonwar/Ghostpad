// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "storage/profile_store.h"
#include "input/keyboard_input.h"
#include <fstream>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>

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

ProfileStore::ProfileStore(const std::string& data_dir) {
    std::filesystem::create_directories(data_dir);
    file_path_ = data_dir + "/ghostpad-profiles.json";

    std::ifstream test(file_path_);
    if (!test.is_open()) {
        writeAll({});
    }
}

std::vector<ProfileBindingEntry> ProfileStore::readAll() const {
    std::ifstream file(file_path_);
    if (!file.is_open()) return {};

    try {
        nlohmann::json j;
        file >> j;
        if (j.is_array()) {
            return j.get<std::vector<ProfileBindingEntry>>();
        }
    } catch (...) {}

    return {};
}

void ProfileStore::writeAll(const std::vector<ProfileBindingEntry>& entries) const {
    nlohmann::json j = entries;
    std::ofstream file(file_path_);
    file << j.dump(2);
}

std::vector<ProfileBindingEntry> ProfileStore::list() const {
    return readAll();
}

ProfileBindingEntry ProfileStore::get(const std::string& id) const {
    auto entries = readAll();
    for (const auto& e : entries) {
        if (e.id == id) return e;
    }
    return {};
}

ProfileBindingEntry ProfileStore::add(const ProfileBindingEntry& entry) {
    auto entries = readAll();
    ProfileBindingEntry e = entry;
    e.id = generateUUID();
    entries.push_back(e);
    writeAll(entries);
    return e;
}

ProfileBindingEntry ProfileStore::update(const std::string& id, const ProfileBindingEntry& patch) {
    auto entries = readAll();
    for (auto& e : entries) {
        if (e.id == id) {
            if (!patch.name.empty()) e.name = patch.name;
            if (!patch.button_bindings.empty()) e.button_bindings = patch.button_bindings;
            e.stick_bindings = patch.stick_bindings;
            e.mouse_look_enabled = patch.mouse_look_enabled;
            e.mouse_sensitivity = patch.mouse_sensitivity;
            e.auto_clicker_enabled = patch.auto_clicker_enabled;
            e.auto_clicker_button_id = patch.auto_clicker_button_id;
            e.auto_clicker_hold_ms = patch.auto_clicker_hold_ms;
            e.auto_clicker_gap_ms = patch.auto_clicker_gap_ms;
            writeAll(entries);
            return e;
        }
    }
    return {};
}

bool ProfileStore::remove(const std::string& id) {
    auto entries = readAll();
    auto it = std::remove_if(entries.begin(), entries.end(),
        [&](const ProfileBindingEntry& e) { return e.id == id; });
    if (it != entries.end()) {
        entries.erase(it, entries.end());
        writeAll(entries);
        return true;
    }
    return false;
}

} // namespace ghostpad
