// ImGui PS4 Console Backend — Gamepad Input (native scePad, no SDL2)
// Uses ImGui 1.92+ AddKeyEvent/AddKeyAnalogEvent API for gamepad navigation

#include "imgui.h"
// BSD compatibility types (needed by kernel.h)
typedef unsigned short u_short;
typedef unsigned int   u_int;

#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
#include <cstdio>
#include <cstring>

static int  g_pad_handle = -1;
static bool g_pad_initialized = false;
static uint64_t g_last_time = 0;

// ── GLFW Input Backend (PS4 gamepad via scePad) ────────────────────────

bool ImGui_ImplGlfw_InitForOpenGL(void* window, bool install_callbacks)
{
    (void)window;
    (void)install_callbacks;

    printf("[ImGui PS4] Initializing gamepad backend (scePad)\n");

    if (!g_pad_initialized) {
        scePadInit();

        OrbisUserServiceInitializeParams param;
        memset(&param, 0, sizeof(param));
        param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
        sceUserServiceInitialize(&param);

        OrbisUserServiceUserId userId;
        if (sceUserServiceGetInitialUser(&userId) == 0) {
            g_pad_handle = scePadOpen(userId, ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL);
            if (g_pad_handle >= 0) {
                printf("[ImGui PS4] Controller opened (handle=%d)\n", g_pad_handle);
            } else {
                printf("[ImGui PS4] Failed to open controller: 0x%X\n", g_pad_handle);
                g_pad_handle = -1;
            }
        } else {
            printf("[ImGui PS4] Failed to get user ID\n");
        }
        g_pad_initialized = true;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.BackendPlatformName = "ps4_gamepad";
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

    g_last_time = sceKernelGetProcessTime();
    return true;
}

void ImGui_ImplGlfw_Shutdown()
{
    printf("[ImGui PS4] Shutting down gamepad backend\n");
    if (g_pad_handle >= 0) {
        scePadClose(g_pad_handle);
        g_pad_handle = -1;
    }
    g_pad_initialized = false;
}

void ImGui_ImplGlfw_NewFrame()
{
    ImGuiIO& io = ImGui::GetIO();

    // Update delta time using sceKernelGetProcessTime (microseconds)
    uint64_t now = sceKernelGetProcessTime();
    if (g_last_time > 0) {
        io.DeltaTime = (float)(now - g_last_time) / 1000000.0f;
    } else {
        io.DeltaTime = 1.0f / 60.0f;
    }
    g_last_time = now;

    if (g_pad_handle < 0) {
        return;
    }

    OrbisPadData pad_data;
    memset(&pad_data, 0, sizeof(pad_data));
    if (scePadReadState(g_pad_handle, &pad_data) != 0) {
        return;
    }

    // Helper: map Orbis button to ImGui key
    auto map_button = [&](unsigned int orbis_btn, ImGuiKey imgui_key) {
        bool pressed = (pad_data.buttons & orbis_btn) != 0;
        io.AddKeyEvent(imgui_key, pressed);
    };

    // Face buttons
    map_button(ORBIS_PAD_BUTTON_CROSS,    ImGuiKey_GamepadFaceDown);   // Cross → A
    map_button(ORBIS_PAD_BUTTON_CIRCLE,   ImGuiKey_GamepadFaceRight);  // Circle → B
    map_button(ORBIS_PAD_BUTTON_SQUARE,   ImGuiKey_GamepadFaceLeft);   // Square → X
    map_button(ORBIS_PAD_BUTTON_TRIANGLE, ImGuiKey_GamepadFaceUp);     // Triangle → Y

    // D-pad
    map_button(ORBIS_PAD_BUTTON_LEFT,     ImGuiKey_GamepadDpadLeft);
    map_button(ORBIS_PAD_BUTTON_RIGHT,    ImGuiKey_GamepadDpadRight);
    map_button(ORBIS_PAD_BUTTON_UP,       ImGuiKey_GamepadDpadUp);
    map_button(ORBIS_PAD_BUTTON_DOWN,     ImGuiKey_GamepadDpadDown);

    // Shoulder buttons
    map_button(ORBIS_PAD_BUTTON_L1,       ImGuiKey_GamepadL1);
    map_button(ORBIS_PAD_BUTTON_R1,       ImGuiKey_GamepadR1);

    // Stick buttons
    map_button(ORBIS_PAD_BUTTON_L3,       ImGuiKey_GamepadL3);
    map_button(ORBIS_PAD_BUTTON_R3,       ImGuiKey_GamepadR3);

    // Start/Back
    map_button(ORBIS_PAD_BUTTON_OPTIONS,  ImGuiKey_GamepadStart);
    map_button(ORBIS_PAD_BUTTON_TOUCH_PAD, ImGuiKey_GamepadBack);

    // Helper: normalize stick axis (0-255, center ~128) to -1..1
    auto stick_val = [](uint8_t raw) -> float {
        return (raw - 128.0f) / 127.0f;
    };

    // Left analog stick
    {
        float lx = stick_val(pad_data.leftStick.x);
        float ly = stick_val(pad_data.leftStick.y);
        io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft,  lx < -0.15f, -lx);
        io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, lx >  0.15f,  lx);
        io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp,    ly < -0.15f, -ly);
        io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown,  ly >  0.15f,  ly);
    }

    // Right analog stick
    {
        float rx = stick_val(pad_data.rightStick.x);
        float ry = stick_val(pad_data.rightStick.y);
        io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickLeft,  rx < -0.15f, -rx);
        io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickRight, rx >  0.15f,  rx);
        io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickUp,    ry < -0.15f, -ry);
        io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickDown,  ry >  0.15f,  ry);
    }

    // Analog triggers (0 → 1, no negative axis)
    {
        float lt = pad_data.analogButtons.l2 / 255.0f;
        float rt = pad_data.analogButtons.r2 / 255.0f;
        io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, lt > 0.15f, lt);
        io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, rt > 0.15f, rt);
    }
}
