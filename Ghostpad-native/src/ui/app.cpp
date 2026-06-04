// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "GLFW/glfw3.h"
#include <cstdio>
#include <algorithm>
#include <string>
#include <cmath>

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
extern void renderPadVisualizer(App& app, const PadStateInput& state, float size);
extern void renderInteractivePadVisualizer(App& app, PadStateInput& state, float size);
}

static GLFWwindow* g_window = nullptr;

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static ImGuiKey glfwKeyToImGuiKey(int glfw_key) {
    if (glfw_key >= GLFW_KEY_SPACE && glfw_key <= GLFW_KEY_GRAVE_ACCENT)
        return static_cast<ImGuiKey>(ImGuiKey_Space + (glfw_key - GLFW_KEY_SPACE));
    if (glfw_key >= GLFW_KEY_ESCAPE && glfw_key <= GLFW_KEY_PAUSE)
        return static_cast<ImGuiKey>(ImGuiKey_Escape + (glfw_key - GLFW_KEY_ESCAPE));
    if (glfw_key >= GLFW_KEY_KP_0 && glfw_key <= GLFW_KEY_KP_EQUAL)
        return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (glfw_key - GLFW_KEY_KP_0));
    return ImGuiKey_None;
}

namespace ghostpad {

App::App(const std::string& data_dir)
    : consoles(data_dir), settings(data_dir), projects(data_dir), profiles(data_dir), is_connecting_(false) {}

App::~App() { shutdown(); }

void App::init() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); exit(1); }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    g_window = glfwCreateWindow(1400, 900, "Ghostpad Native", nullptr, nullptr);
    if (!g_window) { fprintf(stderr, "Window creation failed\n"); glfwTerminate(); exit(1); }

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);
    glfwSetWindowUserPointer(g_window, this);

    glfwSetKeyCallback(g_window, [](GLFWwindow* w, int k, int sc, int a, int m) {
        auto* ap = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (ap) ap->onKey(k, sc, a, m);
    });
    glfwSetMouseButtonCallback(g_window, [](GLFWwindow* w, int b, int a, int m) {
        auto* ap = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (ap) ap->onMouseButton(b, a, m);
    });
    glfwSetCursorPosCallback(g_window, [](GLFWwindow* w, double x, double y) {
        auto* ap = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (ap) ap->onMouseMove(x, y);
    });
    glfwSetScrollCallback(g_window, [](GLFWwindow* w, double xo, double yo) {
        auto* ap = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (ap) ap->onScroll(xo, yo);
    });

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    // Load main font (Roboto-Medium)
#ifdef FONT_ROBOTO_PATH
    ImFont* mainFont = io.Fonts->AddFontFromFileTTF(FONT_ROBOTO_PATH, 16.0f);
#else
    ImFont* mainFont = nullptr;
#endif
    if (!mainFont) {
        io.Fonts->AddFontDefault();
    }

    // Load and merge FontAwesome icons
#ifdef FONT_FA_PATH
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = 14.0f;
    io.Fonts->AddFontFromFileTTF(FONT_FA_PATH, 14.0f, &icons_config, icons_ranges);
#endif

    ui::applyGhostpadTheme();

    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 150");
    glfwShowWindow(g_window);

    last_pad_send_ = std::chrono::steady_clock::now();

    auto appSettings = settings.read();
    if (!appSettings.active_profile_id.empty()) {
        auto profile = profiles.get(appSettings.active_profile_id);
        if (!profile.id.empty()) {
            keyboard.loadFromProfile(profile);
            selected_profile_id = profile.id;
        }
    }
}

void App::update(double dt) {
    fps_frame_count_++;
    fps_update_timer_ += dt;
    if (fps_update_timer_ >= 1.0) {
        current_fps_ = fps_frame_count_ / fps_update_timer_;
        fps_frame_count_ = 0; fps_update_timer_ = 0.0;
    }
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        for (auto& m : status_messages_) m.time_left -= (float)dt;
        while (!status_messages_.empty() && status_messages_.front().time_left <= 0.0f)
            status_messages_.pop_front();
    }

    input_flush_timer_ += dt;
    if (input_flush_timer_ >= 1.0 / 60.0) {
        input_flush_timer_ = 0.0;
        
        // Update macro playback logic
        if (macro_engine.isPlaying()) {
            macro_engine.updatePlayback(16.667);
        }
        
        // Fetch current active input state
        PadStateInput ps = getCurrentPadState();
        
        // ── LIVE SIGNAL RECORDING ──────────────────────────
        // Intercept input state deltas and store them as signals
        if (macro_engine.isRecording()) {
            if (!has_last_recorded_) {
                for (int i = 0; i < 22; i++) {
                    if (ps.button_states[i]) {
                        macro_engine.recordSignal(i, 255);
                    }
                }
                for (int i = 0; i < 4; i++) {
                    if (ps.stick_states[i] != 128) {
                        macro_engine.recordSignal(18 + i, ps.stick_states[i]);
                    }
                }
                if (ps.trigger_l2 > 0) {
                    macro_engine.recordSignal(22, ps.trigger_l2);
                }
                if (ps.trigger_r2 > 0) {
                    macro_engine.recordSignal(23, ps.trigger_r2);
                }
                last_recorded_ps_ = ps;
                has_last_recorded_ = true;
            } else {
                for (int i = 0; i < 22; i++) {
                    if (ps.button_states[i] != last_recorded_ps_.button_states[i]) {
                        macro_engine.recordSignal(i, ps.button_states[i] ? 255 : 0);
                    }
                }
                for (int i = 0; i < 4; i++) {
                    if (ps.stick_states[i] != last_recorded_ps_.stick_states[i]) {
                        macro_engine.recordSignal(18 + i, ps.stick_states[i]);
                    }
                }
                if (ps.trigger_l2 != last_recorded_ps_.trigger_l2) {
                    macro_engine.recordSignal(22, ps.trigger_l2);
                }
                if (ps.trigger_r2 != last_recorded_ps_.trigger_r2) {
                    macro_engine.recordSignal(23, ps.trigger_r2);
                }
                last_recorded_ps_ = ps;
            }
        } else {
            has_last_recorded_ = false;
        }

        // Stream input to all connected PS5 slots
        if (isAnyGhostpadConnected()) {
            sendPadStateToAll(buildGpadState(ps));
        }
    }
}

PadStateInput App::getCurrentPadState() {
    if (macro_engine.isPlaying()) {
        return macro_engine.getPlaybackState();
    }

    auto gps = gamepad_input.listGamepads();
    if (!gps.empty()) {
        auto gp_state = gamepad_input.getPadState(0);
        bool active = false;
        for (int i = 0; i < 4; i++) {
            if (std::abs(gp_state.stick_states[i] - 128) > 10) active = true;
        }
        if (gp_state.trigger_l2 > 5 || gp_state.trigger_r2 > 5) active = true;
        for (int i = 0; i < 22; i++) {
            if (gp_state.button_states[i]) active = true;
        }

        if (active || current_screen != Screen::Controller) {
            return gp_state;
        }
    }

    if (current_screen == Screen::Controller) {
        return virtual_pad;
    }

    return keyboard.getPadState();
}

void App::render() {
    if (glfwWindowShouldClose(g_window)) { should_close = true; return; }
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ui::drawBackground(ImGui::GetBackgroundDrawList(), vp->Pos, vp->Size);

    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::Begin("Ghostpad Native", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    drawAppChrome();
    ImGui::End();

    ImGui::Render();
    int dw, dh; glfwGetFramebufferSize(g_window, &dw, &dh);
    glViewport(0, 0, dw, dh);
    glClearColor(0.07f, 0.07f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(g_window);
}

void App::shutdown() {
    disconnectAllGhostpad();
    deployer.stopKlogWatcher();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (g_window) { glfwDestroyWindow(g_window); g_window = nullptr; }
    glfwTerminate();
}

void App::onKey(int key, int scancode, int action, int mods) {
    if (rebind_button_id >= 0 && action == GLFW_PRESS && key != GLFW_KEY_ESCAPE) {
        keyboard.setButtonBinding(rebind_button_id, key,
            (mods & GLFW_MOD_CONTROL), (mods & GLFW_MOD_SHIFT), (mods & GLFW_MOD_ALT));
        rebind_button_id = -1; rebind_button_name.clear();
        return;
    }
    bool down = (action == GLFW_PRESS || action == GLFW_REPEAT);
    keyboard.setKeyPressed(key, (mods & GLFW_MOD_CONTROL), (mods & GLFW_MOD_SHIFT),
                           (mods & GLFW_MOD_ALT), down);

    ImGuiIO& io = ImGui::GetIO();
    ImGuiKey ik = glfwKeyToImGuiKey(key);
    if (ik != ImGuiKey_None) io.AddKeyEvent(ik, down);
    io.KeyCtrl  = (mods & GLFW_MOD_CONTROL);
    io.KeyShift = (mods & GLFW_MOD_SHIFT);
    io.KeyAlt   = (mods & GLFW_MOD_ALT);
    io.KeySuper = (mods & GLFW_MOD_SUPER);
    (void)scancode;
}

void App::onMouseMove(double x, double y) {
    static double lx = x, ly = y;
    keyboard.updateMouseDelta((float)(x - lx), (float)(y - ly));
    lx = x; ly = y;
    ImGui::GetIO().AddMousePosEvent((float)x, (float)y);
}

void App::onMouseButton(int button, int action, int mods) {
    if (button >= 0 && button < 5)
        ImGui::GetIO().AddMouseButtonEvent(button, action == GLFW_PRESS);
    (void)mods;
}

void App::onScroll(double xo, double yo) {
    ImGui::GetIO().AddMouseWheelEvent((float)xo, (float)yo);
}

// ── Sidebar ──────────────────────────────────────────────

static void drawSidebarGradient(ImVec2 pos, ImVec2 sz) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mx(pos.x + sz.x, pos.y + sz.y);
    dl->AddRectFilledMultiColor(pos, mx,
        ui::u32(ui::rgba(24, 16, 36, 252)), ui::u32(ui::rgba(14, 12, 17, 254)),
        ui::u32(ui::rgba(11, 10, 14, 254)), ui::u32(ui::rgba(18, 14, 24, 252)));
    dl->AddLine(ImVec2(mx.x, pos.y), mx, ui::u32(ui::rgba(68, 55, 84, 110)), 1.0f);
}

static void drawSidebarHeader(float x, float w) {
    const auto& p = ui::colors();
    ImGui::SetCursorPos(ImVec2(x + 4, 26));
    ImGui::TextColored(ui::rgba(215, 200, 255), "%s  GHOSTPAD", ICON_FA_GHOST);
    ImGui::SetCursorPos(ImVec2(x + 4, 50));
    ImGui::TextColored(p.dim, "Native v1.0.0");
    ImGui::SetCursorPos(ImVec2(x, 72));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cp = ImGui::GetCursorScreenPos();
    dl->AddLine(ImVec2(cp.x, cp.y), ImVec2(cp.x + w - 42, cp.y),
                ui::u32(ui::rgba(88, 70, 105, 90)), 1.0f);
    ImGui::Dummy(ImVec2(0, 6));
}

static void drawSidebarConnection(App& app, float x, float w) {
    const auto& p = ui::colors();
    auto ds = app.deployer.getStatus();
    int connected = app.ghostpadConnectedCount();
    bool anyConnected = connected > 0;

    ImGui::SetCursorPosX(x);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::rgba(255, 255, 255, 8));
    ImGui::PushStyleColor(ImGuiCol_Border, ui::rgba(255, 255, 255, 12));
    
    float cardH = 50.0f;
    if (anyConnected) cardH += 22.0f * connected;
    if (ds.phase != "idle" && !ds.phase.empty()) cardH += 22.0f;
    
    ImGui::BeginChild("ConnectionCard", ImVec2(w - 28, cardH), true);
    
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 led_pos(pos.x + 16.0f, pos.y + 16.0f);

    if (anyConnected) {
        float t = (float)glfwGetTime();
        float glow_r = 7.0f + 2.0f * std::sin(t * 4.0f);
        float glow_a = 0.25f + 0.15f * std::sin(t * 4.0f);
        dl->AddCircleFilled(led_pos, glow_r, ui::u32(ui::withAlpha(p.success, glow_a)), 16);
        dl->AddCircleFilled(led_pos, 3.5f, ui::u32(p.success), 16);
        dl->AddText(ImVec2(pos.x + 28.0f, pos.y + 8.0f), ui::u32(ui::rgba(220, 220, 225)),
                    connected > 1 ? ("Connected x" + std::to_string(connected)).c_str() : "Connected");

        float yOff = 34.0f;
        for (int i = 0; i < App::MAX_CONTROLLER_SLOTS; i++) {
            auto slotSt = app.ghostpadSlot(i).getStatus();
            if (slotSt.is_connected) {
                ImGui::SetCursorPos(ImVec2(12, yOff));
                ImGui::TextColored(p.muted, "%s  P%d  %s:%d", ICON_FA_SIGNAL, i + 1, slotSt.ip.c_str(), slotSt.port);
                yOff += 22.0f;
            }
        }
    } else {
        dl->AddCircleFilled(led_pos, 3.5f, ui::u32(p.muted), 16);
        dl->AddText(ImVec2(pos.x + 28.0f, pos.y + 8.0f), ui::u32(p.muted), "Offline");
    }
    
    float deployY = anyConnected ? 34.0f + 22.0f * connected : 34.0f;
    if (ds.phase != "idle" && !ds.phase.empty()) {
        ImVec4 col = ds.phase == "error" ? p.danger :
                     ds.phase == "ready" ? p.success : p.warning;
        ImGui::SetCursorPos(ImVec2(12, deployY));
        ImGui::TextColored(col, "%s  %s", ICON_FA_DOWNLOAD, ds.message.c_str());
    }
    
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    ImGui::Spacing();
}

static void drawSidebarGroup(const char* label, float x, float /*w*/) {
    ImGui::SetCursorPosX(x + 4);
    ImGui::TextColored(ui::rgba(110, 100, 130), "%s", label);
}

static bool drawSidebarNav(const char* icon, const char* label, bool active, ImVec4 accent, float w) {
    const auto& p = ui::colors();
    float h = 48.0f;

    // Position horizontally to align with ConnectionCard
    ImGui::SetCursorPosX(14.0f);
    ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::PushID(label);
    ImGui::InvisibleButton("##nav", ImVec2(w - 14.0f, h));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mx(pos.x + w - 14.0f, pos.y + h);
    bool hovered = ImGui::IsItemHovered();

    if (active) {
        dl->AddRectFilled(pos, mx, ui::u32(ui::withAlpha(accent, 0.18f)), 10.0f);
        dl->AddRectFilled(ImVec2(pos.x, pos.y + 6), ImVec2(pos.x + 4, pos.y + h - 6),
                          ui::u32(accent), 4.0f);
    } else if (hovered) {
        dl->AddRectFilled(pos, mx, ui::u32(ui::rgba(255, 255, 255, 12)), 10.0f);
    }

    ImVec4 txtCol = active ? ui::rgba(240, 235, 255) : (hovered ? p.text : p.muted);
    
    // Draw FontAwesome icon if provided
    if (icon && icon[0]) {
        dl->AddText(ImVec2(pos.x + 18, pos.y + (h - ImGui::GetTextLineHeight()) * 0.5f),
                    ui::u32(active ? accent : p.muted), icon);
        dl->AddText(ImVec2(pos.x + 42, pos.y + (h - ImGui::GetTextLineHeight()) * 0.5f),
                    ui::u32(txtCol), label);
    } else {
        ImVec2 dotC(pos.x + 22, pos.y + h * 0.5f);
        dl->AddCircleFilled(dotC, 5.0f, ui::u32(active ? accent : ui::withAlpha(accent, 0.55f)));
        dl->AddCircle(dotC, 5.0f, ui::u32(ui::withAlpha(accent, 0.7f)), 0, 1.0f);
        dl->AddText(ImVec2(pos.x + 38, pos.y + (h - ImGui::GetTextLineHeight()) * 0.5f),
                    ui::u32(txtCol), label);
    }

    ImGui::PopID();
    return ImGui::IsItemClicked();
}

void App::drawSidebar(float width, float height) {
    const auto& p = ui::colors();
    const float pad = 14.0f;

    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("Sidebar", ImVec2(width, height), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    drawSidebarGradient(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    drawSidebarHeader(pad, width);
    drawSidebarConnection(*this, pad, width);

    float navY = ImGui::GetCursorPosY() + 10;
    float navW = width - pad;

    // ── Navigation Scroll Area ──────────────────────────────────
    ImGui::SetCursorPosY(navY);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("SidebarNavList", ImVec2(width, height - 110 - navY), false, ImGuiWindowFlags_NoScrollbar);

    // ── Main ──
    drawSidebarGroup("MAIN", pad, width);
    if (drawSidebarNav(ICON_FA_HOUSE, "Home",          current_screen == Screen::Home,          p.primary2, navW)) current_screen = Screen::Home;
    if (drawSidebarNav(ICON_FA_DESKTOP, "Consoles",       current_screen == Screen::Consoles,       p.link,     navW)) current_screen = Screen::Consoles;
    if (drawSidebarNav(ICON_FA_GAMEPAD, "Controller",     current_screen == Screen::Controller,     p.success,  navW)) current_screen = Screen::Controller;
    if (drawSidebarNav(ICON_FA_KEYBOARD, "Input Redirect", current_screen == Screen::InputRedirect,  p.primary,  navW)) current_screen = Screen::InputRedirect;

    ImGui::Spacing();

    // ── Toolset ──
    drawSidebarGroup("TOOLSET", pad, width);
    if (drawSidebarNav(ICON_FA_FOLDER_OPEN, "Projects",  current_screen == Screen::Projects,  p.warning, navW)) current_screen = Screen::Projects;
    if (drawSidebarNav(ICON_FA_VOLUME_HIGH, "Beeper",    current_screen == Screen::Beeper,    p.success, navW)) current_screen = Screen::Beeper;

    ImGui::Spacing();

    // ── System ──
    drawSidebarGroup("SYSTEM", pad, width);
    if (drawSidebarNav(ICON_FA_MICROCHIP, "System State", current_screen == Screen::SystemState, p.danger, navW)) current_screen = Screen::SystemState;
    if (drawSidebarNav(ICON_FA_GEAR, "Settings",     current_screen == Screen::Settings,    p.muted,  navW)) current_screen = Screen::Settings;

    ImGui::Spacing();

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // ── Footer ──
    ImGui::SetCursorPosY(height - 110);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 fp = ImGui::GetCursorScreenPos();
    dl->AddLine(ImVec2(fp.x + pad, fp.y), ImVec2(fp.x + width - pad, fp.y),
                ui::u32(ui::rgba(88, 70, 105, 80)), 1.0f);
    ImGui::Dummy(ImVec2(0, 10));

    ImGui::SetCursorPosX(pad + 4);
    ImGui::TextColored(p.dim, "%s  Direct | <1ms", ICON_FA_BOLT);
    ImGui::SetCursorPosX(pad + 4);
    ImGui::TextColored(p.dim, "%s  Native | 60 FPS", ICON_FA_MICROCHIP);

    ImGui::SetCursorPosY(height - 34);
    ImGui::SetCursorPosX(pad + 4);
    
    // Polished Credits selection
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ui::rgba(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ui::rgba(255,255,255,10));
    if (ImGui::Selectable(ICON_FA_CIRCLE_INFO "  Credits", current_screen == Screen::Credits, 0, ImVec2(130, 20)))
        current_screen = Screen::Credits;
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// ── Top Bar ─────────────────────────────────────────────

static const char* screenTitle(Screen screen) {
    switch (screen) {
        case Screen::Home:          return "Command Center";
        case Screen::Consoles:      return "Consoles";
        case Screen::Settings:      return "Settings";
        case Screen::Beeper:        return "Beeper & LED";
        case Screen::SystemState:   return "System State";
        case Screen::Projects:      return "Projects";
        case Screen::ProjectDetail: return "Project Editor";
        case Screen::InputRedirect: return "Input Redirection";
        case Screen::Controller:    return "Virtual Controller";
        case Screen::Credits:       return "Credits & License";
    }
    return "Ghostpad";
}

static const char* screenIcon(Screen screen) {
    switch (screen) {
        case Screen::Home:          return ICON_FA_HOUSE;
        case Screen::Consoles:      return ICON_FA_DESKTOP;
        case Screen::Settings:      return ICON_FA_GEAR;
        case Screen::Beeper:        return ICON_FA_VOLUME_HIGH;
        case Screen::SystemState:   return ICON_FA_MICROCHIP;
        case Screen::Projects:      return ICON_FA_FOLDER_OPEN;
        case Screen::ProjectDetail: return ICON_FA_CODE;
        case Screen::InputRedirect: return ICON_FA_KEYBOARD;
        case Screen::Controller:    return ICON_FA_GAMEPAD;
        case Screen::Credits:       return ICON_FA_CIRCLE_INFO;
    }
    return ICON_FA_GHOST;
}

void App::drawTopBar(float x, float y, float width, float height) {
    const auto& p = ui::colors();
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::rgba(14, 14, 18, 220));
    ImGui::PushStyleColor(ImGuiCol_Border, ui::rgba(48, 44, 58, 120));
    ImGui::BeginChild("TopBar", ImVec2(width, height), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    bool anyConnected = isAnyGhostpadConnected();
    int connected = ghostpadConnectedCount();
    auto st = ghostpad().getStatus();

    ImGui::SetCursorPos(ImVec2(24, 14));
    ImGui::TextColored(p.primary2, "%s  %s", screenIcon(current_screen), screenTitle(current_screen));
    ImGui::SetCursorPos(ImVec2(24, 40));
    if (anyConnected) {
        if (connected > 1)
            ImGui::TextColored(p.success, "%s  Streaming %d controllers  %s:%d", ICON_FA_SIGNAL, connected, st.ip.c_str(), st.port);
        else
            ImGui::TextColored(p.success, "%s  Streaming  %s:%d", ICON_FA_SIGNAL, st.ip.c_str(), st.port);
    } else {
        ImGui::TextColored(p.muted, "Connect a console to begin");
    }

    /*
     *    [ NAV BUTTONS ] -> aligned right
     */
    float r = ImGui::GetWindowWidth() - 12.0f;
    float buttons_w = !anyConnected ? 150.0f : (120.0f + 120.0f + ImGui::GetStyle().ItemSpacing.x);

    ImGui::SetCursorPos(ImVec2(r - buttons_w, 16));
    if (!anyConnected) {
        if (ui::primaryButton(ICON_FA_LINK "  Connect Console", ImVec2(150, 36)))
            current_screen = Screen::Consoles;
    } else {
        if (ui::softButton(ICON_FA_GAMEPAD "  Controller", ImVec2(120, 36)))
            current_screen = Screen::Controller;
        ImGui::SameLine();
        if (ui::dangerButton(ICON_FA_LINK_SLASH "  Disconnect", ImVec2(120, 36))) {
            disconnectAllGhostpad();
            deployer.stopKlogWatcher();
            selected_console_ip.clear();
            addStatus("Disconnected all controllers");
        }
    }
    ImGui::SameLine();
    

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// ── Layout ──────────────────────────────────────────────

void App::drawAppChrome() {
    const ImVec2 sz = ImGui::GetWindowSize();
    const float sb_w = 258.0f, top_h = 80.0f, bot_h = 42.0f, pad = 22.0f;

    drawSidebar(sb_w, sz.y);
    drawTopBar(sb_w, 0.0f, sz.x - sb_w, top_h);

    ImGui::SetCursorPos(ImVec2(sb_w + pad, top_h + pad));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::rgba(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ui::rgba(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("Content", ImVec2(sz.x - sb_w - pad * 2, sz.y - top_h - bot_h - pad * 2), false);
    renderScreen();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPos(ImVec2(sb_w, sz.y - bot_h));
    drawStatusBar();
}

void App::renderScreen() {
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
        case Screen::Credits:       renderCreditsScreen(*this); break;
    }
}

// ── Status ──────────────────────────────────────────────

void App::addStatus(const std::string& msg, bool error) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_messages_.push_back({msg, 5.0f, error});
    if (status_messages_.size() > 10) status_messages_.pop_front();
}

void App::drawStatusBar() {
    const auto& p = ui::colors();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::rgba(13, 13, 16, 240));
    ImGui::PushStyleColor(ImGuiCol_Border, ui::rgba(48, 44, 58, 100));
    ImGui::BeginChild("StatusBar", ImVec2(ImGui::GetContentRegionAvail().x, 42), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::SetCursorPos(ImVec2(24, 11));
    
    StatusMessage latest_msg;
    bool has_msg = false;
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        if (!status_messages_.empty()) {
            latest_msg = status_messages_.back();
            has_msg = true;
        }
    }

    if (has_msg) {
        ImGui::TextColored(latest_msg.is_error ? p.danger : p.success, "%s  %s", 
            latest_msg.is_error ? ICON_FA_TRIANGLE_EXCLAMATION : ICON_FA_CIRCLE_CHECK, latest_msg.text.c_str());
    } else {
        ImGui::TextColored(p.muted, "%s  Ready", ICON_FA_CIRCLE_CHECK);
    }

    float r = ImGui::GetWindowWidth();
    ImGui::SameLine(r - 400);
    ImGui::TextColored(p.dim, "%s  FPS %.0f", ICON_FA_GAUGE_HIGH, current_fps_);
    ImGui::SameLine();
    int cnt = ghostpadConnectedCount();
    ImGui::TextColored(p.dim, "|  %s  %s", ICON_FA_GAMEPAD,
        cnt > 0 ? (cnt > 1 ? ("GPAD x" + std::to_string(cnt)).c_str() : "GPAD active") : "GPAD idle");
    ImGui::SameLine();
    
    size_t num_notices = 0;
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        num_notices = status_messages_.size();
    }
    ImGui::TextColored(p.dim, "|  %s  %zu notices", ICON_FA_BELL, num_notices);

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// ── Multi-Controller Helpers ────────────────────────────

void App::setActiveSlot(int slot) {
    active_slot_ = slot % MAX_CONTROLLER_SLOTS;
}

int App::ghostpadConnectedCount() const {
    int count = 0;
    for (int i = 0; i < MAX_CONTROLLER_SLOTS; i++) {
        if (ghostpad_[i].getStatus().is_connected) count++;
    }
    return count;
}

bool App::isAnyGhostpadConnected() const {
    for (int i = 0; i < MAX_CONTROLLER_SLOTS; i++) {
        if (ghostpad_[i].getStatus().is_connected) return true;
    }
    return false;
}

void App::disconnectAllGhostpad() {
    for (int i = 0; i < MAX_CONTROLLER_SLOTS; i++) {
        ghostpad_[i].disconnect();
    }
}

void App::sendPadStateToAll(const GpadNetworkState& state) {
    for (int i = 0; i < MAX_CONTROLLER_SLOTS; i++) {
        if (ghostpad_[i].getStatus().is_connected) {
            ghostpad_[i].sendPadState(state);
        }
    }
}

} // namespace ghostpad
