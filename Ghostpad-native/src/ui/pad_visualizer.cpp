// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "ui/native_theme.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "GLFW/glfw3.h"

namespace ghostpad {

/*
 *  TEXTURE LOADER AND CONTEXT
 */
static GLuint g_controller_texture = 0;
static int g_tex_w = 0;
static int g_tex_h = 0;

static void loadControllerTexture() {
    if (g_controller_texture != 0) return;

    int width, height, channels;
    unsigned char* data = stbi_load(CONTROLLER_IMAGE_PATH, &width, &height, &channels, 4);
    if (!data) {
        fprintf(stderr, "[Ghostpad] Failed to load controller texture from %s: %s\n", 
                CONTROLLER_IMAGE_PATH, stbi_failure_reason());
        return;
    }

    g_tex_w = width;
    g_tex_h = height;

    // Convert solid black background to transparency channel using color luminance
    for (int i = 0; i < width * height; ++i) {
        unsigned char r = data[i * 4 + 0];
        unsigned char g = data[i * 4 + 1];
        unsigned char b = data[i * 4 + 2];
        unsigned char max_val = std::max({r, g, b});
        data[i * 4 + 3] = max_val;
    }

    glGenTextures(1, &g_controller_texture);
    glBindTexture(GL_TEXTURE_2D, g_controller_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    stbi_image_free(data);
}

struct InteractivePadLayout {
    float w, h, size;
    ImVec2 base;
    float cx, cy;

    // Sticks
    ImVec2 l_stick_c, r_stick_c;
    float stick_radius;

    // D-pad
    ImVec2 dpad_center;
    float dpad_size;

    // Face buttons
    ImVec2 fb_center;
    float fb_radius, fb_dist;

    // Shoulders
    float shoulder_y;
    float trig_width, trig_height;
};

/*
 *  ┌──────────────────────────────────────────────────────────┐
 *  │                   LAYOUT COMPONENTS                      │
 *  └──────────────────────────────────────────────────────────┘
 */
enum LayoutComponent {
    LC_None = 0,
    LC_LeftStick,
    LC_RightStick,
    LC_Dpad,
    LC_FaceButtons,
    LC_ShouldersL,
    LC_ShouldersR,
    LC_Touchpad,
    LC_CenterButtons,
    LC_Count
};

// ─────────────────────────────────────────────────────────────────────────────
//                             LAYOUT COMPUTATION
// ─────────────────────────────────────────────────────────────────────────────
static InteractivePadLayout computeLayout(ImVec2 base, float size, const PadLayoutSettings& layout) {
    InteractivePadLayout l;
    l.base = base;
    l.size = size;
    l.w = size * 2.0f;
    l.h = size * 1.3f;

    // Apply alignment shift to coordinate system
    float shift_x = -size * 0.026f;
    float shift_y = +size * 0.030f;

    l.cx = base.x + l.w * 0.5f + shift_x;
    l.cy = base.y + l.h * 0.45f + shift_y;

    /*
     *     L_STICK (o)   (o) R_STICK
     */
    // Sticks: Centered at 367.0 and 657.0 in pixels (Y=519.5 due to 3D cap perspective)
    l.l_stick_c = ImVec2(l.cx - size * 0.283f + layout.l_stick.x_offset * size,
                         l.cy + size * 0.015f + layout.l_stick.y_offset * size);
    l.r_stick_c = ImVec2(l.cx + size * 0.283f + layout.r_stick.x_offset * size,
                         l.cy + size * 0.015f + layout.r_stick.y_offset * size);
    l.stick_radius = size * 0.11f;

    // D-pad on the left: Centered at 217.18, 390.55 in pixels
    l.dpad_center = ImVec2(l.cx - size * 0.576f + layout.dpad.x_offset * size,
                           l.cy - size * 0.237f + layout.dpad.y_offset * size);
    l.dpad_size = size * 0.06f;

    // Face buttons on the right: Centered at 815.40, 382.82 in pixels
    l.fb_center = ImVec2(l.cx + size * 0.593f + layout.face_buttons.x_offset * size,
                         l.cy - size * 0.252f + layout.face_buttons.y_offset * size);
    l.fb_radius = size * 0.050f;
    l.fb_dist = size * 0.092f;

    return l;
}

/*
 *  ┌──────────────────────────────────────────────────────────┐
 *  │               EDIT MODE HOVER DETECTION                  │
 *  └──────────────────────────────────────────────────────────┘
 */
static int getHoveredComponent(ImVec2 mouse_pos, const InteractivePadLayout& l, const PadLayoutSettings& layout, float size) {
    auto dist = [](ImVec2 a, ImVec2 b) {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    };

    // 1. Center buttons
    float cb_r = size * 0.04f * layout.center_buttons.scale;
    float create_x = l.cx - size * 0.365f + layout.center_buttons.x_offset * size;
    float create_y = l.cy - size * 0.432f + layout.center_buttons.y_offset * size;
    float options_x = l.cx + size * 0.365f + layout.center_buttons.x_offset * size;
    float options_y = l.cy - size * 0.432f + layout.center_buttons.y_offset * size;
    float ps_x = l.cx + layout.center_buttons.x_offset * size;
    float ps_y = l.cy + size * 0.035f + layout.center_buttons.y_offset * size;

    if (dist(mouse_pos, ImVec2(create_x, create_y)) < cb_r ||
        dist(mouse_pos, ImVec2(options_x, options_y)) < cb_r ||
        dist(mouse_pos, ImVec2(ps_x, ps_y)) < cb_r) {
        return LC_CenterButtons;
    }

    // 2. Sticks
    float l_stick_r = l.stick_radius * layout.l_stick.scale;
    float r_stick_r = l.stick_radius * layout.r_stick.scale;
    if (dist(mouse_pos, l.l_stick_c) < l_stick_r) return LC_LeftStick;
    if (dist(mouse_pos, l.r_stick_c) < r_stick_r) return LC_RightStick;

    // 3. D-pad
    float dpad_r = l.dpad_size * 2.0f * layout.dpad.scale;
    if (dist(mouse_pos, l.dpad_center) < dpad_r) return LC_Dpad;

    // 4. Face buttons
    float fb_r = (l.fb_dist + l.fb_radius) * layout.face_buttons.scale;
    if (dist(mouse_pos, l.fb_center) < fb_r) return LC_FaceButtons;

    // 5. Touchpad
    float tp_cx = l.cx + layout.touchpad.x_offset * size;
    float tp_cy = l.cy - size * 0.3725f + layout.touchpad.y_offset * size;
    float tp_w = size * 0.720f * layout.touchpad.scale;
    float tp_h = size * 0.341f * layout.touchpad.scale;
    ImVec2 tp_min(tp_cx - tp_w * 0.5f, tp_cy - tp_h * 0.5f);
    ImVec2 tp_max(tp_cx + tp_w * 0.5f, tp_cy + tp_h * 0.5f);
    if (mouse_pos.x >= tp_min.x && mouse_pos.x <= tp_max.x &&
        mouse_pos.y >= tp_min.y && mouse_pos.y <= tp_max.y) {
        return LC_Touchpad;
    }

    // 6. Shoulders L
    ImVec2 l2_top(l.cx - size * 0.646f + layout.shoulders_l.x_offset * size,
                  l.cy - size * 0.61f + layout.shoulders_l.y_offset * size);
    ImVec2 l1_top(l.cx - size * 0.646f + layout.shoulders_l.x_offset * size,
                  l.cy - size * 0.525f + layout.shoulders_l.y_offset * size);
    ImVec2 sh_sz_l(size * 0.18f * layout.shoulders_l.scale, size * 0.06f * layout.shoulders_l.scale);
    ImVec2 trig_sz_l(size * 0.18f * layout.shoulders_l.scale, size * 0.08f * layout.shoulders_l.scale);
    if (mouse_pos.x >= l2_top.x && mouse_pos.x <= l2_top.x + trig_sz_l.x &&
        mouse_pos.y >= l2_top.y && mouse_pos.y <= l1_top.y + sh_sz_l.y) {
        return LC_ShouldersL;
    }

    // 7. Shoulders R
    ImVec2 r2_top(l.cx + size * 0.466f + layout.shoulders_r.x_offset * size,
                  l.cy - size * 0.61f + layout.shoulders_r.y_offset * size);
    ImVec2 r1_top(l.cx + size * 0.466f + layout.shoulders_r.x_offset * size,
                  l.cy - size * 0.525f + layout.shoulders_r.y_offset * size);
    ImVec2 sh_sz_r(size * 0.18f * layout.shoulders_r.scale, size * 0.06f * layout.shoulders_r.scale);
    ImVec2 trig_sz_r(size * 0.18f * layout.shoulders_r.scale, size * 0.08f * layout.shoulders_r.scale);
    if (mouse_pos.x >= r2_top.x && mouse_pos.x <= r2_top.x + trig_sz_r.x &&
        mouse_pos.y >= r2_top.y && mouse_pos.y <= r1_top.y + sh_sz_r.y) {
        return LC_ShouldersR;
    }

    return LC_None;
}

/*
 *  ┌──────────────────────────────────────────────────────────┐
 *  │              EDIT MODE BOUNDS HIGHLIGHTING               │
 *  └──────────────────────────────────────────────────────────┘
 */
static void drawComponentBounds(ImDrawList* dl, int comp, const InteractivePadLayout& l, const PadLayoutSettings& layout, float size, ImU32 color, float thickness) {
    auto draw_rect_around = [&](ImVec2 min_pt, ImVec2 max_pt) {
        dl->AddRect(min_pt - ImVec2(4, 4), max_pt + ImVec2(4, 4), color, 4.0f, 0, thickness);
    };

    switch (comp) {
        case LC_LeftStick:
            dl->AddCircle(l.l_stick_c, l.stick_radius * layout.l_stick.scale + 4, color, 32, thickness);
            break;
        case LC_RightStick:
            dl->AddCircle(l.r_stick_c, l.stick_radius * layout.r_stick.scale + 4, color, 32, thickness);
            break;
        case LC_Dpad:
            dl->AddCircle(l.dpad_center, l.dpad_size * 2.0f * layout.dpad.scale + 6, color, 32, thickness);
            break;
        case LC_FaceButtons:
            dl->AddCircle(l.fb_center, (l.fb_dist + l.fb_radius) * layout.face_buttons.scale + 6, color, 32, thickness);
            break;
        case LC_ShouldersL: {
            ImVec2 l2_top(l.cx - size * 0.646f + layout.shoulders_l.x_offset * size,
                          l.cy - size * 0.61f + layout.shoulders_l.y_offset * size);
            ImVec2 l1_top(l.cx - size * 0.646f + layout.shoulders_l.x_offset * size,
                          l.cy - size * 0.525f + layout.shoulders_l.y_offset * size);
            ImVec2 sh_sz_l(size * 0.18f * layout.shoulders_l.scale, size * 0.06f * layout.shoulders_l.scale);
            ImVec2 trig_sz_l(size * 0.18f * layout.shoulders_l.scale, size * 0.08f * layout.shoulders_l.scale);
            draw_rect_around(l2_top, ImVec2(l2_top.x + trig_sz_l.x, l1_top.y + sh_sz_l.y));
            break;
        }
        case LC_ShouldersR: {
            ImVec2 r2_top(l.cx + size * 0.466f + layout.shoulders_r.x_offset * size,
                          l.cy - size * 0.61f + layout.shoulders_r.y_offset * size);
            ImVec2 r1_top(l.cx + size * 0.466f + layout.shoulders_r.x_offset * size,
                          l.cy - size * 0.525f + layout.shoulders_r.y_offset * size);
            ImVec2 sh_sz_r(size * 0.18f * layout.shoulders_r.scale, size * 0.06f * layout.shoulders_r.scale);
            ImVec2 trig_sz_r(size * 0.18f * layout.shoulders_r.scale, size * 0.08f * layout.shoulders_r.scale);
            draw_rect_around(r2_top, ImVec2(r2_top.x + trig_sz_r.x, r1_top.y + sh_sz_r.y));
            break;
        }
        case LC_Touchpad: {
            float tp_cx = l.cx + layout.touchpad.x_offset * size;
            float tp_cy = l.cy - size * 0.3725f + layout.touchpad.y_offset * size;
            float tp_w = size * 0.720f * layout.touchpad.scale;
            float tp_h = size * 0.341f * layout.touchpad.scale;
            ImVec2 tp_min(tp_cx - tp_w * 0.5f, tp_cy - tp_h * 0.5f);
            ImVec2 tp_max(tp_cx + tp_w * 0.5f, tp_cy + tp_h * 0.5f);
            draw_rect_around(tp_min, tp_max);
            break;
        }
        case LC_CenterButtons: {
            float cb_r = size * 0.04f * layout.center_buttons.scale;
            float create_x = l.cx - size * 0.365f + layout.center_buttons.x_offset * size;
            float create_y = l.cy - size * 0.432f + layout.center_buttons.y_offset * size;
            float options_x = l.cx + size * 0.365f + layout.center_buttons.x_offset * size;
            float options_y = l.cy - size * 0.432f + layout.center_buttons.y_offset * size;
            float ps_x = l.cx + layout.center_buttons.x_offset * size;
            float ps_y = l.cy + size * 0.035f + layout.center_buttons.y_offset * size;

            dl->AddCircle(ImVec2(create_x, create_y), cb_r + 4, color, 16, thickness);
            dl->AddCircle(ImVec2(options_x, options_y), cb_r + 4, color, 16, thickness);
            dl->AddCircle(ImVec2(ps_x, ps_y), cb_r + 4, color, 16, thickness);
            break;
        }
        default:
            break;
    }
}

void renderInteractivePadVisualizer(App& app, PadStateInput& state, float size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 base = ImGui::GetCursorScreenPos();
    const auto& palette = ui::colors();

    // Use temp layout buffer if in edit mode, otherwise use active persistent settings
    const PadLayoutSettings& layout = app.is_layout_edit_mode ? app.temp_layout : app.settings.read().pad_layout;

    auto l = computeLayout(base, size, layout);
    float cx = l.cx;
    float cy = l.cy;

    // ─────────────────────────────────────────────────────────────────────────────
    //                        CONTROLLER SILHOUETTE DRAWING
    // ─────────────────────────────────────────────────────────────────────────────
    if (g_controller_texture == 0) {
        loadControllerTexture();
    }

    if (g_controller_texture != 0) {
        // Draw the controller image scaled exactly to size * 2.0f and centered on the shifted coordinates
        float img_size = size * 2.0f; 
        
        ImVec2 p_min(cx - img_size * 0.5f, cy - img_size * 0.5f);
        ImVec2 p_max(cx + img_size * 0.5f, cy + img_size * 0.5f);
        
        dl->AddImage((ImTextureID)(intptr_t)g_controller_texture, p_min, p_max, ImVec2(0,0), ImVec2(1,1), ui::u32(ui::rgba(255, 255, 255, 255)));
    } else {
        // Fallback: draw outline body vector representation
        dl->AddCircleFilled(ImVec2(cx - size * 0.52f, cy + size * 0.22f), size * 0.34f, ui::u32(ui::rgba(20, 18, 25, 230)));
        dl->AddCircleFilled(ImVec2(cx + size * 0.52f, cy + size * 0.22f), size * 0.34f, ui::u32(ui::rgba(20, 18, 25, 230)));
        dl->AddRectFilled(ImVec2(cx - size * 0.65f, cy - size * 0.35f), ImVec2(cx + size * 0.65f, cy + size * 0.3f), ui::u32(ui::rgba(20, 18, 25, 230)), 35.0f);
        dl->AddRectFilled(ImVec2(cx - size * 0.82f, cy + size * 0.1f), ImVec2(cx - size * 0.5f, cy + size * 0.62f), ui::u32(ui::rgba(20, 18, 25, 230)), 28.0f);
        dl->AddRectFilled(ImVec2(cx + size * 0.5f, cy + size * 0.1f), ImVec2(cx + size * 0.82f, cy + size * 0.62f), ui::u32(ui::rgba(20, 18, 25, 230)), 28.0f);

        std::vector<ImVec2> pts = {
            ImVec2(cx - size * 0.28f, cy - size * 0.38f),
            ImVec2(cx - size * 0.52f, cy - size * 0.38f),
            ImVec2(cx - size * 0.72f, cy - size * 0.26f),
            ImVec2(cx - size * 0.88f, cy - size * 0.05f),
            ImVec2(cx - size * 0.95f, cy + size * 0.22f),
            ImVec2(cx - size * 0.82f, cy + size * 0.62f),
            ImVec2(cx - size * 0.65f, cy + size * 0.58f),
            ImVec2(cx - size * 0.40f, cy + size * 0.28f),
            ImVec2(cx - size * 0.18f, cy + size * 0.24f),
            ImVec2(cx,                cy + size * 0.28f),
            ImVec2(cx + size * 0.18f, cy + size * 0.24f),
            ImVec2(cx + size * 0.40f, cy + size * 0.28f),
            ImVec2(cx + size * 0.65f, cy + size * 0.58f),
            ImVec2(cx + size * 0.82f, cy + size * 0.62f),
            ImVec2(cx + size * 0.95f, cy + size * 0.22f),
            ImVec2(cx + size * 0.88f, cy - size * 0.05f),
            ImVec2(cx + size * 0.72f, cy - size * 0.26f),
            ImVec2(cx + size * 0.52f, cy - size * 0.38f),
            ImVec2(cx + size * 0.28f, cy - size * 0.38f),
        };
        dl->AddPolyline(pts.data(), pts.size(), ui::u32(ui::withAlpha(palette.primary2, 0.8f)), ImDrawFlags_Closed, 3.0f);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //                           SHOULDER BUTTONS & TRIGGERS
    // ─────────────────────────────────────────────────────────────────────────────

    auto drawShoulderInteractive = [&](ImVec2 sp, ImVec2 sz, int id, const char* txt, const ComponentLayout& comp) {
        ImVec4 custom_col(comp.color[0], comp.color[1], comp.color[2], comp.color[3]);
        ImU32 col = state.button_states[id] ? ui::u32(custom_col) : ui::u32(ui::rgba(25, 22, 33, 160));
        dl->AddRectFilled(sp, ImVec2(sp.x + sz.x, sp.y + sz.y), col, 4.0f);
        dl->AddRect(sp, ImVec2(sp.x + sz.x, sp.y + sz.y), ui::u32(palette.border), 4.0f, 0, 1.0f);
        ImVec2 ts = ImGui::CalcTextSize(txt);
        dl->AddText(ImVec2(sp.x + (sz.x - ts.x) * 0.5f, sp.y + (sz.y - ts.y) * 0.5f), ui::u32(palette.text), txt);

        if (!app.is_layout_edit_mode) {
            ImGui::SetCursorScreenPos(sp);
            ImGui::InvisibleButton((std::string("##shldr") + txt).c_str(), sz);
            if (ImGui::IsItemActive()) state.button_states[id] = true;
            else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !ImGui::IsItemHovered()) state.button_states[id] = false;
        }
    };

    /*
     *     L2 [===]   [===] R2
     *     L1 [---]   [---] R1
     */
    ImVec2 sh_sz(size * 0.18f * layout.shoulders_l.scale, size * 0.06f * layout.shoulders_l.scale);
    ImVec2 trig_sz(size * 0.18f * layout.shoulders_l.scale, size * 0.08f * layout.shoulders_l.scale);
    
    // Position bumpers and triggers exactly on the top shoulders of the DualSense silhouette
    ImVec2 l2_top(cx - size * 0.646f + layout.shoulders_l.x_offset * size,
                  cy - size * 0.61f + layout.shoulders_l.y_offset * size);
    ImVec2 l1_top(cx - size * 0.646f + layout.shoulders_l.x_offset * size,
                  cy - size * 0.525f + layout.shoulders_l.y_offset * size);

    ImVec2 sh_sz_r(size * 0.18f * layout.shoulders_r.scale, size * 0.06f * layout.shoulders_r.scale);
    ImVec2 trig_sz_r(size * 0.18f * layout.shoulders_r.scale, size * 0.08f * layout.shoulders_r.scale);

    ImVec2 r2_top(cx + size * 0.466f + layout.shoulders_r.x_offset * size,
                  cy - size * 0.61f + layout.shoulders_r.y_offset * size);
    ImVec2 r1_top(cx + size * 0.466f + layout.shoulders_r.x_offset * size,
                  cy - size * 0.525f + layout.shoulders_r.y_offset * size);

    // Bumpers L1/R1
    drawShoulderInteractive(l1_top, sh_sz, 4, "L1", layout.shoulders_l);
    drawShoulderInteractive(r1_top, sh_sz_r, 5, "R1", layout.shoulders_r);

    // Triggers L2/R2
    float l2_val = state.trigger_l2 / 255.0f;
    float r2_val = state.trigger_r2 / 255.0f;

    ImVec4 l_trig_col(layout.shoulders_l.color[0], layout.shoulders_l.color[1], layout.shoulders_l.color[2], layout.shoulders_l.color[3]);
    ImVec4 r_trig_col(layout.shoulders_r.color[0], layout.shoulders_r.color[1], layout.shoulders_r.color[2], layout.shoulders_r.color[3]);

    // L2
    dl->AddRectFilled(l2_top, l2_top + trig_sz, ui::u32(ui::rgba(25, 22, 33, 160)), 6.0f);
    dl->AddRectFilled(ImVec2(l2_top.x, l2_top.y + trig_sz.y * (1.0f - l2_val)),
                      l2_top + trig_sz,
                      ui::u32(l_trig_col), 6.0f);
    dl->AddRect(l2_top, l2_top + trig_sz, ui::u32(palette.border), 6.0f, 0, 1.0f);
    ImVec2 l2_ts = ImGui::CalcTextSize("L2");
    dl->AddText(ImVec2(l2_top.x + (trig_sz.x - l2_ts.x) * 0.5f, l2_top.y + (trig_sz.y - l2_ts.y) * 0.5f), ui::u32(palette.text), "L2");

    // R2
    dl->AddRectFilled(r2_top, r2_top + trig_sz_r, ui::u32(ui::rgba(25, 22, 33, 160)), 6.0f);
    dl->AddRectFilled(ImVec2(r2_top.x, r2_top.y + trig_sz_r.y * (1.0f - r2_val)),
                      r2_top + trig_sz_r,
                      ui::u32(r_trig_col), 6.0f);
    dl->AddRect(r2_top, r2_top + trig_sz_r, ui::u32(palette.border), 6.0f, 0, 1.0f);
    ImVec2 r2_ts = ImGui::CalcTextSize("R2");
    dl->AddText(ImVec2(r2_top.x + (trig_sz_r.x - r2_ts.x) * 0.5f, r2_top.y + (trig_sz_r.y - r2_ts.y) * 0.5f), ui::u32(palette.text), "R2");

    // Click & Drag triggers
    if (!app.is_layout_edit_mode) {
        ImGui::SetCursorScreenPos(l2_top);
        ImGui::InvisibleButton("##L2", trig_sz);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            float val = 1.0f - (ImGui::GetMousePos().y - l2_top.y) / trig_sz.y;
            state.trigger_l2 = static_cast<uint8_t>(std::clamp(val * 255.0f, 0.0f, 255.0f));
        } else if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            state.trigger_l2 = 255;
        } else if (!ImGui::IsItemActive()) {
            state.trigger_l2 = 0;
        }

        ImGui::SetCursorScreenPos(r2_top);
        ImGui::InvisibleButton("##R2", trig_sz_r);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            float val = 1.0f - (ImGui::GetMousePos().y - r2_top.y) / trig_sz_r.y;
            state.trigger_r2 = static_cast<uint8_t>(std::clamp(val * 255.0f, 0.0f, 255.0f));
        } else if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            state.trigger_r2 = 255;
        } else if (!ImGui::IsItemActive()) {
            state.trigger_r2 = 0;
        }
    }

    // ── Sticks L3/R3 ────────────────────────────────────────

    auto drawInteractiveStick = [&](ImVec2 sc, const ComponentLayout& comp, int idx_x, int idx_y, const char* label) {
        float r = l.stick_radius * comp.scale;
        ImVec4 custom_col(comp.color[0], comp.color[1], comp.color[2], comp.color[3]);
        // Subtle guide ring
        dl->AddCircle(sc, r + 2, ui::u32(ui::withAlpha(custom_col, 0.25f)), 32, 1.0f);

        // Stick position indicator
        uint8_t xv = state.stick_states[idx_x];
        uint8_t yv = state.stick_states[idx_y];
        float dx = (xv - 128) / 128.0f * r * 0.7f;
        float dy = (yv - 128) / 128.0f * r * 0.7f;
        ImVec2 dot_pos(sc.x + dx, sc.y + dy);
        dl->AddCircleFilled(dot_pos, r * 0.4f, ui::u32(palette.success), 16);
        dl->AddCircle(dot_pos, r * 0.4f + 1, ui::u32(palette.text), 16, 1.0f);

        dl->AddText(ImVec2(sc.x - 8, sc.y + r + 4), ui::u32(palette.muted), label);

        if (!app.is_layout_edit_mode) {
            // Stick dragging
            ImGui::SetCursorScreenPos(ImVec2(sc.x - r, sc.y - r));
            ImGui::InvisibleButton((std::string("stick_") + label).c_str(), ImVec2(r * 2, r * 2));

            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                ImVec2 mouse = ImGui::GetMousePos();
                float rx = std::clamp((mouse.x - sc.x) / (r * 0.7f) * 128.0f + 128.0f, 0.0f, 255.0f);
                float ry = std::clamp((mouse.y - sc.y) / (r * 0.7f) * 128.0f + 128.0f, 0.0f, 255.0f);
                state.stick_states[idx_x] = static_cast<uint8_t>(rx);
                state.stick_states[idx_y] = static_cast<uint8_t>(ry);
            }

            // Click L3/R3
            if (ImGui::IsItemClicked()) {
                int btn_id = (label[0] == 'L') ? 10 : 11;
                state.button_states[btn_id] = !state.button_states[btn_id];
            }
        }
    };

    drawInteractiveStick(l.l_stick_c, layout.l_stick, 0, 1, "L");
    drawInteractiveStick(l.r_stick_c, layout.r_stick, 2, 3, "R");

    // ── D-pad ───────────────────────────────────────────────
    
    float arrow_dist = size * 0.065f * layout.dpad.scale;
    float arrow_w = size * 0.038f * layout.dpad.scale;
    float arrow_h = size * 0.026f * layout.dpad.scale;

    ImVec4 dpad_col(layout.dpad.color[0], layout.dpad.color[1], layout.dpad.color[2], layout.dpad.color[3]);
    auto drawDpadArrow = [&](ImVec2 a, ImVec2 b, ImVec2 c, int btn_id) {
        bool pressed = state.button_states[btn_id];
        if (pressed) {
            dl->AddTriangleFilled(a, b, c, ui::u32(dpad_col));
        } else {
            dl->AddTriangle(a, b, c, ui::u32(ui::rgba(255, 255, 255, 30)), 1.0f);
        }
    };

    // Up arrow
    ImVec2 up_c(l.dpad_center.x, l.dpad_center.y - arrow_dist);
    drawDpadArrow(
        ImVec2(up_c.x, up_c.y - arrow_h),
        ImVec2(up_c.x - arrow_w, up_c.y + arrow_h * 0.5f),
        ImVec2(up_c.x + arrow_w, up_c.y + arrow_h * 0.5f),
        12
    );

    // Down arrow
    ImVec2 dn_c(l.dpad_center.x, l.dpad_center.y + arrow_dist);
    drawDpadArrow(
        ImVec2(dn_c.x, dn_c.y + arrow_h),
        ImVec2(dn_c.x - arrow_w, dn_c.y - arrow_h * 0.5f),
        ImVec2(dn_c.x + arrow_w, dn_c.y - arrow_h * 0.5f),
        13
    );

    // Left arrow
    ImVec2 lf_c(l.dpad_center.x - arrow_dist, l.dpad_center.y);
    drawDpadArrow(
        ImVec2(lf_c.x - arrow_h, lf_c.y),
        ImVec2(lf_c.x + arrow_h * 0.5f, lf_c.y - arrow_w),
        ImVec2(lf_c.x + arrow_h * 0.5f, lf_c.y + arrow_w),
        14
    );

    // Right arrow
    ImVec2 rt_c(l.dpad_center.x + arrow_dist, l.dpad_center.y);
    drawDpadArrow(
        ImVec2(rt_c.x + arrow_h, rt_c.y),
        ImVec2(rt_c.x - arrow_h * 0.5f, rt_c.y - arrow_w),
        ImVec2(rt_c.x - arrow_h * 0.5f, rt_c.y + arrow_w),
        15
    );

    // D-pad interactive overlays
    float btn_size = size * 0.10f * layout.dpad.scale;
    const struct { const char* id; ImVec2 pos; int btn; } dpad_btns[] = {
        {"##dpad_up",    {l.dpad_center.x - btn_size * 0.5f, l.dpad_center.y - arrow_dist - btn_size * 0.5f}, 12},
        {"##dpad_down",  {l.dpad_center.x - btn_size * 0.5f, l.dpad_center.y + arrow_dist - btn_size * 0.5f}, 13},
        {"##dpad_left",  {l.dpad_center.x - arrow_dist - btn_size * 0.5f, l.dpad_center.y - btn_size * 0.5f}, 14},
        {"##dpad_right", {l.dpad_center.x + arrow_dist - btn_size * 0.5f, l.dpad_center.y - btn_size * 0.5f}, 15},
    };

    if (!app.is_layout_edit_mode) {
        for (auto& db : dpad_btns) {
            ImGui::SetCursorScreenPos(db.pos);
            ImGui::InvisibleButton(db.id, ImVec2(btn_size, btn_size));
            if (ImGui::IsItemActive()) state.button_states[db.btn] = true;
            else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !ImGui::IsItemHovered()) state.button_states[db.btn] = false;
        }
    }

    // ── Face Buttons ────────────────────────────────────────
    
    float fb_dist_val = l.fb_dist * layout.face_buttons.scale;
    float fb_radius_val = l.fb_radius * layout.face_buttons.scale;

    const struct { ImVec2 off; int id; ImU32 col; const char* txt; } face_btns[] = {
        {{0, -fb_dist_val}, 3, IM_COL32(60, 180, 120, 220), "T"},
        {{fb_dist_val, 0},  1, IM_COL32(200, 60, 60, 220),  "O"},
        {{0, fb_dist_val},  0, IM_COL32(60, 120, 200, 220), "X"},
        {{-fb_dist_val, 0}, 2, IM_COL32(200, 120, 180, 220),"S"},
    };

    ImVec4 fb_custom_col(layout.face_buttons.color[0], layout.face_buttons.color[1], layout.face_buttons.color[2], layout.face_buttons.color[3]);
    bool fb_is_custom = (layout.face_buttons.color != std::array<float, 4>{0.725f, 0.549f, 1.0f, 1.0f});

    for (auto& fb : face_btns) {
        ImVec2 fc(l.fb_center.x + fb.off.x, l.fb_center.y + fb.off.y);
        
        float r2 = fb_radius_val + 4;
        
        bool pressed = state.button_states[fb.id];
        bool hovered = false;
        
        if (!app.is_layout_edit_mode) {
            ImGui::SetCursorScreenPos(ImVec2(fc.x - r2, fc.y - r2));
            ImGui::InvisibleButton((std::string("##face") + fb.txt).c_str(), ImVec2(r2 * 2, r2 * 2));
            if (ImGui::IsItemActive()) state.button_states[fb.id] = true;
            else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !ImGui::IsItemHovered()) state.button_states[fb.id] = false;
            hovered = ImGui::IsItemHovered();
        }
        
        if (pressed) {
            ImU32 press_col = fb_is_custom ? ui::u32(fb_custom_col) : fb.col;
            dl->AddCircleFilled(fc, fb_radius_val, press_col, 16);
            dl->AddCircle(fc, fb_radius_val + 1, IM_COL32(255, 255, 255, 255), 16, 2.0f);
            auto ts = ImGui::CalcTextSize(fb.txt);
            dl->AddText(ImVec2(fc.x - ts.x * 0.5f, fc.y - ts.y * 0.5f), IM_COL32(255, 255, 255, 255), fb.txt);
        } else if (hovered) {
            ImU32 hover_col = ui::u32(ui::withAlpha(ui::rgba(255, 255, 255), 0.25f));
            dl->AddCircleFilled(fc, fb_radius_val, hover_col, 16);
            dl->AddCircle(fc, fb_radius_val + 1, ui::u32(fb_custom_col), 16, 1.0f);
        } else {
            dl->AddCircle(fc, fb_radius_val, ui::u32(ui::rgba(255, 255, 255, 30)), 16, 1.0f);
        }
    }

    // ── Create, PS, Options ─────────────────────────────────

    ImVec4 cb_custom_col(layout.center_buttons.color[0], layout.center_buttons.color[1], layout.center_buttons.color[2], layout.center_buttons.color[3]);
    auto drawCenterInteractive = [&](float bx, float by, int id, const char* lbl) {
        bool pressed = state.button_states[id];
        ImU32 col = pressed ? ui::u32(cb_custom_col) : ui::u32(ui::rgba(255, 255, 255, 20));
        float button_r = size * 0.02f * layout.center_buttons.scale;
        dl->AddCircleFilled(ImVec2(bx, by), button_r, col, 8);
        dl->AddCircle(ImVec2(bx, by), button_r + 1, ui::u32(palette.border), 8, 1.0f);

        if (!app.is_layout_edit_mode) {
            float click_size = size * 0.04f * layout.center_buttons.scale;
            ImGui::SetCursorScreenPos(ImVec2(bx - click_size, by - click_size));
            ImGui::InvisibleButton((std::string("##ctr") + lbl).c_str(), ImVec2(click_size * 2, click_size * 2));
            if (ImGui::IsItemClicked()) state.button_states[id] = !state.button_states[id];
        }
    };

    // Draw buttons
    float create_x = l.cx - size * 0.365f + layout.center_buttons.x_offset * size;
    float create_y = l.cy - size * 0.432f + layout.center_buttons.y_offset * size;
    float options_x = l.cx + size * 0.365f + layout.center_buttons.x_offset * size;
    float options_y = l.cy - size * 0.432f + layout.center_buttons.y_offset * size;
    float ps_x = l.cx + layout.center_buttons.x_offset * size;
    float ps_y = l.cy + size * 0.035f + layout.center_buttons.y_offset * size;

    drawCenterInteractive(create_x, create_y, 8, "Create");
    drawCenterInteractive(ps_x, ps_y, 16, "PS");
    drawCenterInteractive(options_x, options_y, 9, "Options");

    // Draw text labels with custom positioning to prevent touchpad overlap
    float cb_offset_x = size * 0.03f * layout.center_buttons.scale;
    float cb_offset_y = size * 0.03f * layout.center_buttons.scale;

    ImVec2 ts_c = ImGui::CalcTextSize("Create");
    dl->AddText(ImVec2(create_x - ts_c.x - cb_offset_x, create_y - ts_c.y * 0.5f), ui::u32(palette.muted), "Create");

    ImVec2 ts_o = ImGui::CalcTextSize("Options");
    dl->AddText(ImVec2(options_x + cb_offset_x, options_y - ts_o.y * 0.5f), ui::u32(palette.muted), "Options");

    ImVec2 ts_p = ImGui::CalcTextSize("PS");
    dl->AddText(ImVec2(ps_x - ts_p.x * 0.5f, ps_y + cb_offset_y), ui::u32(palette.muted), "PS");

    // ── Touchpad ────────────────────────────────────────────

    float tp_cx = l.cx + layout.touchpad.x_offset * size;
    float tp_cy = l.cy - size * 0.3725f + layout.touchpad.y_offset * size;
    float tp_w = size * 0.720f * layout.touchpad.scale;
    float tp_h = size * 0.341f * layout.touchpad.scale;
    ImVec2 tp_min(tp_cx - tp_w * 0.5f, tp_cy - tp_h * 0.5f);
    ImVec2 tp_max(tp_cx + tp_w * 0.5f, tp_cy + tp_h * 0.5f);

    ImVec4 tp_custom_col(layout.touchpad.color[0], layout.touchpad.color[1], layout.touchpad.color[2], layout.touchpad.color[3]);
    ImU32 touch_col = state.button_states[17] ? ui::u32(ui::withAlpha(tp_custom_col, 0.4f)) : ui::u32(ui::rgba(25, 22, 33, 120));
    dl->AddRectFilled(tp_min, tp_max, touch_col, 8.0f);
    dl->AddRect(tp_min, tp_max, ui::u32(palette.border), 8.0f, 0, 1.0f);
    ImVec2 tp_ts = ImGui::CalcTextSize("Touchpad");
    dl->AddText(ImVec2(tp_cx - tp_ts.x * 0.5f, tp_cy - tp_ts.y * 0.5f), ui::u32(palette.text), "Touchpad");

    if (!app.is_layout_edit_mode) {
        ImGui::SetCursorScreenPos(tp_min);
        ImGui::InvisibleButton("##touchpad", tp_max - tp_min);
        if (ImGui::IsItemClicked()) state.button_states[17] = !state.button_states[17];
    }

    // ── L3/R3 Indicators ────────────────────────────────────

    if (state.button_states[10]) {
        ImVec4 l_col(layout.l_stick.color[0], layout.l_stick.color[1], layout.l_stick.color[2], layout.l_stick.color[3]);
        dl->AddText(ImVec2(l.l_stick_c.x - 8, l.l_stick_c.y - (l.stick_radius * layout.l_stick.scale) - 16), ui::u32(l_col), "L3");
    }
    if (state.button_states[11]) {
        ImVec4 r_col(layout.r_stick.color[0], layout.r_stick.color[1], layout.r_stick.color[2], layout.r_stick.color[3]);
        dl->AddText(ImVec2(l.r_stick_c.x - 8, l.r_stick_c.y - (l.stick_radius * layout.r_stick.scale) - 16), ui::u32(r_col), "R3");
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //                         EDIT MODE OVERLAYS & CONTROLS
    // ─────────────────────────────────────────────────────────────────────────────
    if (app.is_layout_edit_mode) {
        // Draw hover highlights and selected borders
        int hovered = getHoveredComponent(ImGui::GetMousePos(), l, layout, size);
        
        if (hovered != LC_None && hovered != app.selected_layout_component) {
            drawComponentBounds(dl, hovered, l, layout, size, ui::u32(ui::rgba(255, 165, 0, 160)), 1.0f);
        }
        if (app.selected_layout_component != LC_None) {
            drawComponentBounds(dl, app.selected_layout_component, l, layout, size, ui::u32(palette.success), 3.0f);
        }

        // Invisible button spanning the entire visualizer area to handle click and drag adjustments
        ImVec2 overlay_min = base;
        ImVec2 overlay_max(base.x + l.w, base.y + l.h);
        
        ImGui::SetCursorScreenPos(overlay_min);
        ImGui::InvisibleButton("##layout_edit_overlay", overlay_max - overlay_min);
        
        ImVec2 mouse_pos = ImGui::GetMousePos();
        if (ImGui::IsItemClicked()) {
            app.selected_layout_component = getHoveredComponent(mouse_pos, l, layout, size);
        }
        
        if (app.selected_layout_component != LC_None && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            float dx = drag_delta.x / size;
            float dy = drag_delta.y / size;
            
            auto& temp = app.temp_layout;
            switch (app.selected_layout_component) {
                case LC_LeftStick:     temp.l_stick.x_offset += dx; temp.l_stick.y_offset += dy; break;
                case LC_RightStick:    temp.r_stick.x_offset += dx; temp.r_stick.y_offset += dy; break;
                case LC_Dpad:          temp.dpad.x_offset += dx; temp.dpad.y_offset += dy; break;
                case LC_FaceButtons:   temp.face_buttons.x_offset += dx; temp.face_buttons.y_offset += dy; break;
                case LC_ShouldersL:    temp.shoulders_l.x_offset += dx; temp.shoulders_l.y_offset += dy; break;
                case LC_ShouldersR:    temp.shoulders_r.x_offset += dx; temp.shoulders_r.y_offset += dy; break;
                case LC_Touchpad:      temp.touchpad.x_offset += dx; temp.touchpad.y_offset += dy; break;
                case LC_CenterButtons: temp.center_buttons.x_offset += dx; temp.center_buttons.y_offset += dy; break;
            }
        }
    }

    ImGui::Dummy(ImVec2(l.w, l.h + 20));
}

// Display-only version
void renderPadVisualizer(App& app, const PadStateInput& state, float size) {
    PadStateInput copy = state;
    renderInteractivePadVisualizer(app, copy, size);
}

} // namespace ghostpad

