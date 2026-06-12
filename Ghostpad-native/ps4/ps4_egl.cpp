// PS4 EGL/GLES2 initialization
// Based on OpenOrbis ootk_GraphicsManager pattern (thanks flat_z!)

#include <cstdint>
#include <orbis/Pigletv2VSH.h>
#include <cstring>
#include <cstdio>
#include "ps4_egl.h"

bool ps4_egl_init(PS4EglState* state, int width, int height) {
    if (!state) return false;
    
    state->width = width;
    state->height = height;
    
    // Configure Piglet (PS4 GPU context manager)
    OrbisPglConfig pgl_config{};
    OrbisPglWindow render_window{
        0,
        static_cast<khronos_uint32_t>(width),
        static_cast<khronos_uint32_t>(height)
    };
    
    EGLConfig config{};
    EGLint num_configs = 0;
    int major = -1, minor = -1;
    
    const EGLint attribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_SAMPLE_BUFFERS, 0,
        EGL_SAMPLES, 0,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE,
    };
    
    const EGLint ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE,
    };
    
    const EGLint window_attribs[] = {
        EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
        EGL_NONE,
    };
    
    std::memset(&pgl_config, 0, sizeof(pgl_config));
    pgl_config.size = sizeof(pgl_config);
    pgl_config.flags = ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT | ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY | 0x60;
    pgl_config.processOrder = 1;
    pgl_config.systemSharedMemorySize = 250 * 1024 * 1024;
    pgl_config.videoSharedMemorySize = 512 * 1024 * 1024;
    pgl_config.maxMappedFlexibleMemory = 170 * 1024 * 1024;
    pgl_config.drawCommandBufferSize = 1 * 1024 * 1024;
    pgl_config.lcueResourceBufferSize = 1 * 1024 * 1024;
    pgl_config.dbgPosCmd_0x40 = width;
    pgl_config.dbgPosCmd_0x44 = height;
    pgl_config.dbgPosCmd_0x48 = 0;
    pgl_config.dbgPosCmd_0x4C = 0;
    pgl_config.unk_0x5C = 2;
    
    if (!scePigletSetConfigurationVSH(&pgl_config)) {
        printf("[PS4 EGL] Failed to set piglet configuration\n");
        return false;
    }
    
    state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (state->display == EGL_NO_DISPLAY) {
        printf("[PS4 EGL] Failed to get EGL display: 0x%X\n", eglGetError());
        return false;
    }
    
    if (eglInitialize(state->display, &major, &minor) == EGL_FALSE) {
        printf("[PS4 EGL] Failed to initialize EGL: 0x%X\n", eglGetError());
        return false;
    }
    printf("[PS4 EGL] Initialized EGL %d.%d\n", major, minor);
    
    if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE) {
        printf("[PS4 EGL] Failed to bind EGL API: 0x%X\n", eglGetError());
        return false;
    }
    
    if (eglSwapInterval(state->display, 0) == EGL_FALSE) {
        printf("[PS4 EGL] Failed to set swap interval: 0x%X\n", eglGetError());
        return false;
    }
    
    if (eglChooseConfig(state->display, attribs, &config, 1, &num_configs) == EGL_FALSE) {
        printf("[PS4 EGL] Failed to choose EGL config: 0x%X\n", eglGetError());
        return false;
    }
    
    if (num_configs < 1) {
        printf("[PS4 EGL] No EGL configs found\n");
        return false;
    }
    
    state->surface = eglCreateWindowSurface(state->display, config, &render_window, window_attribs);
    if (state->surface == EGL_NO_SURFACE) {
        printf("[PS4 EGL] Failed to create EGL surface: 0x%X\n", eglGetError());
        return false;
    }
    
    state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, ctx_attribs);
    if (state->context == EGL_NO_CONTEXT) {
        printf("[PS4 EGL] Failed to create EGL context: 0x%X\n", eglGetError());
        return false;
    }
    
    if (eglMakeCurrent(state->display, state->surface, state->surface, state->context) == EGL_FALSE) {
        printf("[PS4 EGL] Failed to make EGL current: 0x%X\n", eglGetError());
        return false;
    }
    
    printf("[PS4 EGL] Display %dx%d initialized\n", width, height);
    return true;
}

void ps4_egl_swap(PS4EglState* state) {
    if (state && state->display != EGL_NO_DISPLAY && state->surface != EGL_NO_SURFACE) {
        eglSwapBuffers(state->display, state->surface);
    }
}

void ps4_egl_cleanup(PS4EglState* state) {
    if (!state) return;
    
    if (state->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (state->context != EGL_NO_CONTEXT) {
            eglDestroyContext(state->display, state->context);
        }
        if (state->surface != EGL_NO_SURFACE) {
            eglDestroySurface(state->display, state->surface);
        }
        eglTerminate(state->display);
    }
    printf("[PS4 EGL] Cleanup complete\n");
}
