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
#include <vector>
#include "protocol/gpad_packet.h"

namespace ghostpad {

struct GamepadDevice {
    int index = 0;
    std::string name;
    bool connected = false;
};

struct GamepadRemap {
    int from_button = 0;
    int to_button = 0;
};

class GamepadInput {
public:
    GamepadInput();

    void update();
    std::vector<GamepadDevice> listGamepads() const;
    PadStateInput getPadState(int device_index = 0) const;
    bool isButtonPressed(int device_index, int button) const;
    float getAxis(int device_index, int axis) const;

    void setRemap(int from_button, int to_button);
    void clearRemaps();
    std::vector<GamepadRemap> getAllRemaps() const;

private:
    std::vector<GamepadRemap> remaps_;
    mutable void* last_glfw_window_ = nullptr; // stored for polling
};

} // namespace ghostpad
