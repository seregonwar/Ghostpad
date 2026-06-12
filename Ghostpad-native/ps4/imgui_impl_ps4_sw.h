// ImGui PS4 Software Rasterizer Backend
// Renders ImGui draw data to an RGBA8888 framebuffer via CPU rasterization
// The framebuffer is then uploaded to SDL_Texture for display

#pragma once

#include <cstdint>
#include "imgui.h"

// Initialize the backend - allocates framebuffer, captures font texture
bool ImGui_ImplPS4SW_Init(int width, int height);

// Shutdown - frees framebuffer
void ImGui_ImplPS4SW_Shutdown();

// Call at start of each frame (clears framebuffer)
void ImGui_ImplPS4SW_NewFrame();

// Render ImGui draw data to the internal framebuffer
void ImGui_ImplPS4SW_RenderDrawData(ImDrawData* draw_data);

// Get the framebuffer for SDL upload (RGBA8888, size = width*height*4)
const uint8_t* ImGui_ImplPS4SW_GetFramebuffer();
int ImGui_ImplPS4SW_GetWidth();
int ImGui_ImplPS4SW_GetHeight();
