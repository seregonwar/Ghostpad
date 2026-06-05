// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "input/gamepad_input.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace ghostpad {

GamepadInput::GamepadInput() {}

void GamepadInput::update() {
    // GLFW polls gamepad states automatically via glfwPollEvents()
    // called in the main loop
}

std::vector<GamepadDevice> GamepadInput::listGamepads() const {
    std::vector<GamepadDevice> devices;
    for (int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_16; i++) {
        if (glfwJoystickPresent(i) && glfwJoystickIsGamepad(i)) {
            GamepadDevice d;
            d.index = i;
            d.name = glfwGetJoystickName(i) ? glfwGetJoystickName(i) : "Unknown";
            d.connected = true;
            devices.push_back(d);
        }
    }
    return devices;
}

int mapGlfwButtonToPS5(int glfw_button) {
    switch (glfw_button) {
        case GLFW_GAMEPAD_BUTTON_A: return 0;           // Cross
        case GLFW_GAMEPAD_BUTTON_B: return 1;           // Circle
        case GLFW_GAMEPAD_BUTTON_X: return 2;           // Square
        case GLFW_GAMEPAD_BUTTON_Y: return 3;           // Triangle
        case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: return 4; // L1
        case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: return 5;// R1
        case GLFW_GAMEPAD_BUTTON_DPAD_UP: return 12;
        case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: return 13;
        case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: return 14;
        case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: return 15;
        case GLFW_GAMEPAD_BUTTON_BACK: return 8;        // Create
        case GLFW_GAMEPAD_BUTTON_START: return 9;       // Options
        case GLFW_GAMEPAD_BUTTON_LEFT_THUMB: return 10; // L3
        case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB: return 11;// R3
        case GLFW_GAMEPAD_BUTTON_GUIDE: return 16;      // PS
        default: return -1;
    }
}

PadStateInput GamepadInput::getPadState(int device_index) const {
    PadStateInput pad = {};
    int joy = GLFW_JOYSTICK_1 + device_index;

    if (!glfwJoystickPresent(joy) || !glfwJoystickIsGamepad(joy)) {
        return pad;
    }

    GLFWgamepadstate gs;
    if (!glfwGetGamepadState(joy, &gs)) {
        return pad;
    }

    // Buttons
    for (int b = 0; b <= GLFW_GAMEPAD_BUTTON_LAST; b++) {
        if (gs.buttons[b] == GLFW_PRESS) {
            int ps5_btn = mapGlfwButtonToPS5(b);
            if (ps5_btn >= 0 && ps5_btn < 18) {
                pad.button_states[ps5_btn] = true;
            }
        }
    }

    // Analog triggers
    float lt = gs.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
    float rt = gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
    // GLFW trigger: -1 to 1, map to 0-255
    pad.trigger_l2 = static_cast<uint8_t>(std::clamp((lt + 1.0f) * 127.5f, 0.0f, 255.0f));
    pad.trigger_r2 = static_cast<uint8_t>(std::clamp((rt + 1.0f) * 127.5f, 0.0f, 255.0f));

    // Left stick
    float lx = gs.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
    float ly = gs.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
    pad.stick_states[0] = static_cast<uint8_t>(std::clamp((lx + 1.0f) * 127.5f, 0.0f, 255.0f));
    pad.stick_states[1] = static_cast<uint8_t>(std::clamp((ly + 1.0f) * 127.5f, 0.0f, 255.0f));

    // Right stick
    float rx = gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
    float ry = gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
    pad.stick_states[2] = static_cast<uint8_t>(std::clamp((rx + 1.0f) * 127.5f, 0.0f, 255.0f));
    pad.stick_states[3] = static_cast<uint8_t>(std::clamp((ry + 1.0f) * 127.5f, 0.0f, 255.0f));

    // Deadzone
    for (int i = 0; i < 4; i++) {
        int val = pad.stick_states[i];
        if (std::abs(val - 128) < 16) { // ~12%
            pad.stick_states[i] = 128;
        }
    }

    // Apply remaps
    for (const auto& remap : remaps_) {
        if (pad.button_states[remap.from_button]) {
            pad.button_states[remap.from_button] = false;
            pad.button_states[remap.to_button] = true;
        }
    }

    return pad;
}

bool GamepadInput::isButtonPressed(int device_index, int button) const {
    int joy = GLFW_JOYSTICK_1 + device_index;
    if (!glfwJoystickPresent(joy) || !glfwJoystickIsGamepad(joy)) return false;

    GLFWgamepadstate gs;
    if (!glfwGetGamepadState(joy, &gs)) return false;

    return gs.buttons[button] == GLFW_PRESS;
}

float GamepadInput::getAxis(int device_index, int axis) const {
    int joy = GLFW_JOYSTICK_1 + device_index;
    if (!glfwJoystickPresent(joy) || !glfwJoystickIsGamepad(joy)) return 0.0f;

    GLFWgamepadstate gs;
    if (!glfwGetGamepadState(joy, &gs)) return 0.0f;

    if (axis >= 0 && axis <= GLFW_GAMEPAD_AXIS_LAST) {
        return gs.axes[axis];
    }
    return 0.0f;
}

void GamepadInput::setRemap(int from_button, int to_button) {
    // Remove existing remap for the from_button
    for (auto it = remaps_.begin(); it != remaps_.end(); ) {
        if (it->from_button == from_button) {
            it = remaps_.erase(it);
        } else {
            ++it;
        }
    }
    remaps_.push_back({from_button, to_button});
}

void GamepadInput::clearRemaps() {
    remaps_.clear();
}

const std::vector<GamepadRemap>& GamepadInput::getAllRemaps() const {
    return remaps_;
}

} // namespace ghostpad
