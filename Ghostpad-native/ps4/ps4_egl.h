#pragma once
// PS4 EGL/GLES2 initialization for homebrew rendering
// Based on OpenOrbis ootk_GraphicsManager pattern

#include <EGL/egl.h>
#include <GLES2/gl2.h>

struct PS4EglState {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int width;
    int height;
};

bool ps4_egl_init(PS4EglState* state, int width, int height);
void ps4_egl_swap(PS4EglState* state);
void ps4_egl_cleanup(PS4EglState* state);
