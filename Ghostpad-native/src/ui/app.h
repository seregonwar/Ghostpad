#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2024  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <string>
#include <vector>
#include <chrono>
#include <deque>

#include "network/ghostpad_client.h"
#include "network/beeper_client.h"
#include "network/ssm_client.h"
#include "network/payload_deploy.h"
#include "network/network_scanner.h"
#include "storage/console_store.h"
#include "storage/settings_store.h"
#include "storage/project_store.h"
#include "input/keyboard_input.h"
#include "input/gamepad_input.h"
#include "input/macro_engine.h"

namespace ghostpad {

enum class Screen {
    Home,
    Consoles,
    Settings,
    Beeper,
    SystemState,
    Projects,
    ProjectDetail,
    InputRedirect,
    Controller,
    Credits,
};

struct StatusMessage {
    std::string text;
    float time_left = 0.0f;
    bool is_error = false;
};

class App {
public:
    App(const std::string& data_dir);
    ~App();

    void init();
    void update(double dt);
    void render();
    void shutdown();

    // GLFW callbacks
    void onKey(int key, int scancode, int action, int mods);
    void onMouseMove(double x, double y);
    void onMouseButton(int button, int action, int mods);
    void onScroll(double xoffset, double yoffset);

    bool should_close = false;

    // Public state
    GhostpadClient ghostpad;
    KeyboardInput keyboard;
    GamepadInput gamepad_input;
    MacroEngine macro_engine;
    ConsoleStore consoles;
    SettingsStore settings;
    ProjectStore projects;
    PayloadDeployer deployer;

    // UI state
    Screen current_screen = Screen::Home;
    std::string selected_console_ip;
    int selected_console_port = 6967;
    std::string selected_project_id;
    std::string rebind_button_name;
    int rebind_button_id = -1;

    // Status messages
    void addStatus(const std::string& msg, bool error = false);
    void drawStatusBar();

private:
    void drawMainMenu();
    void renderScreen();

    // Deploy status tracking
    DeployStatus deploy_status_;
    std::deque<StatusMessage> status_messages_;

    // FPS tracking
    double fps_update_timer_ = 0.0;
    int fps_frame_count_ = 0;
    double current_fps_ = 0.0;

    // Input flush timer
    double input_flush_timer_ = 0.0;
    std::chrono::steady_clock::time_point last_pad_send_;
};

} // namespace ghostpad
