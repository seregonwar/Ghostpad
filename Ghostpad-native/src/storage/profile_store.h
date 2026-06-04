#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "input/keyboard_input.h"

namespace ghostpad {

struct ProfileBindingEntry {
    std::string id;
    std::string name;
    std::vector<KeyBinding> button_bindings;
    StickBindings stick_bindings;
    bool mouse_look_enabled = false;
    float mouse_sensitivity = 3.0f;
    bool auto_clicker_enabled = false;
    int auto_clicker_button_id = 0;
    int auto_clicker_hold_ms = 100;
    int auto_clicker_gap_ms = 500;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ProfileBindingEntry, id, name, button_bindings, stick_bindings, mouse_look_enabled, mouse_sensitivity, auto_clicker_enabled, auto_clicker_button_id, auto_clicker_hold_ms, auto_clicker_gap_ms)
};

class ProfileStore {
public:
    explicit ProfileStore(const std::string& data_dir);
    std::vector<ProfileBindingEntry> list() const;
    ProfileBindingEntry get(const std::string& id) const;
    ProfileBindingEntry add(const ProfileBindingEntry& entry);
    ProfileBindingEntry update(const std::string& id, const ProfileBindingEntry& patch);
    bool remove(const std::string& id);

private:
    std::string file_path_;
    std::vector<ProfileBindingEntry> readAll() const;
    void writeAll(const std::vector<ProfileBindingEntry>& entries) const;
};

} // namespace ghostpad
