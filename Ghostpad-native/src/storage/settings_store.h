#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <string>
#include <array>
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
    std::array<float, 4> color = {0.725f, 0.549f, 1.0f, 1.0f};
    std::array<float, 4> secondary_color = {1.0f, 1.0f, 1.0f, 1.0f};
};

inline void to_json(nlohmann::json& j, const ComponentLayout& c) {
    j = nlohmann::json{
        {"x_offset", c.x_offset},
        {"y_offset", c.y_offset},
        {"scale", c.scale},
        {"color", c.color},
        {"secondary_color", c.secondary_color}
    };
}

inline void from_json(const nlohmann::json& j, ComponentLayout& c) {
    c.x_offset = j.value("x_offset", 0.0f);
    c.y_offset = j.value("y_offset", 0.0f);
    c.scale = j.value("scale", 1.0f);
    if (j.contains("color")) {
        c.color = j.at("color").get<std::array<float, 4>>();
    } else {
        c.color = {0.725f, 0.549f, 1.0f, 1.0f};
    }
    if (j.contains("secondary_color")) {
        c.secondary_color = j.at("secondary_color").get<std::array<float, 4>>();
    } else {
        c.secondary_color = {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

//  ===========================================================
//  |                  PAD LAYOUT CONFIGURATION               |
//  ===========================================================
struct PadLayoutSettings {
    ComponentLayout l_stick;
    ComponentLayout r_stick;
    ComponentLayout dpad_up;
    ComponentLayout dpad_down;
    ComponentLayout dpad_left;
    ComponentLayout dpad_right;
    ComponentLayout face_buttons;
    ComponentLayout shoulders_l;
    ComponentLayout shoulders_r;
    ComponentLayout touchpad;
    ComponentLayout create_btn;
    ComponentLayout options_btn;
    ComponentLayout ps_btn;
};

inline void to_json(nlohmann::json& j, const PadLayoutSettings& p) {
    j = nlohmann::json{
        {"l_stick", p.l_stick},
        {"r_stick", p.r_stick},
        {"dpad_up", p.dpad_up},
        {"dpad_down", p.dpad_down},
        {"dpad_left", p.dpad_left},
        {"dpad_right", p.dpad_right},
        {"face_buttons", p.face_buttons},
        {"shoulders_l", p.shoulders_l},
        {"shoulders_r", p.shoulders_r},
        {"touchpad", p.touchpad},
        {"create_btn", p.create_btn},
        {"options_btn", p.options_btn},
        {"ps_btn", p.ps_btn}
    };
}

inline void from_json(const nlohmann::json& j, PadLayoutSettings& p) {
    if (j.contains("l_stick")) j.at("l_stick").get_to(p.l_stick);
    if (j.contains("r_stick")) j.at("r_stick").get_to(p.r_stick);
    
    // Legacy migration check for merged D-pad
    ComponentLayout legacy_dpad;
    bool has_legacy_dpad = j.contains("dpad");
    if (has_legacy_dpad) {
        j.at("dpad").get_to(legacy_dpad);
    }

    if (j.contains("dpad_up")) {
        j.at("dpad_up").get_to(p.dpad_up);
    } else if (has_legacy_dpad) {
        p.dpad_up = legacy_dpad;
    }

    if (j.contains("dpad_down")) {
        j.at("dpad_down").get_to(p.dpad_down);
    } else if (has_legacy_dpad) {
        p.dpad_down = legacy_dpad;
    }

    if (j.contains("dpad_left")) {
        j.at("dpad_left").get_to(p.dpad_left);
    } else if (has_legacy_dpad) {
        p.dpad_left = legacy_dpad;
    }

    if (j.contains("dpad_right")) {
        j.at("dpad_right").get_to(p.dpad_right);
    } else if (has_legacy_dpad) {
        p.dpad_right = legacy_dpad;
    }

    if (j.contains("face_buttons")) j.at("face_buttons").get_to(p.face_buttons);
    if (j.contains("shoulders_l")) j.at("shoulders_l").get_to(p.shoulders_l);
    if (j.contains("shoulders_r")) j.at("shoulders_r").get_to(p.shoulders_r);
    if (j.contains("touchpad")) j.at("touchpad").get_to(p.touchpad);

    // Legacy migration check for merged center buttons
    ComponentLayout legacy_center;
    bool has_legacy_center = j.contains("center_buttons");
    if (has_legacy_center) {
        j.at("center_buttons").get_to(legacy_center);
    }

    if (j.contains("create_btn")) {
        j.at("create_btn").get_to(p.create_btn);
    } else if (has_legacy_center) {
        p.create_btn = legacy_center;
    }

    if (j.contains("options_btn")) {
        j.at("options_btn").get_to(p.options_btn);
    } else if (has_legacy_center) {
        p.options_btn = legacy_center;
    }

    if (j.contains("ps_btn")) {
        j.at("ps_btn").get_to(p.ps_btn);
    } else if (has_legacy_center) {
        p.ps_btn = legacy_center;
    }
}

struct AppSettings {
    std::string payload_elf_path;
    bool auto_deploy_on_connect = true;
    bool auto_bind_via_klog = true;
    bool connect_beep_enabled = false;
    int connect_beep_type = 1;
    PadLayoutSettings pad_layout;
    std::string active_profile_id;
};

inline void to_json(nlohmann::json& j, const AppSettings& s) {
    j = nlohmann::json{
        {"payload_elf_path", s.payload_elf_path},
        {"auto_deploy_on_connect", s.auto_deploy_on_connect},
        {"auto_bind_via_klog", s.auto_bind_via_klog},
        {"connect_beep_enabled", s.connect_beep_enabled},
        {"connect_beep_type", s.connect_beep_type},
        {"pad_layout", s.pad_layout},
        {"active_profile_id", s.active_profile_id}
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
    s.active_profile_id = j.value("active_profile_id", "");
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
