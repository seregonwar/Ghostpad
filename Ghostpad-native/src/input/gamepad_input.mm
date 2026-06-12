#include "ghostpad_platform.h"
#include "input/gamepad_input.h"
#if !defined(GHOSTPAD_IOS) && !defined(GHOSTPAD_CONSOLE)
#include <GLFW/glfw3.h>
#else
#import <GameController/GameController.h>
#endif
#include <algorithm>
#include <cmath>

namespace ghostpad {

#ifdef GHOSTPAD_IOS
static NSMutableArray<GCController*>* g_connected_controllers = nil;
static id g_connect_observer = nil;
static id g_disconnect_observer = nil;

static void ios_gamepad_init() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    g_connected_controllers = [[NSMutableArray alloc] init];

    g_connect_observer = [[NSNotificationCenter defaultCenter]
        addObserverForName:GCControllerDidConnectNotification
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification* note) {
        GCController* controller = note.object;
        if (controller.extendedGamepad) {
            [g_connected_controllers addObject:controller];
            NSLog(@"[Ghostpad] Controller connected: %@", controller.vendorName ?: @"Unknown");
        }
    }];

    g_disconnect_observer = [[NSNotificationCenter defaultCenter]
        addObserverForName:GCControllerDidDisconnectNotification
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification* note) {
        GCController* controller = note.object;
        [g_connected_controllers removeObject:controller];
        NSLog(@"[Ghostpad] Controller disconnected: %@", controller.vendorName ?: @"Unknown");
    }];

    [GCController startWirelessControllerDiscoveryWithCompletionHandler:^{
        for (GCController* controller in [GCController controllers]) {
            if (controller.extendedGamepad && ![g_connected_controllers containsObject:controller]) {
                [g_connected_controllers addObject:controller];
                NSLog(@"[Ghostpad] Found controller: %@", controller.vendorName ?: @"Unknown");
            }
        }
    }];
}

static GCController* ios_get_controller(int index) {
    if (!g_connected_controllers || index < 0 || index >= (int)[g_connected_controllers count]) {
        return nil;
    }
    return g_connected_controllers[index];
}
#endif

GamepadInput::GamepadInput() {
#ifdef GHOSTPAD_IOS
    ios_gamepad_init();
#endif
}

GamepadInput::~GamepadInput() {
#ifdef GHOSTPAD_IOS
    if (g_connect_observer) {
        [[NSNotificationCenter defaultCenter] removeObserver:g_connect_observer];
        g_connect_observer = nil;
    }
    if (g_disconnect_observer) {
        [[NSNotificationCenter defaultCenter] removeObserver:g_disconnect_observer];
        g_disconnect_observer = nil;
    }
#endif
}

void GamepadInput::update() {
#if !defined(GHOSTPAD_IOS) && !defined(GHOSTPAD_CONSOLE)
    // GLFW polls gamepad states automatically via glfwPollEvents()
#else
    // Refresh controller list from system
    if (g_connected_controllers) {
        // Check for controllers not yet in our list (e.g. already connected before app launch)
        for (GCController* c in [GCController controllers]) {
            if (c.extendedGamepad && ![g_connected_controllers containsObject:c]) {
                [g_connected_controllers addObject:c];
                NSLog(@"[Ghostpad] Found existing controller: %@", c.vendorName ?: @"Unknown");
            }
        }
    }
#endif
}

std::vector<GamepadDevice> GamepadInput::listGamepads() const {
    std::vector<GamepadDevice> devices;
#if !defined(GHOSTPAD_IOS) && !defined(GHOSTPAD_CONSOLE)
    for (int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_16; i++) {
        if (glfwJoystickPresent(i) && glfwJoystickIsGamepad(i)) {
            GamepadDevice d;
            d.index = i;
            d.name = glfwGetJoystickName(i) ? glfwGetJoystickName(i) : "Unknown";
            d.connected = true;
            devices.push_back(d);
        }
    }
#else
    if (!g_connected_controllers) return devices;
    for (int i = 0; i < (int)[g_connected_controllers count]; i++) {
        GCController* controller = g_connected_controllers[i];
        GamepadDevice d;
        d.index = i;
        d.name = controller.vendorName ? [controller.vendorName UTF8String] : "iOS Controller";
        d.connected = true;
        devices.push_back(d);
    }
#endif
    return devices;
}

#if !defined(GHOSTPAD_IOS) && !defined(GHOSTPAD_CONSOLE)
static int mapGlfwButtonToPS5(int glfw_button) {
    switch (glfw_button) {
        case GLFW_GAMEPAD_BUTTON_A: return 0;
        case GLFW_GAMEPAD_BUTTON_B: return 1;
        case GLFW_GAMEPAD_BUTTON_X: return 2;
        case GLFW_GAMEPAD_BUTTON_Y: return 3;
        case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: return 4;
        case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: return 5;
        case GLFW_GAMEPAD_BUTTON_DPAD_UP: return 12;
        case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: return 13;
        case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: return 14;
        case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: return 15;
        case GLFW_GAMEPAD_BUTTON_BACK: return 8;
        case GLFW_GAMEPAD_BUTTON_START: return 9;
        case GLFW_GAMEPAD_BUTTON_LEFT_THUMB: return 10;
        case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB: return 11;
        case GLFW_GAMEPAD_BUTTON_GUIDE: return 16;
        default: return -1;
    }
}
#endif

PadStateInput GamepadInput::getPadState(int device_index) const {
    PadStateInput pad = {};
#if !defined(GHOSTPAD_IOS) && !defined(GHOSTPAD_CONSOLE)
    int joy = GLFW_JOYSTICK_1 + device_index;

    if (!glfwJoystickPresent(joy) || !glfwJoystickIsGamepad(joy)) {
        return pad;
    }

    GLFWgamepadstate gs;
    if (!glfwGetGamepadState(joy, &gs)) {
        return pad;
    }

    for (int b = 0; b <= GLFW_GAMEPAD_BUTTON_LAST; b++) {
        if (gs.buttons[b] == GLFW_PRESS) {
            int ps5_btn = mapGlfwButtonToPS5(b);
            if (ps5_btn >= 0 && ps5_btn < 18) {
                pad.button_states[ps5_btn] = true;
            }
        }
    }

    float lt = gs.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
    float rt = gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
    pad.trigger_l2 = static_cast<uint8_t>(std::clamp((lt + 1.0f) * 127.5f, 0.0f, 255.0f));
    pad.trigger_r2 = static_cast<uint8_t>(std::clamp((rt + 1.0f) * 127.5f, 0.0f, 255.0f));

    float lx = gs.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
    float ly = gs.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
    pad.stick_states[0] = static_cast<uint8_t>(std::clamp((lx + 1.0f) * 127.5f, 0.0f, 255.0f));
    pad.stick_states[1] = static_cast<uint8_t>(std::clamp((ly + 1.0f) * 127.5f, 0.0f, 255.0f));

    float rx = gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
    float ry = gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
    pad.stick_states[2] = static_cast<uint8_t>(std::clamp((rx + 1.0f) * 127.5f, 0.0f, 255.0f));
    pad.stick_states[3] = static_cast<uint8_t>(std::clamp((ry + 1.0f) * 127.5f, 0.0f, 255.0f));

    for (int i = 0; i < 4; i++) {
        int val = pad.stick_states[i];
        if (std::abs(val - 128) < 16) {
            pad.stick_states[i] = 128;
        }
    }
#else
    GCController* controller = ios_get_controller(device_index);
    if (!controller || !controller.extendedGamepad) {
        return pad;
    }

    GCExtendedGamepad* gp = controller.extendedGamepad;

    if (gp.buttonA.isPressed) pad.button_states[0] = true;
    if (gp.buttonB.isPressed) pad.button_states[1] = true;
    if (gp.buttonX.isPressed) pad.button_states[2] = true;
    if (gp.buttonY.isPressed) pad.button_states[3] = true;
    if (gp.leftShoulder.isPressed) pad.button_states[4] = true;
    if (gp.rightShoulder.isPressed) pad.button_states[5] = true;
    if (gp.dpad.up.isPressed) pad.button_states[12] = true;
    if (gp.dpad.down.isPressed) pad.button_states[13] = true;
    if (gp.dpad.left.isPressed) pad.button_states[14] = true;
    if (gp.dpad.right.isPressed) pad.button_states[15] = true;
    if (gp.leftThumbstickButton.isPressed) pad.button_states[10] = true;
    if (gp.rightThumbstickButton.isPressed) pad.button_states[11] = true;
    if (gp.buttonMenu.isPressed) pad.button_states[9] = true;
    if (@available(iOS 14.0, *)) {
        if (gp.buttonOptions.isPressed) pad.button_states[8] = true;
        if (gp.buttonHome.isPressed) pad.button_states[16] = true;
    }

    float lt = gp.leftTrigger.value;
    float rt = gp.rightTrigger.value;
    pad.trigger_l2 = static_cast<uint8_t>(std::clamp(lt * 255.0f, 0.0f, 255.0f));
    pad.trigger_r2 = static_cast<uint8_t>(std::clamp(rt * 255.0f, 0.0f, 255.0f));

    float lx = gp.leftThumbstick.xAxis.value;
    float ly = -gp.leftThumbstick.yAxis.value;
    pad.stick_states[0] = static_cast<uint8_t>(std::clamp((lx + 1.0f) * 127.5f, 0.0f, 255.0f));
    pad.stick_states[1] = static_cast<uint8_t>(std::clamp((ly + 1.0f) * 127.5f, 0.0f, 255.0f));

    float rx = gp.rightThumbstick.xAxis.value;
    float ry = -gp.rightThumbstick.yAxis.value;
    pad.stick_states[2] = static_cast<uint8_t>(std::clamp((rx + 1.0f) * 127.5f, 0.0f, 255.0f));
    pad.stick_states[3] = static_cast<uint8_t>(std::clamp((ry + 1.0f) * 127.5f, 0.0f, 255.0f));

    for (int i = 0; i < 4; i++) {
        int val = pad.stick_states[i];
        if (std::abs(val - 128) < 16) {
            pad.stick_states[i] = 128;
        }
    }
#endif

    for (const auto& remap : remaps_) {
        if (pad.button_states[remap.from_button]) {
            pad.button_states[remap.from_button] = false;
            pad.button_states[remap.to_button] = true;
        }
    }

    return pad;
}

bool GamepadInput::isButtonPressed(int device_index, int button) const {
#if !defined(GHOSTPAD_IOS) && !defined(GHOSTPAD_CONSOLE)
    int joy = GLFW_JOYSTICK_1 + device_index;
    if (!glfwJoystickPresent(joy) || !glfwJoystickIsGamepad(joy)) return false;

    GLFWgamepadstate gs;
    if (!glfwGetGamepadState(joy, &gs)) return false;

    return gs.buttons[button] == GLFW_PRESS;
#else
    GCController* controller = ios_get_controller(device_index);
    if (!controller || !controller.extendedGamepad) return false;

    GCExtendedGamepad* gp = controller.extendedGamepad;
    switch (button) {
        case 0: return gp.buttonA.isPressed;
        case 1: return gp.buttonB.isPressed;
        case 2: return gp.buttonX.isPressed;
        case 3: return gp.buttonY.isPressed;
        case 4: return gp.leftShoulder.isPressed;
        case 5: return gp.rightShoulder.isPressed;
        case 12: return gp.dpad.up.isPressed;
        case 13: return gp.dpad.down.isPressed;
        case 14: return gp.dpad.left.isPressed;
        case 15: return gp.dpad.right.isPressed;
        case 10: return gp.leftThumbstickButton.isPressed;
        case 11: return gp.rightThumbstickButton.isPressed;
        case 9: return gp.buttonMenu.isPressed;
        default: return false;
    }
#endif
}

float GamepadInput::getAxis(int device_index, int axis) const {
#if !defined(GHOSTPAD_IOS) && !defined(GHOSTPAD_CONSOLE)
    int joy = GLFW_JOYSTICK_1 + device_index;
    if (!glfwJoystickPresent(joy) || !glfwJoystickIsGamepad(joy)) return 0.0f;

    GLFWgamepadstate gs;
    if (!glfwGetGamepadState(joy, &gs)) return 0.0f;

    if (axis >= 0 && axis <= GLFW_GAMEPAD_AXIS_LAST) {
        return gs.axes[axis];
    }
#else
    GCController* controller = ios_get_controller(device_index);
    if (!controller || !controller.extendedGamepad) return 0.0f;

    GCExtendedGamepad* gp = controller.extendedGamepad;
    switch (axis) {
        case 0: return gp.leftThumbstick.xAxis.value;
        case 1: return -gp.leftThumbstick.yAxis.value;
        case 2: return gp.rightThumbstick.xAxis.value;
        case 3: return -gp.rightThumbstick.yAxis.value;
        case 4: return gp.leftTrigger.value * 2.0f - 1.0f;
        case 5: return gp.rightTrigger.value * 2.0f - 1.0f;
        default: return 0.0f;
    }
#endif
    return 0.0f;
}

void GamepadInput::setRemap(int from_button, int to_button) {
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
