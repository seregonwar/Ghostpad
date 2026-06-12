// Ghostpad console gamepad stub for PS4/PS5
// Provides empty GamepadInput implementations for headless console operation

#include "input/gamepad_input.h"
#include <vector>
#include <string>

namespace ghostpad {

GamepadInput::GamepadInput() {}
GamepadInput::~GamepadInput() {}

void GamepadInput::update() {
    // No gamepad polling on console (headless mode)
}

std::vector<GamepadDevice> GamepadInput::listGamepads() const {
    return {};
}

PadStateInput GamepadInput::getPadState(int index) const {
    PadStateInput pad = {};
    (void)index;
    return pad;
}

bool GamepadInput::isButtonPressed(int device_index, int button) const {
    (void)device_index; (void)button;
    return false;
}

float GamepadInput::getAxis(int device_index, int axis) const {
    (void)device_index; (void)axis;
    return 0.0f;
}

void GamepadInput::setRemap(int from_button, int to_button) {
    (void)from_button; (void)to_button;
}

} // namespace ghostpad
