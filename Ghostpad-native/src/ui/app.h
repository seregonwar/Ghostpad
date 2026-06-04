#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <string>
#include <vector>
#include <chrono>
#include <deque>
#include <atomic>

#include "network/ghostpad_client.h"
#include "network/beeper_client.h"
#include "network/ssm_client.h"
#include "network/payload_deploy.h"
#include "network/network_scanner.h"
#include "storage/console_store.h"
#include "storage/settings_store.h"
#include "storage/project_store.h"
#include "storage/profile_store.h"
#include "input/keyboard_input.h"
#include "input/gamepad_input.h"
#include "input/macro_engine.h"
#include "ui/gif_export.h"

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
    static constexpr int MAX_CONTROLLER_SLOTS = 4;

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
    KeyboardInput keyboard;
    GamepadInput gamepad_input;
    MacroEngine macro_engine;
    ConsoleStore consoles;
    SettingsStore settings;
    ProjectStore projects;
    ProfileStore profiles;
    PayloadDeployer deployer;

    // Multi-controller slot access
    GhostpadClient& ghostpad() { return ghostpad_[active_slot_]; }
    const GhostpadClient& ghostpad() const { return ghostpad_[active_slot_]; }
    GhostpadClient& ghostpadSlot(int slot) { return ghostpad_[slot % MAX_CONTROLLER_SLOTS]; }
    int activeSlot() const { return active_slot_; }
    void setActiveSlot(int slot);
    int ghostpadConnectedCount() const;
    bool isAnyGhostpadConnected() const;
    void disconnectAllGhostpad();
    void sendPadStateToAll(const GpadNetworkState& state);

    // UI state
    Screen current_screen = Screen::Home;
    std::string selected_console_ip;
    int selected_console_port = 6967;
    std::string selected_project_id;
    std::string selected_profile_id;
    std::string selected_command_id;
    std::string rebind_button_name;
    int rebind_button_id = -1;
    PadStateInput virtual_pad;
    std::atomic<bool> is_connecting_{false};
    bool is_layout_edit_mode = false;
    int selected_layout_component = 0;
    PadLayoutSettings temp_layout;
    bool has_last_recorded_ = false;
    PadStateInput last_recorded_ps_;

    // Active controller state query
    PadStateInput getCurrentPadState();

    // GIF export
    void startGifExport(const std::string& output_path, float vis_size, int fps);
    void cancelGifExport();
    bool isGifExportActive() const { return gif_export_active_; }

    // Status messages
    void addStatus(const std::string& msg, bool error = false);
    void drawStatusBar();

private:
    void drawMainMenu();
    void drawAppChrome();
    void drawSidebar(float width, float height);
    void drawTopBar(float x, float y, float width, float height);
    void renderScreen();

    // Multi-controller state
    GhostpadClient ghostpad_[MAX_CONTROLLER_SLOTS];
    int active_slot_ = 0;

    // Deploy status tracking
    DeployStatus deploy_status_;
    std::deque<StatusMessage> status_messages_;
    mutable std::mutex status_mutex_;

    // FPS tracking
    double fps_update_timer_ = 0.0;
    int fps_frame_count_ = 0;
    double current_fps_ = 0.0;

    // Input flush timer
    double input_flush_timer_ = 0.0;
    std::chrono::steady_clock::time_point last_pad_send_;

    // GIF export state
    bool gif_export_active_ = false;
    bool gif_frame_ready_ = false;
    GifExporter gif_exporter_;
    std::string gif_output_path_;
    float gif_vis_size_ = 200.0f;
    int gif_fps_ = 30;
    int gif_capture_width_ = 0;
    int gif_capture_height_ = 0;
    int gif_frame_idx_ = 0;
    double gif_frame_timer_ = 0.0;
};

} // namespace ghostpad
