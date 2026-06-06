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

namespace ghostpad {

/*
 *  TEXTURE LOADER AND CONTEXT
 */
#include "dualsense_solid_black_png.h"

extern ImTextureID createControllerTexture(const unsigned char* pixels, int width, int height);

static ImTextureID g_controller_texture = 0;
static int g_tex_w = 0;
static int g_tex_h = 0;

static void loadControllerTexture() {
    if (g_controller_texture != 0) return;

    int width, height, channels;
    unsigned char* data = stbi_load_from_memory(dualsense_solid_black_png, dualsense_solid_black_png_size, &width, &height, &channels, 4);
    if (!data) {
        fprintf(stderr, "[Ghostpad] Failed to load controller texture from embedded memory: %s\n", 
                stbi_failure_reason());
        return;
    }

    g_tex_w = width;
    g_tex_h = height;

    // Convert solid black background to transparency channel using color luminance
    for (int i = 0; i < width * height; ++i) {
        unsigned char r = data[i * 4 + 0];
        unsigned char g = data[i * 4 + 1];
        unsigned char b = data[i * 4 + 2];
        unsigned char max_val = (std::max)({r, g, b});
        data[i * 4 + 3] = max_val;
    }

    g_controller_texture = createControllerTexture(data, width, height);
    
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
    LC_Create,
    LC_Options,
    LC_PS,
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

    // D-pad base center (before individual offsets)
    ImVec2 dpad_base = ImVec2(l.cx - size * 0.576f, l.cy - size * 0.237f);
    float dpad_dist_val = size * 0.065f;
    
    ImVec2 up_c = dpad_base + ImVec2(0.0f, -dpad_dist_val * layout.dpad_up.scale) + ImVec2(layout.dpad_up.x_offset * size, layout.dpad_up.y_offset * size);
    ImVec2 dn_c = dpad_base + ImVec2(0.0f, dpad_dist_val * layout.dpad_down.scale) + ImVec2(layout.dpad_down.x_offset * size, layout.dpad_down.y_offset * size);
    ImVec2 lf_c = dpad_base + ImVec2(-dpad_dist_val * layout.dpad_left.scale, 0.0f) + ImVec2(layout.dpad_left.x_offset * size, layout.dpad_left.y_offset * size);
    ImVec2 rt_c = dpad_base + ImVec2(dpad_dist_val * layout.dpad_right.scale, 0.0f) + ImVec2(layout.dpad_right.x_offset * size, layout.dpad_right.y_offset * size);
    
    l.dpad_center = ImVec2((up_c.x + dn_c.x + lf_c.x + rt_c.x) * 0.25f,
                           (up_c.y + dn_c.y + lf_c.y + rt_c.y) * 0.25f);
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

    // Create button hover
    float create_r = size * 0.04f * layout.create_btn.scale;
    float create_x = l.cx - size * 0.365f + layout.create_btn.x_offset * size;
    float create_y = l.cy - size * 0.432f + layout.create_btn.y_offset * size;
    if (dist(mouse_pos, ImVec2(create_x, create_y)) < create_r) {
        return LC_Create;
    }

    // Options button hover
    float options_r = size * 0.04f * layout.options_btn.scale;
    float options_x = l.cx + size * 0.365f + layout.options_btn.x_offset * size;
    float options_y = l.cy - size * 0.432f + layout.options_btn.y_offset * size;
    if (dist(mouse_pos, ImVec2(options_x, options_y)) < options_r) {
        return LC_Options;
    }

    // PS button hover
    float ps_r = size * 0.04f * layout.ps_btn.scale;
    float ps_x = l.cx + layout.ps_btn.x_offset * size;
    float ps_y = l.cy + size * 0.035f + layout.ps_btn.y_offset * size;
    if (dist(mouse_pos, ImVec2(ps_x, ps_y)) < ps_r) {
        return LC_PS;
    }

    // 2. Sticks
    float l_stick_r = l.stick_radius * layout.l_stick.scale;
    float r_stick_r = l.stick_radius * layout.r_stick.scale;
    if (dist(mouse_pos, l.l_stick_c) < l_stick_r) return LC_LeftStick;
    if (dist(mouse_pos, l.r_stick_c) < r_stick_r) return LC_RightStick;

    // 3. D-pad
    float avg_dpad_scale = (layout.dpad_up.scale + layout.dpad_down.scale + layout.dpad_left.scale + layout.dpad_right.scale) * 0.25f;
    float dpad_r = l.dpad_size * 2.0f * avg_dpad_scale;
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
        case LC_Dpad: {
            float avg_dpad_scale = (layout.dpad_up.scale + layout.dpad_down.scale + layout.dpad_left.scale + layout.dpad_right.scale) * 0.25f;
            dl->AddCircle(l.dpad_center, l.dpad_size * 2.0f * avg_dpad_scale + 6, color, 32, thickness);
            break;
        }
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
        case LC_Create: {
            float r = size * 0.04f * layout.create_btn.scale;
            float cx = l.cx - size * 0.365f + layout.create_btn.x_offset * size;
            float cy = l.cy - size * 0.432f + layout.create_btn.y_offset * size;
            dl->AddCircle(ImVec2(cx, cy), r + 4, color, 16, thickness);
            break;
        }
        case LC_Options: {
            float r = size * 0.04f * layout.options_btn.scale;
            float cx = l.cx + size * 0.365f + layout.options_btn.x_offset * size;
            float cy = l.cy - size * 0.432f + layout.options_btn.y_offset * size;
            dl->AddCircle(ImVec2(cx, cy), r + 4, color, 16, thickness);
            break;
        }
        case LC_PS: {
            float r = size * 0.04f * layout.ps_btn.scale;
            float cx = l.cx + layout.ps_btn.x_offset * size;
            float cy = l.cy + size * 0.035f + layout.ps_btn.y_offset * size;
            dl->AddCircle(ImVec2(cx, cy), r + 4, color, 16, thickness);
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
        
        dl->AddImage(g_controller_texture, p_min, p_max, ImVec2(0,0), ImVec2(1,1), ui::u32(ui::rgba(255, 255, 255, 255)));
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

    auto drawShoulderInteractive = [&](ImVec2 sp, ImVec2 sz, int id, const char* txt, const ComponentLayout& comp, int comp_id) {
        ImVec4 custom_col(comp.color[0], comp.color[1], comp.color[2], comp.color[3]);
        bool pressed = state.button_states[id];
        if (app.is_layout_edit_mode && app.selected_layout_component == comp_id) {
            pressed = true;
        }

        ImU32 col = pressed ? ui::u32(custom_col) : ui::u32(ui::withAlpha(custom_col, custom_col.w * 0.15f));
        dl->AddRectFilled(sp, ImVec2(sp.x + sz.x, sp.y + sz.y), col, 4.0f);
        dl->AddRect(sp, ImVec2(sp.x + sz.x, sp.y + sz.y), ui::u32(ui::withAlpha(custom_col, custom_col.w * 0.40f)), 4.0f, 0, 1.0f);
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
    drawShoulderInteractive(l1_top, sh_sz, 4, "L1", layout.shoulders_l, LC_ShouldersL);
    drawShoulderInteractive(r1_top, sh_sz_r, 5, "R1", layout.shoulders_r, LC_ShouldersR);

    // Triggers L2/R2
    float l2_val = state.trigger_l2 / 255.0f;
    float r2_val = state.trigger_r2 / 255.0f;
    if (app.is_layout_edit_mode && app.selected_layout_component == LC_ShouldersL) {
        l2_val = 1.0f;
    }
    if (app.is_layout_edit_mode && app.selected_layout_component == LC_ShouldersR) {
        r2_val = 1.0f;
    }

    ImVec4 l_trig_col(layout.shoulders_l.color[0], layout.shoulders_l.color[1], layout.shoulders_l.color[2], layout.shoulders_l.color[3]);
    ImVec4 r_trig_col(layout.shoulders_r.color[0], layout.shoulders_r.color[1], layout.shoulders_r.color[2], layout.shoulders_r.color[3]);

    // L2
    dl->AddRectFilled(l2_top, l2_top + trig_sz, ui::u32(ui::withAlpha(l_trig_col, l_trig_col.w * 0.15f)), 6.0f);
    if (l2_val > 0.0f) {
        dl->AddRectFilled(ImVec2(l2_top.x, l2_top.y + trig_sz.y * (1.0f - l2_val)),
                          l2_top + trig_sz,
                          ui::u32(l_trig_col), 6.0f);
    }
    dl->AddRect(l2_top, l2_top + trig_sz, ui::u32(ui::withAlpha(l_trig_col, l_trig_col.w * 0.40f)), 6.0f, 0, 1.0f);
    ImVec2 l2_ts = ImGui::CalcTextSize("L2");
    dl->AddText(ImVec2(l2_top.x + (trig_sz.x - l2_ts.x) * 0.5f, l2_top.y + (trig_sz.y - l2_ts.y) * 0.5f), ui::u32(palette.text), "L2");

    // R2
    dl->AddRectFilled(r2_top, r2_top + trig_sz_r, ui::u32(ui::withAlpha(r_trig_col, r_trig_col.w * 0.15f)), 6.0f);
    if (r2_val > 0.0f) {
        dl->AddRectFilled(ImVec2(r2_top.x, r2_top.y + trig_sz_r.y * (1.0f - r2_val)),
                          r2_top + trig_sz_r,
                          ui::u32(r_trig_col), 6.0f);
    }
    dl->AddRect(r2_top, r2_top + trig_sz_r, ui::u32(ui::withAlpha(r_trig_col, r_trig_col.w * 0.40f)), 6.0f, 0, 1.0f);
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

    auto drawInteractiveStick = [&](ImVec2 sc, const ComponentLayout& comp, int idx_x, int idx_y, const char* label, int comp_id) {
        float r = l.stick_radius * comp.scale;
        ImVec4 custom_col(comp.color[0], comp.color[1], comp.color[2], comp.color[3]);
        
        bool is_selected = app.is_layout_edit_mode && app.selected_layout_component == comp_id;
        
        // Guide ring using custom color and opacity
        float ring_alpha = is_selected ? custom_col.w : (custom_col.w * 0.35f);
        dl->AddCircle(sc, r + 2, ui::u32(ui::withAlpha(custom_col, ring_alpha)), 32, is_selected ? 2.0f : 1.0f);

        // Stick position indicator
        uint8_t xv = state.stick_states[idx_x];
        uint8_t yv = state.stick_states[idx_y];
        float dx = (xv - 128) / 128.0f * r * 0.7f;
        float dy = (yv - 128) / 128.0f * r * 0.7f;
        ImVec2 dot_pos(sc.x + dx, sc.y + dy);
        dl->AddCircleFilled(dot_pos, r * 0.4f, ui::u32(custom_col), 16);
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

    drawInteractiveStick(l.l_stick_c, layout.l_stick, 0, 1, "L", LC_LeftStick);
    drawInteractiveStick(l.r_stick_c, layout.r_stick, 2, 3, "R", LC_RightStick);

    // ── D-pad ───────────────────────────────────────────────
    
    float avg_scale = (layout.dpad_up.scale + layout.dpad_down.scale + layout.dpad_left.scale + layout.dpad_right.scale) * 0.25f;
    float arrow_dist = size * 0.065f * avg_scale;
    float arrow_w = size * 0.038f * avg_scale;
    float arrow_h = size * 0.026f * avg_scale;

    bool dpad_selected = app.is_layout_edit_mode && app.selected_layout_component == LC_Dpad;
    ImVec2 dpad_base = ImVec2(l.cx - size * 0.576f, l.cy - size * 0.237f);

    // D-pad circular recessed housing well
    float housing_r = arrow_dist * 1.82f;
    ImVec4 dpad_col_up(layout.dpad_up.color[0], layout.dpad_up.color[1], layout.dpad_up.color[2], layout.dpad_up.color[3]);
    float housing_bg_alpha = 240.0f / 255.0f * dpad_col_up.w;
    dl->AddCircleFilled(l.dpad_center, housing_r, ui::u32(ui::rgba(14, 13, 17, (int)(housing_bg_alpha * 255))), 32);
    // Draw recessed well shadow bevel
    dl->AddCircle(l.dpad_center, housing_r - 1.0f, ui::u32(ui::rgba(5, 5, 7, 255)), 32, 2.0f);
    dl->AddCircle(l.dpad_center, housing_r, ui::u32(ui::withAlpha(dpad_col_up, dpad_col_up.w * 0.18f)), 32, 1.5f);

    auto drawWedgeButton = [&](int dir, const ComponentLayout& comp, int btn_id) {
        bool actually_pressed = state.button_states[btn_id];
        bool pressed = actually_pressed || dpad_selected;
        
        float s = comp.scale;
        
        // 18-vertex high-fidelity shield shape coordinates normalized by arrow_dist
        const ImVec2 shield_template[] = {
            ImVec2( 0.00f, -0.30f), // Rounded inner tip
            ImVec2( 0.08f, -0.38f),
            ImVec2( 0.46f, -0.72f), // Bottom-right diagonal edge
            ImVec2( 0.58f, -0.78f), // Bottom-right corner rounding 1
            ImVec2( 0.62f, -0.86f), // Bottom-right corner rounding 2
            ImVec2( 0.62f, -1.34f), // Right vertical side
            ImVec2( 0.58f, -1.42f), // Top-right corner rounding 1
            ImVec2( 0.48f, -1.48f), // Top-right corner rounding 2
            ImVec2( 0.26f, -1.56f), // Dome curve right
            ImVec2( 0.00f, -1.60f), // Outer dome peak
            ImVec2(-0.26f, -1.56f), // Dome curve left
            ImVec2(-0.48f, -1.48f), // Top-left corner rounding 2
            ImVec2(-0.58f, -1.42f), // Top-left corner rounding 1
            ImVec2(-0.62f, -1.34f), // Left vertical side
            ImVec2(-0.62f, -0.86f), // Bottom-left corner rounding 2
            ImVec2(-0.58f, -0.78f), // Bottom-left corner rounding 1
            ImVec2(-0.46f, -0.72f), // Bottom-left diagonal edge
            ImVec2(-0.08f, -0.38f)
        };
        
        // Transform coordinates with rotation & translation
        ImVec2 offset(comp.x_offset * size, comp.y_offset * size);
        std::vector<ImVec2> trans_verts;
        trans_verts.reserve(18);
        
        for (const auto& v : shield_template) {
            float vx = v.x * arrow_dist * s / avg_scale;
            float vy = v.y * arrow_dist * s / avg_scale;
            
            float rx = vx;
            float ry = vy;
            if (dir == 3) { // Right (90 deg CW)
                rx = -vy;
                ry = vx;
            } else if (dir == 1) { // Down (180 deg)
                rx = -vx;
                ry = -vy;
            } else if (dir == 2) { // Left (270 deg CW)
                rx = vy;
                ry = -vx;
            }
            trans_verts.push_back(ImVec2(dpad_base.x + rx + offset.x, dpad_base.y + ry + offset.y));
        }
        
        ImVec4 body_col(comp.color[0], comp.color[1], comp.color[2], comp.color[3]);
        ImVec4 arrow_col(comp.secondary_color[0], comp.secondary_color[1], comp.secondary_color[2], comp.secondary_color[3]);
        
        // Shadow/glow background
        if (pressed) {
            dl->AddConvexPolyFilled(trans_verts.data(), (int)trans_verts.size(), ui::u32(ui::withAlpha(body_col, body_col.w * 0.25f)));
        } else {
            std::vector<ImVec2> shadow_verts = trans_verts;
            for (auto& sv : shadow_verts) {
                sv.x += 1.5f;
                sv.y += 1.5f;
            }
            dl->AddConvexPolyFilled(shadow_verts.data(), (int)shadow_verts.size(), ui::u32(ui::rgba(5, 5, 7, 100)));
        }
        
        /*
         *  ┌──────────────────────────────────────────────────────────┐
         *  │                BUTTON CASING RENDERING                   │
         *  └──────────────────────────────────────────────────────────┘
         */
        float backing_alpha = 220.0f / 255.0f * body_col.w;
        dl->AddConvexPolyFilled(trans_verts.data(), (int)trans_verts.size(), ui::u32(ui::rgba(18, 17, 22, (int)(backing_alpha * 255))));
        
        float overlay_alpha = pressed ? body_col.w : (body_col.w * 0.22f);
        dl->AddConvexPolyFilled(trans_verts.data(), (int)trans_verts.size(), ui::u32(ui::withAlpha(body_col, overlay_alpha)));
        
        // Border outline
        ImU32 border_col;
        float border_thickness = 1.0f;
        if (pressed) {
            border_col = IM_COL32(255, 255, 255, 255);
            border_thickness = 1.5f;
        } else {
            border_col = ui::u32(ui::withAlpha(body_col, body_col.w * 0.35f));
        }
        dl->AddPolyline(trans_verts.data(), (int)trans_verts.size(), border_col, ImDrawFlags_Closed, border_thickness);
        
        // Arrow chevron
        float r_mid = arrow_dist * 0.95f * s / avg_scale;
        ImVec2 arrow_center_local(0.0f, -r_mid);
        float ax = arrow_center_local.x;
        float ay = arrow_center_local.y;
        if (dir == 3) {
            ax = -arrow_center_local.y;
            ay = arrow_center_local.x;
        } else if (dir == 1) {
            ax = -arrow_center_local.x;
            ay = -arrow_center_local.y;
        } else if (dir == 2) {
            ax = arrow_center_local.y;
            ay = -arrow_center_local.x;
        }
        
        ImVec2 arrow_c(dpad_base.x + ax + offset.x, dpad_base.y + ay + offset.y);
        
        float tr_w = arrow_w * 0.45f * s;
        float tr_h = arrow_h * 0.45f * s;
        ImU32 symbol_col = actually_pressed ? IM_COL32(255, 255, 255, 255) : ui::u32(ui::withAlpha(arrow_col, arrow_col.w * 0.85f));
        
        if (dir == 0) { // Up
            dl->AddTriangleFilled(
                ImVec2(arrow_c.x, arrow_c.y - tr_h),
                ImVec2(arrow_c.x - tr_w, arrow_c.y + tr_h),
                ImVec2(arrow_c.x + tr_w, arrow_c.y + tr_h),
                symbol_col
            );
        } else if (dir == 1) { // Down
            dl->AddTriangleFilled(
                ImVec2(arrow_c.x, arrow_c.y + tr_h),
                ImVec2(arrow_c.x - tr_w, arrow_c.y - tr_h),
                ImVec2(arrow_c.x + tr_w, arrow_c.y - tr_h),
                symbol_col
            );
        } else if (dir == 2) { // Left
            dl->AddTriangleFilled(
                ImVec2(arrow_c.x - tr_h, arrow_c.y),
                ImVec2(arrow_c.x + tr_h, arrow_c.y - tr_w),
                ImVec2(arrow_c.x + tr_h, arrow_c.y + tr_w),
                symbol_col
            );
        } else if (dir == 3) { // Right
            dl->AddTriangleFilled(
                ImVec2(arrow_c.x + tr_h, arrow_c.y),
                ImVec2(arrow_c.x - tr_h, arrow_c.y - tr_w),
                ImVec2(arrow_c.x - tr_h, arrow_c.y + tr_w),
                symbol_col
            );
        }
        
        return arrow_c;
    };

    ImVec2 arrow_c_up = drawWedgeButton(0, layout.dpad_up, 12);
    ImVec2 arrow_c_dn = drawWedgeButton(1, layout.dpad_down, 13);
    ImVec2 arrow_c_lf = drawWedgeButton(2, layout.dpad_left, 14);
    ImVec2 arrow_c_rt = drawWedgeButton(3, layout.dpad_right, 15);

    // D-pad center pivot cap overlaying the inner intersection gap
    float pivot_r = arrow_dist * 0.32f;
    float pivot_bg_alpha = dpad_col_up.w;
    dl->AddCircleFilled(l.dpad_center, pivot_r, ui::u32(ui::rgba(14, 13, 17, (int)(pivot_bg_alpha * 255))), 16);
    dl->AddCircle(l.dpad_center, pivot_r, ui::u32(ui::withAlpha(dpad_col_up, dpad_col_up.w * 0.22f)), 16, 1.0f);

    // D-pad interactive hitboxes
    float btn_size_up = size * 0.10f * layout.dpad_up.scale;
    float btn_size_dn = size * 0.10f * layout.dpad_down.scale;
    float btn_size_lf = size * 0.10f * layout.dpad_left.scale;
    float btn_size_rt = size * 0.10f * layout.dpad_right.scale;
    
    const struct { const char* id; ImVec2 pos; float btn_sz; int btn; } dpad_btns[] = {
        {"##dpad_up",    {arrow_c_up.x - btn_size_up * 0.5f, arrow_c_up.y - btn_size_up * 0.5f}, btn_size_up, 12},
        {"##dpad_down",  {arrow_c_dn.x - btn_size_dn * 0.5f, arrow_c_dn.y - btn_size_dn * 0.5f}, btn_size_dn, 13},
        {"##dpad_left",  {arrow_c_lf.x - btn_size_lf * 0.5f, arrow_c_lf.y - btn_size_lf * 0.5f}, btn_size_lf, 14},
        {"##dpad_right", {arrow_c_rt.x - btn_size_rt * 0.5f, arrow_c_rt.y - btn_size_rt * 0.5f}, btn_size_rt, 15},
    };

    if (!app.is_layout_edit_mode) {
        for (auto& db : dpad_btns) {
            ImGui::SetCursorScreenPos(db.pos);
            ImGui::InvisibleButton(db.id, ImVec2(db.btn_sz, db.btn_sz));
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
    ImVec4 fb_custom_sec_col(layout.face_buttons.secondary_color[0], layout.face_buttons.secondary_color[1], layout.face_buttons.secondary_color[2], layout.face_buttons.secondary_color[3]);
    bool fb_is_custom = (layout.face_buttons.color != std::array<float, 4>{0.725f, 0.549f, 1.0f, 1.0f});
    bool fb_selected = app.is_layout_edit_mode && app.selected_layout_component == LC_FaceButtons;

    for (auto& fb : face_btns) {
        ImVec2 fc(l.fb_center.x + fb.off.x, l.fb_center.y + fb.off.y);
        
        float r2 = fb_radius_val + 4;
        
        bool pressed = state.button_states[fb.id] || fb_selected;
        bool hovered = false;
        
        if (!app.is_layout_edit_mode) {
            ImGui::SetCursorScreenPos(ImVec2(fc.x - r2, fc.y - r2));
            ImGui::InvisibleButton((std::string("##face") + fb.txt).c_str(), ImVec2(r2 * 2, r2 * 2));
            if (ImGui::IsItemActive()) state.button_states[fb.id] = true;
            else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !ImGui::IsItemHovered()) state.button_states[fb.id] = false;
            hovered = ImGui::IsItemHovered();
        }
        
        /*
         *  ┌──────────────────────────────────────────────────────────┐
         *  │                FACE BUTTONS RENDERING                    │
         *  └──────────────────────────────────────────────────────────┘
         */
        float backing_alpha = 220.0f / 255.0f * fb_custom_col.w;
        dl->AddCircleFilled(fc, fb_radius_val, ui::u32(ui::rgba(18, 17, 22, (int)(backing_alpha * 255))), 16);

        ImVec4 press_col_vec4 = fb_is_custom ? fb_custom_col : ImGui::ColorConvertU32ToFloat4(fb.col);
        ImU32 press_col = ui::u32(press_col_vec4);

        if (pressed) {
            dl->AddCircleFilled(fc, fb_radius_val + 5.0f, ui::u32(ui::withAlpha(press_col_vec4, press_col_vec4.w * 0.25f)), 16);
            dl->AddCircleFilled(fc, fb_radius_val, press_col, 16);
            dl->AddCircle(fc, fb_radius_val, IM_COL32(255, 255, 255, 255), 16, 1.5f);
        } else if (hovered) {
            dl->AddCircleFilled(fc, fb_radius_val, ui::u32(ui::withAlpha(fb_custom_col, fb_custom_col.w * 0.35f)), 16);
            dl->AddCircle(fc, fb_radius_val, ui::u32(fb_custom_col), 16, 1.0f);
        } else {
            dl->AddCircleFilled(fc, fb_radius_val, ui::u32(ui::withAlpha(fb_custom_col, fb_custom_col.w * 0.12f)), 16);
            dl->AddCircle(fc, fb_radius_val, ui::u32(ui::withAlpha(fb_custom_col, fb_custom_col.w * 0.35f)), 16, 1.0f);
        }

        bool actually_pressed = state.button_states[fb.id];
        ImU32 symbol_col = actually_pressed ? IM_COL32(255, 255, 255, 255) : ui::u32(ui::withAlpha(fb_custom_sec_col, fb_custom_sec_col.w * 0.85f));
        float h = fb_radius_val * 0.40f;
        if (fb.txt[0] == 'T') {
            ImVec2 p1(fc.x, fc.y - h);
            ImVec2 p2(fc.x - h * 1.1f, fc.y + h * 0.7f);
            ImVec2 p3(fc.x + h * 1.1f, fc.y + h * 0.7f);
            dl->AddTriangle(p1, p2, p3, symbol_col, 2.0f);
        } else if (fb.txt[0] == 'O') {
            dl->AddCircle(fc, fb_radius_val * 0.40f, symbol_col, 16, 2.0f);
        } else if (fb.txt[0] == 'X') {
            float sz = fb_radius_val * 0.35f;
            dl->AddLine(ImVec2(fc.x - sz, fc.y - sz), ImVec2(fc.x + sz, fc.y + sz), symbol_col, 2.5f);
            dl->AddLine(ImVec2(fc.x + sz, fc.y - sz), ImVec2(fc.x - sz, fc.y + sz), symbol_col, 2.5f);
        } else if (fb.txt[0] == 'S') {
            float sz = fb_radius_val * 0.35f;
            dl->AddRect(ImVec2(fc.x - sz, fc.y - sz), ImVec2(fc.x + sz, fc.y + sz), symbol_col, 0.0f, 0, 2.0f);
        }
    }

    // ── Create, PS, Options ─────────────────────────────────

    auto drawCenterInteractive = [&](float bx, float by, int id, const char* lbl, const ComponentLayout& comp, int comp_id) {
        bool pressed = state.button_states[id] || (app.is_layout_edit_mode && app.selected_layout_component == comp_id);
        ImVec4 custom_col(comp.color[0], comp.color[1], comp.color[2], comp.color[3]);
        ImU32 col = pressed ? ui::u32(custom_col) : ui::u32(ui::withAlpha(custom_col, custom_col.w * 0.20f));
        float button_r = size * 0.02f * comp.scale;
        
        if (pressed) {
            dl->AddCircleFilled(ImVec2(bx, by), button_r + 3.0f, ui::u32(ui::withAlpha(custom_col, custom_col.w * 0.25f)), 16);
        }
        dl->AddCircleFilled(ImVec2(bx, by), button_r, ui::u32(ui::rgba(18, 17, 22, 220)), 16);
        dl->AddCircleFilled(ImVec2(bx, by), button_r, col, 16);
        dl->AddCircle(ImVec2(bx, by), button_r, ui::u32(ui::withAlpha(custom_col, custom_col.w * 0.50f)), 16, 1.0f);

        if (!app.is_layout_edit_mode) {
            float click_size = size * 0.04f * comp.scale;
            ImGui::SetCursorScreenPos(ImVec2(bx - click_size, by - click_size));
            ImGui::InvisibleButton((std::string("##ctr") + lbl).c_str(), ImVec2(click_size * 2, click_size * 2));
            if (ImGui::IsItemClicked()) state.button_states[id] = !state.button_states[id];
        }
    };

    // Draw buttons
    float create_x = l.cx - size * 0.365f + layout.create_btn.x_offset * size;
    float create_y = l.cy - size * 0.432f + layout.create_btn.y_offset * size;
    float options_x = l.cx + size * 0.365f + layout.options_btn.x_offset * size;
    float options_y = l.cy - size * 0.432f + layout.options_btn.y_offset * size;
    float ps_x = l.cx + layout.ps_btn.x_offset * size;
    float ps_y = l.cy + size * 0.035f + layout.ps_btn.y_offset * size;

    drawCenterInteractive(create_x, create_y, 8, "Create", layout.create_btn, LC_Create);
    drawCenterInteractive(ps_x, ps_y, 16, "PS", layout.ps_btn, LC_PS);
    drawCenterInteractive(options_x, options_y, 9, "Options", layout.options_btn, LC_Options);

    // Draw text labels with custom positioning to prevent touchpad overlap
    float cb_offset_x = size * 0.03f;
    float cb_offset_y = size * 0.03f;

    ImVec2 ts_c = ImGui::CalcTextSize("Create");
    ImVec4 create_sec_col(layout.create_btn.secondary_color[0], layout.create_btn.secondary_color[1], layout.create_btn.secondary_color[2], layout.create_btn.secondary_color[3]);
    dl->AddText(ImVec2(create_x - ts_c.x - cb_offset_x * layout.create_btn.scale, create_y - ts_c.y * 0.5f), ui::u32(create_sec_col), "Create");

    ImVec2 ts_o = ImGui::CalcTextSize("Options");
    ImVec4 options_sec_col(layout.options_btn.secondary_color[0], layout.options_btn.secondary_color[1], layout.options_btn.secondary_color[2], layout.options_btn.secondary_color[3]);
    dl->AddText(ImVec2(options_x + cb_offset_x * layout.options_btn.scale, options_y - ts_o.y * 0.5f), ui::u32(options_sec_col), "Options");

    ImVec2 ts_p = ImGui::CalcTextSize("PS");
    ImVec4 ps_sec_col(layout.ps_btn.secondary_color[0], layout.ps_btn.secondary_color[1], layout.ps_btn.secondary_color[2], layout.ps_btn.secondary_color[3]);
    dl->AddText(ImVec2(ps_x - ts_p.x * 0.5f, ps_y + cb_offset_y * layout.ps_btn.scale), ui::u32(ps_sec_col), "PS");

    // ── Touchpad ────────────────────────────────────────────

    float tp_cx = l.cx + layout.touchpad.x_offset * size;
    float tp_cy = l.cy - size * 0.3725f + layout.touchpad.y_offset * size;
    float tp_w = size * 0.720f * layout.touchpad.scale;
    float tp_h = size * 0.341f * layout.touchpad.scale;
    ImVec2 tp_min(tp_cx - tp_w * 0.5f, tp_cy - tp_h * 0.5f);
    ImVec2 tp_max(tp_cx + tp_w * 0.5f, tp_cy + tp_h * 0.5f);

    bool tp_selected = app.is_layout_edit_mode && app.selected_layout_component == LC_Touchpad;
    bool tp_pressed = state.button_states[17] || tp_selected;
    ImVec4 tp_custom_col(layout.touchpad.color[0], layout.touchpad.color[1], layout.touchpad.color[2], layout.touchpad.color[3]);
    
    /*
     *  ┌──────────────────────────────────────────────────────────┐
     *  │                  TOUCHPAD RENDERING                      │
     *  └──────────────────────────────────────────────────────────┘
     */
    float tp_bg_alpha = 240.0f / 255.0f * tp_custom_col.w;
    ImU32 touch_bg = ui::u32(ui::rgba(18, 17, 22, (int)(tp_bg_alpha * 255)));
    dl->AddRectFilled(tp_min, tp_max, touch_bg, 8.0f);
    
    ImU32 touch_tint = tp_pressed ? ui::u32(ui::withAlpha(tp_custom_col, tp_custom_col.w * 0.40f)) : ui::u32(ui::withAlpha(tp_custom_col, tp_custom_col.w * 0.08f));
    dl->AddRectFilled(tp_min, tp_max, touch_tint, 8.0f);
    
    if (tp_pressed) {
        dl->AddRect(tp_min - ImVec2(2, 2), tp_max + ImVec2(2, 2), ui::u32(ui::withAlpha(tp_custom_col, tp_custom_col.w * 0.25f)), 8.0f, 0, 2.0f);
        dl->AddRect(tp_min, tp_max, ui::u32(tp_custom_col), 8.0f, 0, 1.5f);
    } else {
        dl->AddRect(tp_min, tp_max, ui::u32(ui::withAlpha(tp_custom_col, tp_custom_col.w * 0.35f)), 8.0f, 0, 1.0f);
    }

    ImVec2 tp_ts = ImGui::CalcTextSize("Touchpad");
    dl->AddText(ImVec2(tp_cx - tp_ts.x * 0.5f, tp_cy - tp_ts.y * 0.5f), ui::u32(palette.text), "Touchpad");

    if (!app.is_layout_edit_mode) {
        ImGui::SetCursorScreenPos(tp_min);
        ImGui::InvisibleButton("##touchpad", tp_max - tp_min);
        if (ImGui::IsItemClicked()) state.button_states[17] = !state.button_states[17];
    }

    // ── L3/R3 Indicators ────────────────────────────────────

    bool l3_active = state.button_states[10] || (app.is_layout_edit_mode && app.selected_layout_component == LC_LeftStick);
    bool r3_active = state.button_states[11] || (app.is_layout_edit_mode && app.selected_layout_component == LC_RightStick);

    if (l3_active) {
        ImVec4 l_col(layout.l_stick.color[0], layout.l_stick.color[1], layout.l_stick.color[2], layout.l_stick.color[3]);
        dl->AddText(ImVec2(l.l_stick_c.x - 8, l.l_stick_c.y - (l.stick_radius * layout.l_stick.scale) - 16), ui::u32(l_col), "L3");
    }
    if (r3_active) {
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
        
        if (app.selected_layout_component != LC_None && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            float dx = drag_delta.x / size;
            float dy = drag_delta.y / size;
            
            auto& temp = app.temp_layout;
            switch (app.selected_layout_component) {
                case LC_LeftStick:     temp.l_stick.x_offset += dx; temp.l_stick.y_offset += dy; break;
                case LC_RightStick:    temp.r_stick.x_offset += dx; temp.r_stick.y_offset += dy; break;
                case LC_Dpad:
                    temp.dpad_up.x_offset += dx; temp.dpad_up.y_offset += dy;
                    temp.dpad_down.x_offset += dx; temp.dpad_down.y_offset += dy;
                    temp.dpad_left.x_offset += dx; temp.dpad_left.y_offset += dy;
                    temp.dpad_right.x_offset += dx; temp.dpad_right.y_offset += dy;
                    break;
                case LC_FaceButtons:   temp.face_buttons.x_offset += dx; temp.face_buttons.y_offset += dy; break;
                case LC_ShouldersL:    temp.shoulders_l.x_offset += dx; temp.shoulders_l.y_offset += dy; break;
                case LC_ShouldersR:    temp.shoulders_r.x_offset += dx; temp.shoulders_r.y_offset += dy; break;
                case LC_Touchpad:      temp.touchpad.x_offset += dx; temp.touchpad.y_offset += dy; break;
                case LC_Create:        temp.create_btn.x_offset += dx; temp.create_btn.y_offset += dy; break;
                case LC_Options:       temp.options_btn.x_offset += dx; temp.options_btn.y_offset += dy; break;
                case LC_PS:            temp.ps_btn.x_offset += dx; temp.ps_btn.y_offset += dy; break;
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

