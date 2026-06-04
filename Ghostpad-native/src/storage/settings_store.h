#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.


// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <string>
#include <nlohmann/json.hpp>

namespace ghostpad {

/*
 *  ┌──────────────────────────────────────────────────────────┐
 *  │               CUSTOM BUTTON LAYOUT SETTINGS              │
 *  └──────────────────────────────────────────────────────────┘
 */
struct ComponentLayout {
    float x_offset = 0.0f;
    float y_offset = 0.0f;
    float scale = 1.0f;
};

inline void to_json(nlohmann::json& j, const ComponentLayout& c) {
    j = nlohmann::json{{"x_offset", c.x_offset}, {"y_offset", c.y_offset}, {"scale", c.scale}};
}

inline void from_json(const nlohmann::json& j, ComponentLayout& c) {
    c.x_offset = j.value("x_offset", 0.0f);
    c.y_offset = j.value("y_offset", 0.0f);
    c.scale = j.value("scale", 1.0f);
}

struct PadLayoutSettings {
    ComponentLayout l_stick;
    ComponentLayout r_stick;
    ComponentLayout dpad;
    ComponentLayout face_buttons;
    ComponentLayout shoulders_l;
    ComponentLayout shoulders_r;
    ComponentLayout touchpad;
    ComponentLayout center_buttons;
};

inline void to_json(nlohmann::json& j, const PadLayoutSettings& p) {
    j = nlohmann::json{
        {"l_stick", p.l_stick},
        {"r_stick", p.r_stick},
        {"dpad", p.dpad},
        {"face_buttons", p.face_buttons},
        {"shoulders_l", p.shoulders_l},
        {"shoulders_r", p.shoulders_r},
        {"touchpad", p.touchpad},
        {"center_buttons", p.center_buttons}
    };
}

inline void from_json(const nlohmann::json& j, PadLayoutSettings& p) {
    if (j.contains("l_stick")) j.at("l_stick").get_to(p.l_stick);
    if (j.contains("r_stick")) j.at("r_stick").get_to(p.r_stick);
    if (j.contains("dpad")) j.at("dpad").get_to(p.dpad);
    if (j.contains("face_buttons")) j.at("face_buttons").get_to(p.face_buttons);
    if (j.contains("shoulders_l")) j.at("shoulders_l").get_to(p.shoulders_l);
    if (j.contains("shoulders_r")) j.at("shoulders_r").get_to(p.shoulders_r);
    if (j.contains("touchpad")) j.at("touchpad").get_to(p.touchpad);
    if (j.contains("center_buttons")) j.at("center_buttons").get_to(p.center_buttons);
}

struct AppSettings {
    std::string payload_elf_path;
    bool auto_deploy_on_connect = true;
    bool auto_bind_via_klog = true;
    bool connect_beep_enabled = false;
    int connect_beep_type = 1;
    PadLayoutSettings pad_layout;
};

inline void to_json(nlohmann::json& j, const AppSettings& s) {
    j = nlohmann::json{
        {"payload_elf_path", s.payload_elf_path},
        {"auto_deploy_on_connect", s.auto_deploy_on_connect},
        {"auto_bind_via_klog", s.auto_bind_via_klog},
        {"connect_beep_enabled", s.connect_beep_enabled},
        {"connect_beep_type", s.connect_beep_type},
        {"pad_layout", s.pad_layout}
    };
}

inline void from_json(const nlohmann::json& j, AppSettings& s) {
    s.payload_elf_path = j.value("payload_elf_path", "");
    s.auto_deploy_on_connect = j.value("auto_deploy_on_connect", true);
    s.auto_bind_via_klog = j.value("auto_bind_via_klog", true);
    s.connect_beep_enabled = j.value("connect_beep_enabled", false);
    s.connect_beep_type = j.value("connect_beep_type", 1);
    if (j.contains("pad_layout")) {
        j.at("pad_layout").get_to(s.pad_layout);
    }
}

class SettingsStore {
public:
    explicit SettingsStore(const std::string& data_dir);
    AppSettings read() const;
    AppSettings write(const AppSettings& patch);
    std::string resolvePayloadPath() const;

private:
    std::string file_path_;
    std::string app_root_;
};

} // namespace ghostpad
