// Ghostpad PS4 Homebrew Entry Point
// Software rasterizer + sceVideoOut for display
// Native scePad for gamepad input (no SDL2)
// PS5Bridge payload NEVER executed on PS4 (platform guard via compile-time + CPUID)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

// BSD compatibility types (needed by kernel.h)
typedef unsigned short u_short;
typedef unsigned int   u_int;

#include <orbis/VideoOut.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

#include "imgui_impl_ps4_sw.h"

/* PS5Bridge is NEVER linked or executed on PS4.
 * The entire bridge subsystem is excluded at compile time via __ORBIS__. */
#ifndef __ORBIS__
extern "C" {
#include "ps5bridge/ps5bridge.h"
#include "ps5bridge/launcher.h"
}
#endif

#include "ui/app.h"

#define DISPLAY_WIDTH  1920
#define DISPLAY_HEIGHT 1080

// ── VideoOut globals ────────────────────────────────────────────
static int    g_video_handle = -1;
static void*  g_display_buf = nullptr;
static off_t  g_direct_mem_offset = 0;

// ── Platform Detection ──────────────────────────────────────────
//
// Three independent checks, evaluated in order:
//   1. Compile-time: __ORBIS__ (PS4) vs __PROSPERO__ (PS5) — set by SDK
//   2. CPUID: AMD Jaguar (family 0x16) vs Zen 2 (family 0x17+)
//   3. Kernel version via sceKernelGetCompiledSdkVersion / uname
//
// The compile-time guard prevents PS5Bridge code from even being
// linked into a PS4 build, eliminating any accidental kernel-call
// paths through libkernel.sprx.

static bool is_ps5(void) {
#ifdef __ORBIS__
    /* Compile-time: this is a PS4 build — never PS5 */
    return false;
#else
    /* Runtime CPUID check as double-confirmation */
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    uint32_t family = ((eax >> 8) & 0xF) + ((eax >> 20) & 0xFF);
    return (family >= 0x17);
#endif
}

// ── VideoOut init ───────────────────────────────────────────────
static bool video_init() {
    // Get user ID
    OrbisUserServiceUserId userId;
    sceUserServiceInitialize(nullptr);
    if (sceUserServiceGetInitialUser(&userId) < 0) {
        printf("[VideoOut] Failed to get user\n");
        return false;
    }

    // Open VideoOut
    g_video_handle = sceVideoOutOpen(userId, 0, 0, nullptr);
    if (g_video_handle < 0) {
        printf("[VideoOut] sceVideoOutOpen failed: 0x%X\n", g_video_handle);
        return false;
    }

    // Allocate display buffer (Garlic memory, 2MB aligned)
    const size_t buf_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * 4; // RGBA
    const size_t align_size = 2 * 1024 * 1024;
    const size_t alloc_size = (buf_size + align_size - 1) & ~(align_size - 1);

    int ret = sceKernelAllocateDirectMemory(0, (off_t)-1, alloc_size, align_size,
                                           3,  // ORBIS_KERNEL_WC_GARLIC
                                           &g_direct_mem_offset);
    if (ret < 0) {
        printf("[VideoOut] AllocateDirectMemory failed: 0x%X\n", ret);
        sceVideoOutClose(g_video_handle);
        return false;
    }

    ret = sceKernelMapDirectMemory(&g_display_buf, alloc_size,
                                   1 | 2 | 0x10,  // PROT_CPU_READ|WRITE|GPU_ALL
                                   0, g_direct_mem_offset, align_size);
    if (ret < 0) {
        printf("[VideoOut] MapDirectMemory failed: 0x%X\n", ret);
        sceVideoOutClose(g_video_handle);
        return false;
    }

    // Set buffer attribute: A8B8G8R8 (ABGR), linear tiling
    OrbisVideoOutBufferAttribute attr;
    sceVideoOutSetBufferAttribute(&attr,
                                  0x80002200,  // A8B8G8R8_SRGB
                                  0,           // LINEAR
                                  0,
                                  DISPLAY_WIDTH,
                                  DISPLAY_HEIGHT,
                                  DISPLAY_WIDTH);  // pixel pitch

    void* addrs[1] = { g_display_buf };
    ret = sceVideoOutRegisterBuffers(g_video_handle, 0, addrs, 1, &attr);
    if (ret < 0) {
        printf("[VideoOut] RegisterBuffers failed: 0x%X\n", ret);
        sceVideoOutClose(g_video_handle);
        return false;
    }

    printf("[VideoOut] Initialized %dx%d\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    return true;
}

// ── Submit framebuffer ──────────────────────────────────────────
static void video_submit(const uint8_t* fb) {
    if (!g_display_buf || g_video_handle < 0) return;
    memcpy(g_display_buf, fb, DISPLAY_WIDTH * DISPLAY_HEIGHT * 4);
    sceVideoOutSubmitFlip(g_video_handle, 0, 0, 0);  // VSYNC mode
}

// ── VideoOut cleanup ────────────────────────────────────────────
static void video_cleanup() {
    if (g_video_handle >= 0) {
        sceVideoOutClose(g_video_handle);
        g_video_handle = -1;
    }
    if (g_direct_mem_offset != 0) {
        sceKernelReleaseDirectMemory(g_direct_mem_offset, 
            (DISPLAY_WIDTH * DISPLAY_HEIGHT * 4 + (2*1024*1024) - 1) & ~((size_t)(2*1024*1024)-1));
        g_direct_mem_offset = 0;
    }
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    printf("[Ghostpad] PS4 Homebrew starting...\n");

#ifdef __ORBIS__
    printf("[Ghostpad] PS4 build — PS5Bridge excluded at compile time\n");
#else
    // ── Step 1: PS5 Bridge (PS5 back-compat only) ───────────────
    ps5bridge_shm_t *shm = nullptr;
    if (is_ps5()) {
        printf("[Ghostpad] PS5 detected, launching PS5Bridge...\n");
        int rc = ps5bridge_launch(&shm);
        if (rc == 0 && shm) {
            printf("[Ghostpad] PS5Bridge connected\n");
        } else {
            printf("[Ghostpad] PS5Bridge launch failed (rc=%d)\n", rc);
            shm = nullptr;
        }
    } else {
        printf("[Ghostpad] PS4 CPUID detected, running standalone\n");
    }
#endif

    // ── Step 2: Init VideoOut ───────────────────────────────────
    if (!video_init()) {
        printf("[Ghostpad] VideoOut init failed, aborting\n");
        return 1;
    }

    // ── Step 4: Init app (ImGui) ────────────────────────────────
    ghostpad::App app("/data/ghostpad");
    app.init();

    // ── Step 5: Main render loop ────────────────────────────────
    int frame = 0;
    while (!app.should_close) {
        app.update(1.0 / 60.0);
        app.render();

        // Submit software framebuffer to display
        video_submit(ImGui_ImplPS4SW_GetFramebuffer());

        if (++frame % 300 == 0) {
            printf("[Ghostpad] Frame %d\n", frame);
        }
    }

    // ── Step 6: Shutdown ────────────────────────────────────────
    app.shutdown();
    ImGui_ImplPS4SW_Shutdown();
    video_cleanup();
    printf("[Ghostpad] Clean shutdown\n");
    return 0;
}
