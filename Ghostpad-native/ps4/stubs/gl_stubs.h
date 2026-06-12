// Minimal OpenGL stubs for PS4 console builds (GHOSTPAD_CONSOLE)
// Provides no-op macros for OpenGL functions used by Ghostpad's app.cpp
// On console, SDL2 renderer handles all rendering; GL calls are not needed

#pragma once

// Texture functions (return fake IDs on console)
#define glGenTextures(n, ids)       do { if (ids) *(ids) = 1; } while(0)
#define glBindTexture(target, tex)  ((void)0)
#define glTexParameteri(target, pname, param) ((void)0)
#define glTexImage2D(target, level, internalformat, w, h, border, format, type, pixels) ((void)0)

// Render state
#define glViewport(x, y, w, h)      ((void)0)
#define glClearColor(r, g, b, a)    ((void)0)
#define glClear(mask)               ((void)0)
#define glReadPixels(x, y, w, h, format, type, pixels) ((void)0)

// Other GL functions that may be used
#define glEnable(cap)               ((void)0)
#define glDisable(cap)              ((void)0)
#define glBlendFunc(sfactor, dfactor) ((void)0)
#define glScissor(x, y, w, h)       ((void)0)

// GL types (if needed)
#define GL_TRUE  1
#define GL_FALSE 0
typedef unsigned int GLuint;
typedef int GLint;
typedef unsigned char GLboolean;

// SDL2 compatibility (PS4 SDL2 port uses older API)
#define SDL_ScaleModeLinear 1
#define SDL_SetTextureScaleMode(t, s) ((void)0)
