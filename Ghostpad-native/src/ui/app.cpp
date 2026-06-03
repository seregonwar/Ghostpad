// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2024  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "GLFW/glfw3.h"
#include <cstdio>

// Forward declarations of screen render functions
namespace ghostpad {
extern void renderHomeScreen(App& app);
extern void renderConsolesScreen(App& app);
extern void renderSettingsScreen(App& app);
extern void renderBeeperScreen(App& app);
extern void renderSystemStateScreen(App& app);
extern void renderProjectsScreen(App& app);
extern void renderProjectDetailScreen(App& app);
extern void renderInputRedirectScreen(App& app);
extern void renderControllerScreen(App& app);
extern void renderCreditsScreen(App& app);
extern void renderPadVisualizer(const PadStateInput& state, float size);
extern void renderInteractivePadVisualizer(PadStateInput& state, float size);
}

static GLFWwindow* g_window = nullptr;

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static ImGuiKey glfwKeyToImGuiKey(int glfw_key) {
    if (glfw_key >= GLFW_KEY_SPACE && glfw_key <= GLFW_KEY_GRAVE_ACCENT) {
        return static_cast<ImGuiKey>(ImGuiKey_Space + (glfw_key - GLFW_KEY_SPACE));
    }
    if (glfw_key >= GLFW_KEY_ESCAPE && glfw_key <= GLFW_KEY_PAUSE) {
        return static_cast<ImGuiKey>(ImGuiKey_Escape + (glfw_key - GLFW_KEY_ESCAPE));
    }
    if (glfw_key >= GLFW_KEY_KP_0 && glfw_key <= GLFW_KEY_KP_EQUAL) {
        return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (glfw_key - GLFW_KEY_KP_0));
    }
    return ImGuiKey_None;
}

namespace ghostpad {

static void setupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text]                   = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.10f, 0.10f, 0.10f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.39f, 0.78f, 0.55f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.39f, 0.78f, 0.55f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.49f, 0.88f, 0.65f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.39f, 0.78f, 0.55f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.39f, 0.78f, 0.55f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 4.0f;
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);
}

App::App(const std::string& data_dir)
    : consoles(data_dir)
    , settings(data_dir)
    , projects(data_dir) {
}

App::~App() {
    shutdown();
}

void App::init() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        exit(1);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    g_window = glfwCreateWindow(1400, 900, "Ghostpad Native", nullptr, nullptr);
    if (!g_window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        exit(1);
    }

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);

    glfwSetWindowUserPointer(g_window, this);
    glfwSetKeyCallback(g_window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (app) app->onKey(key, scancode, action, mods);
    });
    glfwSetMouseButtonCallback(g_window, [](GLFWwindow* w, int button, int action, int mods) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (app) app->onMouseButton(button, action, mods);
    });
    glfwSetCursorPosCallback(g_window, [](GLFWwindow* w, double x, double y) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (app) app->onMouseMove(x, y);
    });
    glfwSetScrollCallback(g_window, [](GLFWwindow* w, double xoffset, double yoffset) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (app) app->onScroll(xoffset, yoffset);
    });

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    setupImGuiStyle();

    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    glfwShowWindow(g_window);

    last_pad_send_ = std::chrono::steady_clock::now();
}

void App::update(double dt) {
    fps_frame_count_++;
    fps_update_timer_ += dt;
    if (fps_update_timer_ >= 1.0) {
        current_fps_ = fps_frame_count_ / fps_update_timer_;
        fps_frame_count_ = 0;
        fps_update_timer_ = 0.0;
    }

    for (auto& msg : status_messages_) {
        msg.time_left -= static_cast<float>(dt);
    }
    while (!status_messages_.empty() && status_messages_.front().time_left <= 0.0f) {
        status_messages_.pop_front();
    }

    input_flush_timer_ += dt;
    if (input_flush_timer_ >= 1.0 / 60.0) {
        input_flush_timer_ = 0.0;
        auto status = ghostpad.getStatus();
        if (status.is_connected) {
            PadStateInput pad_state;
            auto gamepads = gamepad_input.listGamepads();
            if (!gamepads.empty()) {
                pad_state = gamepad_input.getPadState(0);
            } else {
                pad_state = keyboard.getPadState();
            }

            if (macro_engine.isPlaying()) {
                macro_engine.updatePlayback(1.0 / 60.0 * 1000.0);
                pad_state = macro_engine.getPlaybackState();
            }

            auto gpad_state = buildGpadState(pad_state);
            ghostpad.sendPadState(gpad_state);
        }
    }
}

void App::render() {
    if (glfwWindowShouldClose(g_window)) {
        should_close = true;
        return;
    }

    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Main window filling the entire viewport
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGui::Begin("Ghostpad Native", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);

    drawMainMenu();
    renderScreen();
    drawStatusBar();

    ImGui::End();

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(g_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.06f, 0.06f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(g_window);
}

void App::shutdown() {
    ghostpad.disconnect();
    deployer.stopKlogWatcher();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (g_window) {
        glfwDestroyWindow(g_window);
        g_window = nullptr;
    }
    glfwTerminate();
}

void App::onKey(int key, int scancode, int action, int mods) {
    if (rebind_button_id >= 0 && action == GLFW_PRESS && key != GLFW_KEY_ESCAPE) {
        keyboard.setButtonBinding(rebind_button_id, key,
                                   (mods & GLFW_MOD_CONTROL) != 0,
                                   (mods & GLFW_MOD_SHIFT) != 0,
                                   (mods & GLFW_MOD_ALT) != 0);
        rebind_button_id = -1;
        rebind_button_name.clear();
        return;
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        keyboard.setKeyPressed(key,
                               (mods & GLFW_MOD_CONTROL) != 0,
                               (mods & GLFW_MOD_SHIFT) != 0,
                               (mods & GLFW_MOD_ALT) != 0,
                               true);
    } else if (action == GLFW_RELEASE) {
        keyboard.setKeyPressed(key,
                               (mods & GLFW_MOD_CONTROL) != 0,
                               (mods & GLFW_MOD_SHIFT) != 0,
                               (mods & GLFW_MOD_ALT) != 0,
                               false);
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGuiKey imkey = glfwKeyToImGuiKey(key);
    if (imkey != ImGuiKey_None) {
        if (action == GLFW_PRESS) {
            io.AddKeyEvent(imkey, true);
        } else if (action == GLFW_RELEASE) {
            io.AddKeyEvent(imkey, false);
        }
    }

    io.KeyCtrl = (mods & GLFW_MOD_CONTROL) != 0;
    io.KeyShift = (mods & GLFW_MOD_SHIFT) != 0;
    io.KeyAlt = (mods & GLFW_MOD_ALT) != 0;
    io.KeySuper = (mods & GLFW_MOD_SUPER) != 0;

    (void)scancode;
}

void App::onMouseMove(double x, double y) {
    static double last_x = x, last_y = y;
    double dx = x - last_x;
    double dy = y - last_y;
    last_x = x;
    last_y = y;

    keyboard.updateMouseDelta(static_cast<float>(dx), static_cast<float>(dy));

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(static_cast<float>(x), static_cast<float>(y));
}

void App::onMouseButton(int button, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (button >= 0 && button < 5) {
        io.AddMouseButtonEvent(button, action == GLFW_PRESS);
    }
    (void)mods;
}

void App::onScroll(double xoffset, double yoffset) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseWheelEvent(static_cast<float>(xoffset), static_cast<float>(yoffset));
}

void App::drawMainMenu() {
    if (ImGui::BeginMenuBar()) {
        ImGui::TextUnformatted("Ghostpad");
        ImGui::SameLine();

        if (ImGui::MenuItem("Home")) current_screen = Screen::Home;
        if (ImGui::MenuItem("Consoles")) current_screen = Screen::Consoles;
        if (ImGui::MenuItem("Settings")) current_screen = Screen::Settings;
        if (ImGui::MenuItem("Beeper")) current_screen = Screen::Beeper;
        if (ImGui::MenuItem("System State")) current_screen = Screen::SystemState;
        if (ImGui::MenuItem("Projects")) current_screen = Screen::Projects;
        if (ImGui::MenuItem("Input Redirect")) current_screen = Screen::InputRedirect;
        if (ImGui::MenuItem("Controller")) current_screen = Screen::Controller;
        if (ImGui::MenuItem("Credits")) current_screen = Screen::Credits;

        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        auto status = ghostpad.getStatus();
        if (status.is_connected) {
            ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f), "Connected: %s:%d",
                               status.ip.c_str(), status.port);
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.3f, 0.3f, 1.0f), "Disconnected");
        }

        ImGui::EndMenuBar();
    }
}

void App::renderScreen() {
    ImVec2 content_size = ImGui::GetContentRegionAvail();
    content_size.y -= 30.0f; // Leave space for status bar

    ImGui::BeginChild("ScreenContent", content_size);

    switch (current_screen) {
        case Screen::Home:          renderHomeScreen(*this); break;
        case Screen::Consoles:      renderConsolesScreen(*this); break;
        case Screen::Settings:      renderSettingsScreen(*this); break;
        case Screen::Beeper:        renderBeeperScreen(*this); break;
        case Screen::SystemState:   renderSystemStateScreen(*this); break;
        case Screen::Projects:      renderProjectsScreen(*this); break;
        case Screen::ProjectDetail: renderProjectDetailScreen(*this); break;
        case Screen::InputRedirect: renderInputRedirectScreen(*this); break;
        case Screen::Controller:    renderControllerScreen(*this); break;
        case Screen::Credits:      renderCreditsScreen(*this); break;
    }

    ImGui::EndChild();
}

void App::addStatus(const std::string& msg, bool error) {
    status_messages_.push_back({msg, 5.0f, error});
    if (status_messages_.size() > 10) {
        status_messages_.pop_front();
    }
}

void App::drawStatusBar() {
    if (status_messages_.empty()) {
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 25);
        ImGui::Text("FPS: %.1f", current_fps_);
        return;
    }

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 28);
    ImGui::BeginChild("StatusBar", ImVec2(ImGui::GetWindowWidth(), 25), true,
                      ImGuiWindowFlags_NoScrollbar);
    for (auto& msg : status_messages_) {
        if (msg.is_error) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", msg.text.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.55f, 1.0f), "%s", msg.text.c_str());
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("|");
        ImGui::SameLine();
    }
    ImGui::Text("FPS: %.1f", current_fps_);
    ImGui::EndChild();
}

} // namespace ghostpad
