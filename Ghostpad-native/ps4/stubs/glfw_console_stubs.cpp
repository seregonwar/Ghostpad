// GLFW Console Stubs for PS4
// Provides GLFW window/event stubs + native scePad gamepad input

#include "glfw_console_stubs.h"

// BSD compatibility types (needed by kernel.h)
typedef unsigned short u_short;
typedef unsigned int   u_int;

#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
#include <cstdio>
#include <cstring>

// ── Window state ──────────────────────────────────────────────────
struct GLFWwindow {
    void* user_ptr;
    GLFWkeyfun         key_cb;
    GLFWmousebuttonfun mouse_cb;
    GLFWcursorposfun   cursor_cb;
    GLFWscrollfun      scroll_cb;
    GLFWwindowsizefun  size_cb;
    GLFWwindowrefreshfun refresh_cb;
    bool should_close;
    int width, height;
};

static GLFWwindow  g_window = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, false, 1920, 1080};
static GLFWerrorfun g_error_cb = nullptr;

// ── Gamepad state ──────────────────────────────────────────────────
static int  g_glfw_pad = -1;
static bool g_glfw_pad_init = false;

static int get_pad() {
    if (!g_glfw_pad_init) {
        scePadInit();
        OrbisUserServiceInitializeParams param;
        memset(&param, 0, sizeof(param));
        param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
        sceUserServiceInitialize(&param);
        OrbisUserServiceUserId userId;
        if (sceUserServiceGetInitialUser(&userId) == 0) {
            g_glfw_pad = scePadOpen(userId, ORBIS_PAD_PORT_TYPE_STANDARD, 0, nullptr);
        }
        g_glfw_pad_init = true;
    }
    return g_glfw_pad;
}

static float stick_val(uint8_t raw) {
    return (raw - 128.0f) / 127.0f;
}

// ── GLFW Init / Terminate ─────────────────────────────────────────
int glfwInit(void) {
    printf("[GLFW Stub] glfwInit\n");
    return 1;
}

void glfwTerminate(void) {
    printf("[GLFW Stub] glfwTerminate\n");
}

GLFWerrorfun glfwSetErrorCallback(GLFWerrorfun callback) {
    GLFWerrorfun prev = g_error_cb;
    g_error_cb = callback;
    return prev;
}

const char* glfwGetVersionString(void) {
    return "3.3.0 PS4 stub";
}

// ── Window Hints ──────────────────────────────────────────────────
void glfwWindowHint(int hint, int value) {
    (void)hint; (void)value;
}

void glfwDefaultWindowHints(void) {
}

// ── Window Creation / Management ──────────────────────────────────
GLFWwindow* glfwCreateWindow(int width, int height, const char* title, void* monitor, void* share) {
    (void)title; (void)monitor; (void)share;
    g_window.width  = width  > 0 ? width  : 1920;
    g_window.height = height > 0 ? height : 1080;
    printf("[GLFW Stub] glfwCreateWindow %dx%d\n", g_window.width, g_window.height);
    return &g_window;
}

void glfwDestroyWindow(GLFWwindow* window) {
    (void)window;
}

int glfwWindowShouldClose(GLFWwindow* window) {
    return window ? window->should_close : 1;
}

void glfwSetWindowShouldClose(GLFWwindow* window, int value) {
    if (window) window->should_close = (value != 0);
}

void glfwShowWindow(GLFWwindow* window) {
    (void)window;
}

void glfwHideWindow(GLFWwindow* window) {
    (void)window;
}

void glfwMakeContextCurrent(GLFWwindow* window) {
    printf("[GLFW Stub] glfwMakeContextCurrent\n");
    (void)window;
}

void glfwSwapBuffers(GLFWwindow* window) {
    (void)window;
}

void glfwSwapInterval(int interval) {
    (void)interval;
}

void glfwSetWindowUserPointer(GLFWwindow* window, void* pointer) {
    if (window) window->user_ptr = pointer;
}

void* glfwGetWindowUserPointer(GLFWwindow* window) {
    return window ? window->user_ptr : nullptr;
}

void glfwGetWindowSize(GLFWwindow* window, int* width, int* height) {
    if (window) {
        if (width)  *width  = window->width;
        if (height) *height = window->height;
    }
}

void glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height) {
    glfwGetWindowSize(window, width, height);
}

// ── Input: Keyboard / Mouse ───────────────────────────────────────
int glfwGetKey(GLFWwindow* window, int key) {
    (void)window; (void)key;
    return 0;
}

int glfwGetMouseButton(GLFWwindow* window, int button) {
    (void)window; (void)button;
    return 0;
}

void glfwGetCursorPos(GLFWwindow* window, double* xpos, double* ypos) {
    if (xpos) *xpos = 0.0;
    if (ypos) *ypos = 0.0;
}

// ── Input: Callbacks ──────────────────────────────────────────────
GLFWkeyfun glfwSetKeyCallback(GLFWwindow* window, GLFWkeyfun callback) {
    GLFWkeyfun old = window ? window->key_cb : nullptr;
    if (window) window->key_cb = callback;
    return old;
}

GLFWcharfun glfwSetCharCallback(GLFWwindow* window, GLFWcharfun callback) {
    (void)window; (void)callback;
    return nullptr;
}

GLFWmousebuttonfun glfwSetMouseButtonCallback(GLFWwindow* window, GLFWmousebuttonfun callback) {
    GLFWmousebuttonfun old = window ? window->mouse_cb : nullptr;
    if (window) window->mouse_cb = callback;
    return old;
}

GLFWcursorposfun glfwSetCursorPosCallback(GLFWwindow* window, GLFWcursorposfun callback) {
    GLFWcursorposfun old = window ? window->cursor_cb : nullptr;
    if (window) window->cursor_cb = callback;
    return old;
}

GLFWscrollfun glfwSetScrollCallback(GLFWwindow* window, GLFWscrollfun callback) {
    GLFWscrollfun old = window ? window->scroll_cb : nullptr;
    if (window) window->scroll_cb = callback;
    return old;
}

GLFWwindowsizefun glfwSetWindowSizeCallback(GLFWwindow* window, GLFWwindowsizefun callback) {
    GLFWwindowsizefun old = window ? window->size_cb : nullptr;
    if (window) window->size_cb = callback;
    return old;
}

GLFWwindowrefreshfun glfwSetWindowRefreshCallback(GLFWwindow* window, GLFWwindowrefreshfun callback) {
    GLFWwindowrefreshfun old = window ? window->refresh_cb : nullptr;
    if (window) window->refresh_cb = callback;
    return old;
}

void glfwPollEvents(void) {
    // No events to poll on PS4 console
}

// ── Time ──────────────────────────────────────────────────────────
double glfwGetTime(void) {
    return (double)sceKernelGetProcessTime() / 1000000.0;
}

// ── Gamepad / Joystick ────────────────────────────────────────────
int glfwJoystickPresent(int jid) {
    if (jid != 0) return 0;
    return get_pad() >= 0 ? 1 : 0;
}

const char* glfwGetJoystickName(int jid) {
    (void)jid;
    return "Wireless Controller";
}

const float* glfwGetJoystickAxes(int jid, int* count) {
    (void)jid;
    if (count) *count = 0;
    return nullptr;
}

const unsigned char* glfwGetJoystickButtons(int jid, int* count) {
    (void)jid;
    if (count) *count = 0;
    return nullptr;
}

int glfwGetGamepadState(int jid, GLFWgamepadstate* state) {
    if (jid != 0 || !state) return 0;
    int pad = get_pad();
    if (pad < 0) return 0;

    OrbisPadData data;
    memset(&data, 0, sizeof(data));
    if (scePadReadState(pad, &data) != 0) {
        return 0;
    }

    unsigned int btn = data.buttons;
    state->buttons[0]  = (btn & ORBIS_PAD_BUTTON_CROSS)    ? 1 : 0;  // A
    state->buttons[1]  = (btn & ORBIS_PAD_BUTTON_CIRCLE)   ? 1 : 0;  // B
    state->buttons[2]  = (btn & ORBIS_PAD_BUTTON_SQUARE)   ? 1 : 0;  // X
    state->buttons[3]  = (btn & ORBIS_PAD_BUTTON_TRIANGLE) ? 1 : 0;  // Y
    state->buttons[4]  = (btn & ORBIS_PAD_BUTTON_L1)       ? 1 : 0;  // LB
    state->buttons[5]  = (btn & ORBIS_PAD_BUTTON_R1)       ? 1 : 0;  // RB
    state->buttons[6]  = (btn & ORBIS_PAD_BUTTON_TOUCH_PAD)? 1 : 0;  // BACK
    state->buttons[7]  = (btn & ORBIS_PAD_BUTTON_OPTIONS)  ? 1 : 0;  // START
    state->buttons[8]  = 0;                                           // GUIDE
    state->buttons[9]  = (btn & ORBIS_PAD_BUTTON_L3)       ? 1 : 0;  // LEFT_THUMB
    state->buttons[10] = (btn & ORBIS_PAD_BUTTON_R3)       ? 1 : 0;  // RIGHT_THUMB
    state->buttons[11] = (btn & ORBIS_PAD_BUTTON_UP)       ? 1 : 0;  // DPAD_UP
    state->buttons[12] = (btn & ORBIS_PAD_BUTTON_RIGHT)    ? 1 : 0;  // DPAD_RIGHT
    state->buttons[13] = (btn & ORBIS_PAD_BUTTON_DOWN)     ? 1 : 0;  // DPAD_DOWN
    state->buttons[14] = (btn & ORBIS_PAD_BUTTON_LEFT)     ? 1 : 0;  // DPAD_LEFT

    state->axes[0] = stick_val(data.leftStick.x);    // LEFT_X
    state->axes[1] = -stick_val(data.leftStick.y);   // LEFT_Y (GLFW: up=+1)
    state->axes[2] = stick_val(data.rightStick.x);   // RIGHT_X
    state->axes[3] = -stick_val(data.rightStick.y);  // RIGHT_Y (GLFW: up=+1)
    state->axes[4] = data.analogButtons.l2 / 255.0f; // LEFT_TRIGGER
    state->axes[5] = data.analogButtons.r2 / 255.0f; // RIGHT_TRIGGER

    return 1;
}
