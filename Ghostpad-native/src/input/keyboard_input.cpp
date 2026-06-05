// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "input/keyboard_input.h"
#include "storage/profile_store.h"
#include <algorithm>
#include <cmath>
#include <GLFW/glfw3.h>

namespace ghostpad {

KeyboardInput::KeyboardInput() {
    loadDefaultBindings();
}

void KeyboardInput::loadDefaultBindings() {
    // Face buttons: IJKL
    setButtonBinding(2, GLFW_KEY_I);  // Square
    setButtonBinding(3, GLFW_KEY_J);  // Triangle
    setButtonBinding(1, GLFW_KEY_K);  // Circle
    setButtonBinding(0, GLFW_KEY_L);  // Cross

    // Shoulder buttons
    setButtonBinding(4, GLFW_KEY_Q);  // L1
    setButtonBinding(5, GLFW_KEY_E);  // R1
    setButtonBinding(6, GLFW_KEY_U);  // L2
    setButtonBinding(7, GLFW_KEY_O);  // R2

    // D-pad: Arrow keys
    setButtonBinding(12, GLFW_KEY_UP);     // D-pad Up
    setButtonBinding(13, GLFW_KEY_DOWN);   // D-pad Down
    setButtonBinding(14, GLFW_KEY_LEFT);   // D-pad Left
    setButtonBinding(15, GLFW_KEY_RIGHT);  // D-pad Right

    // Center buttons
    setButtonBinding(8, GLFW_KEY_TAB);        // Create
    setButtonBinding(9, GLFW_KEY_BACKSPACE);  // Options
    setButtonBinding(10, GLFW_KEY_LEFT_SHIFT); // L3
    setButtonBinding(11, GLFW_KEY_RIGHT_SHIFT); // R3
    setButtonBinding(16, GLFW_KEY_ESCAPE);    // PS
    setButtonBinding(17, GLFW_KEY_T);         // Touchpad

    // Left stick: WASD
    stick_bindings_.lx_pos = GLFW_KEY_D;
    stick_bindings_.lx_neg = GLFW_KEY_A;
    stick_bindings_.ly_pos = GLFW_KEY_S;
    stick_bindings_.ly_neg = GLFW_KEY_W;

    // Right stick: arrows numpad
    stick_bindings_.rx_pos = GLFW_KEY_RIGHT;
    stick_bindings_.rx_neg = GLFW_KEY_LEFT;
    stick_bindings_.ry_pos = GLFW_KEY_DOWN;
    stick_bindings_.ry_neg = GLFW_KEY_UP;

    mouse_look_.enabled = false;
    mouse_look_.sensitivity = 3.0f;
}

void KeyboardInput::update(double dt_ms) {
    if (!auto_clicker_.enabled) {
        clicker_timer_ = 0.0;
        clicker_state_pressed_ = false;
        return;
    }

    // Is the key bound to the target button_id held down?
    auto it = button_bindings_.find(auto_clicker_.button_id);
    bool trigger_held = false;
    if (it != button_bindings_.end()) {
        auto it_state = key_states_.find(it->second.glfw_key);
        trigger_held = (it_state != key_states_.end() && it_state->second);
    }

    if (!trigger_held) {
        clicker_timer_ = 0.0;
        clicker_state_pressed_ = false;
        return;
    }

    clicker_timer_ += dt_ms;
    double threshold = clicker_state_pressed_ ? auto_clicker_.hold_ms : auto_clicker_.gap_ms;
    if (clicker_timer_ >= threshold) {
        clicker_timer_ -= threshold;
        clicker_state_pressed_ = !clicker_state_pressed_;
    }
}

void KeyboardInput::setKeyPressed(int glfw_key, bool ctrl, bool shift, bool alt, bool pressed) {
    key_states_[glfw_key] = pressed;
    ctrl_held_ = ctrl;
    shift_held_ = shift;
    alt_held_ = alt;
}

void KeyboardInput::setButtonBinding(int button_id, int glfw_key, bool ctrl, bool shift, bool alt) {
    button_bindings_[button_id] = {button_id, glfw_key, ctrl, shift, alt};
}

KeyBinding KeyboardInput::getButtonBinding(int button_id) const {
    auto it = button_bindings_.find(button_id);
    if (it != button_bindings_.end()) return it->second;
    return {};
}

const std::map<int, KeyBinding>& KeyboardInput::getAllBindings() const {
    return button_bindings_;
}

void KeyboardInput::clearBinding(int button_id) {
    button_bindings_.erase(button_id);
}

void KeyboardInput::setStickBinding(const std::string& direction, int key) {
    if (direction == "lx_pos") stick_bindings_.lx_pos = key;
    else if (direction == "lx_neg") stick_bindings_.lx_neg = key;
    else if (direction == "ly_pos") stick_bindings_.ly_pos = key;
    else if (direction == "ly_neg") stick_bindings_.ly_neg = key;
    else if (direction == "rx_pos") stick_bindings_.rx_pos = key;
    else if (direction == "rx_neg") stick_bindings_.rx_neg = key;
    else if (direction == "ry_pos") stick_bindings_.ry_pos = key;
    else if (direction == "ry_neg") stick_bindings_.ry_neg = key;
}

int KeyboardInput::getStickBinding(const std::string& direction) const {
    if (direction == "lx_pos") return stick_bindings_.lx_pos;
    if (direction == "lx_neg") return stick_bindings_.lx_neg;
    if (direction == "ly_pos") return stick_bindings_.ly_pos;
    if (direction == "ly_neg") return stick_bindings_.ly_neg;
    if (direction == "rx_pos") return stick_bindings_.rx_pos;
    if (direction == "rx_neg") return stick_bindings_.rx_neg;
    if (direction == "ry_pos") return stick_bindings_.ry_pos;
    if (direction == "ry_neg") return stick_bindings_.ry_neg;
    return 0;
}

const StickBindings& KeyboardInput::getStickBindings() const {
    return stick_bindings_;
}

void KeyboardInput::setMouseLook(bool enabled) {
    mouse_look_.enabled = enabled;
}

bool KeyboardInput::isMouseLookEnabled() const {
    return mouse_look_.enabled;
}

void KeyboardInput::setSensitivity(float sens) {
    mouse_look_.sensitivity = std::max(0.1f, std::min(sens, 50.0f));
}

float KeyboardInput::getSensitivity() const {
    return mouse_look_.sensitivity;
}

void KeyboardInput::updateMouseDelta(float dx, float dy) {
    mouse_dx_ = dx;
    mouse_dy_ = dy;
}

void KeyboardInput::setAutoClicker(const AutoClickerSettings& settings) {
    auto_clicker_ = settings;
}

const AutoClickerSettings& KeyboardInput::getAutoClicker() const {
    return auto_clicker_;
}

static bool isKeyHeld(const std::map<int, bool>& key_states, int key) {
    if (key == 0) return false;
    auto it = key_states.find(key);
    return it != key_states.end() && it->second;
}

PadStateInput KeyboardInput::getPadState() const {
    PadStateInput pad = {};

    // Button bindings
    for (const auto& [id, binding] : button_bindings_) {
        bool match = isKeyHeld(key_states_, binding.glfw_key);
        if (match) {
            // Apply auto-clicker state override if active and matched
            if (auto_clicker_.enabled && id == auto_clicker_.button_id) {
                pad.button_states[id] = clicker_state_pressed_;
            } else {
                pad.button_states[id] = true;
            }
        }
    }

    // Stick bindings
    constexpr int CENTER = 128;
    constexpr int MAX = 255;
    constexpr int MIN = 0;

    int lx = CENTER, ly = CENTER, rx = CENTER, ry = CENTER;

    if (isKeyHeld(key_states_, stick_bindings_.lx_pos)) lx = MAX;
    if (isKeyHeld(key_states_, stick_bindings_.lx_neg)) lx = MIN;
    if (isKeyHeld(key_states_, stick_bindings_.ly_pos)) ly = MAX;
    if (isKeyHeld(key_states_, stick_bindings_.ly_neg)) ly = MIN;

    if (isKeyHeld(key_states_, stick_bindings_.rx_pos)) rx = MAX;
    if (isKeyHeld(key_states_, stick_bindings_.rx_neg)) rx = MIN;
    if (isKeyHeld(key_states_, stick_bindings_.ry_pos)) ry = MAX;
    if (isKeyHeld(key_states_, stick_bindings_.ry_neg)) ry = MIN;

    // Mouse look override for right stick
    if (mouse_look_.enabled && (mouse_dx_ != 0.0f || mouse_dy_ != 0.0f)) {
        rx = static_cast<int>(std::clamp(CENTER + mouse_dx_ * mouse_look_.sensitivity * 10.0f,
                                         float(MIN), float(MAX)));
        ry = static_cast<int>(std::clamp(CENTER + mouse_dy_ * mouse_look_.sensitivity * 10.0f,
                                         float(MIN), float(MAX)));
        // Decay/reset mouse deltas so they don't stick forever
        mouse_dx_ = 0.0f;
        mouse_dy_ = 0.0f;
    }

    // Trigger keys (L2/R2 as analog values)
    if (isKeyHeld(key_states_, stick_bindings_.lx_neg) && isKeyHeld(key_states_, stick_bindings_.lx_pos)) {
        lx = CENTER;
    }
    if (isKeyHeld(key_states_, stick_bindings_.ly_neg) && isKeyHeld(key_states_, stick_bindings_.ly_pos)) {
        ly = CENTER;
    }

    pad.stick_states[0] = static_cast<uint8_t>(lx);
    pad.stick_states[1] = static_cast<uint8_t>(ly);
    pad.stick_states[2] = static_cast<uint8_t>(rx);
    pad.stick_states[3] = static_cast<uint8_t>(ry);

    // Trigger analog values from button bindings
    auto it_l2 = button_bindings_.find(6);
    auto it_r2 = button_bindings_.find(7);
    pad.trigger_l2 = (it_l2 != button_bindings_.end() && isKeyHeld(key_states_, it_l2->second.glfw_key)) ? 255 : 0;
    pad.trigger_r2 = (it_r2 != button_bindings_.end() && isKeyHeld(key_states_, it_r2->second.glfw_key)) ? 255 : 0;

    return pad;
}

void KeyboardInput::loadFromProfile(const ProfileBindingEntry& profile) {
    button_bindings_.clear();
    for (const auto& b : profile.button_bindings) {
        button_bindings_[b.button_id] = b;
    }
    stick_bindings_ = profile.stick_bindings;
    mouse_look_.enabled = profile.mouse_look_enabled;
    mouse_look_.sensitivity = profile.mouse_sensitivity;
    auto_clicker_.enabled = profile.auto_clicker_enabled;
    auto_clicker_.button_id = profile.auto_clicker_button_id;
    auto_clicker_.hold_ms = profile.auto_clicker_hold_ms;
    auto_clicker_.gap_ms = profile.auto_clicker_gap_ms;
}

ProfileBindingEntry KeyboardInput::saveToProfile(const std::string& name) const {
    ProfileBindingEntry profile;
    profile.name = name;
    for (const auto& [_, b] : button_bindings_) {
        profile.button_bindings.push_back(b);
    }
    profile.stick_bindings = stick_bindings_;
    profile.mouse_look_enabled = mouse_look_.enabled;
    profile.mouse_sensitivity = mouse_look_.sensitivity;
    profile.auto_clicker_enabled = auto_clicker_.enabled;
    profile.auto_clicker_button_id = auto_clicker_.button_id;
    profile.auto_clicker_hold_ms = auto_clicker_.hold_ms;
    profile.auto_clicker_gap_ms = auto_clicker_.gap_ms;
    return profile;
}

} // namespace ghostpad
