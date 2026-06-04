#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <map>
#include <string>
#include <vector>
#include "protocol/gpad_packet.h"

namespace ghostpad {

struct KeyBinding {
    int button_id = 0;  // PS5 button ID
    int glfw_key = 0;   // GLFW key code
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

struct StickBindings {
    int lx_pos = 0;   // key for LX positive (right)
    int lx_neg = 0;   // key for LX negative (left)
    int ly_pos = 0;   // key for LY positive (down)
    int ly_neg = 0;   // key for LY negative (up)
    int rx_pos = 0;   // key for RX positive (right)
    int rx_neg = 0;   // key for RX negative (left)
    int ry_pos = 0;   // key for RY positive (down)
    int ry_neg = 0;   // key for RY negative (up)
};

struct MouseLookSettings {
    bool enabled = false;
    float sensitivity = 3.0f;
};

struct AutoClickerSettings {
    bool enabled = false;
    int button_id = 0;
    int hold_ms = 100;
    int gap_ms = 500;
    double cps = 0.0;
};

class KeyboardInput {
public:
    KeyboardInput();

    void setKeyPressed(int glfw_key, bool ctrl, bool shift, bool alt, bool pressed);
    PadStateInput getPadState() const;

    // Key bindings management
    void setButtonBinding(int button_id, int glfw_key, bool ctrl = false, bool shift = false, bool alt = false);
    KeyBinding getButtonBinding(int button_id) const;
    std::map<int, KeyBinding> getAllBindings() const;
    void clearBinding(int button_id);

    // Stick bindings
    void setStickBinding(const std::string& direction, int key);
    int getStickBinding(const std::string& direction) const;
    StickBindings getStickBindings() const;

    // Mouse look
    void setMouseLook(bool enabled);
    bool isMouseLookEnabled() const;
    void setSensitivity(float sens);
    float getSensitivity() const;
    void updateMouseDelta(float dx, float dy);
    float getMouseDx() const { return mouse_dx_; }
    float getMouseDy() const { return mouse_dy_; }

    // Auto-clicker
    void setAutoClicker(const AutoClickerSettings& settings);
    AutoClickerSettings getAutoClicker() const;

    // Default bindings (WASD layout)
    void loadDefaultBindings();

private:
    struct KeyState {
        int key = 0;
        int ctrl = 0;
        int shift = 0;
        int alt = 0;
        bool pressed = false;
    };

    std::map<int, KeyBinding> button_bindings_;
    StickBindings stick_bindings_;
    std::map<int, bool> key_states_;
    bool ctrl_held_ = false;
    bool shift_held_ = false;
    bool alt_held_ = false;

    MouseLookSettings mouse_look_;
    float mouse_dx_ = 0.0f;
    float mouse_dy_ = 0.0f;

    AutoClickerSettings auto_clicker_;
};

} // namespace ghostpad
