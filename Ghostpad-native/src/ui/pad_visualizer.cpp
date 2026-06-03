// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2024  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>

namespace ghostpad {

static bool pointInCircle(ImVec2 p, ImVec2 c, float r) {
    float dx = p.x - c.x, dy = p.y - c.y;
    return dx * dx + dy * dy <= r * r;
}

static bool pointInRect(ImVec2 p, ImVec2 min, ImVec2 max) {
    return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
}

static bool pointInTriangle(ImVec2 p, ImVec2 a, ImVec2 b, ImVec2 c) {
    float d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
    float d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);
    float d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);
    bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos);
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

    // Buttons by ID
    struct HitRegion {
        ImVec2 center; float radius; int btn_id; int alt_btn;
    };
    std::vector<HitRegion> circles;
};

static InteractivePadLayout computeLayout(ImVec2 base, float size) {
    InteractivePadLayout l;
    l.base = base;
    l.size = size;
    l.w = size * 2.0f;
    l.h = size * 1.2f;
    l.cx = base.x + l.w * 0.5f;
    l.cy = base.y + l.h * 0.5f;

    // Sticks
    l.l_stick_c = ImVec2(base.x + l.w * 0.2f, base.y + l.h * 0.55f);
    l.r_stick_c = ImVec2(base.x + l.w * 0.8f, l.l_stick_c.y);
    l.stick_radius = size * 0.12f;

    // D-pad
    l.dpad_center = ImVec2(l.l_stick_c.x - size * 0.25f, base.y + l.h * 0.25f);
    l.dpad_size = size * 0.06f;

    // Face buttons
    l.fb_center = ImVec2(l.r_stick_c.x + size * 0.25f, base.y + l.h * 0.22f);
    l.fb_radius = size * 0.055f;
    l.fb_dist = size * 0.08f;

    // Shoulders
    l.shoulder_y = base.y + l.h * 0.05f;
    l.trig_width = size * 0.12f;
    l.trig_height = size * 0.12f;

    // Circle hit regions: ID, center, radius
    l.circles = {
        {l.fb_center + ImVec2(0, -l.fb_dist), l.fb_radius, 3, -1},   // Triangle
        {l.fb_center + ImVec2(l.fb_dist, 0), l.fb_radius, 1, -1},    // Circle
        {l.fb_center + ImVec2(0, l.fb_dist), l.fb_radius, 0, -1},    // Cross
        {l.fb_center + ImVec2(-l.fb_dist, 0), l.fb_radius, 2, -1},   // Square
    };

    return l;
}

void renderInteractivePadVisualizer(PadStateInput& state, float size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 base = ImGui::GetCursorScreenPos();
    ImGuiIO& io = ImGui::GetIO();

    auto l = computeLayout(base, size);

    // Background
    dl->AddRectFilled(base, ImVec2(base.x + l.w, base.y + l.h),
                      IM_COL32(30, 30, 30, 255), 12.0f);

    // Interactive sticks
    auto drawInteractiveStick = [&](ImVec2 sc, int idx_x, int idx_y, const char* label) {
        float r = l.stick_radius;
        // Draw stick base
        dl->AddCircle(sc, r + 2, IM_COL32(80, 80, 80, 255), 32, 2.0f);
        dl->AddCircleFilled(sc, r, IM_COL32(50, 50, 50, 255), 32);

        // Current stick position
        uint8_t xv = state.stick_states[idx_x];
        uint8_t yv = state.stick_states[idx_y];
        float dx = (xv - 128) / 128.0f * r * 0.7f;
        float dy = (yv - 128) / 128.0f * r * 0.7f;
        ImVec2 dot_pos(sc.x + dx, sc.y + dy);
        dl->AddCircleFilled(dot_pos, r * 0.4f, IM_COL32(100, 200, 140, 255), 16);

        dl->AddText(ImVec2(sc.x - 8, sc.y + r + 4), IM_COL32(180, 180, 180, 255), label);

        // Invisible button for drag interaction
        ImGui::SetCursorScreenPos(ImVec2(sc.x - r, sc.y - r));
        ImGui::InvisibleButton((std::string("stick_") + label).c_str(), ImVec2(r * 2, r * 2));

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 mouse = ImGui::GetMousePos();
            float rx = std::clamp((mouse.x - sc.x) / (r * 0.7f) * 128.0f + 128.0f, 0.0f, 255.0f);
            float ry = std::clamp((mouse.y - sc.y) / (r * 0.7f) * 128.0f + 128.0f, 0.0f, 255.0f);
            state.stick_states[idx_x] = static_cast<uint8_t>(rx);
            state.stick_states[idx_y] = static_cast<uint8_t>(ry);
        }

        if (!ImGui::IsItemActive() && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // Snap back to center
            if (state.stick_states[idx_x] != 128 || state.stick_states[idx_y] != 128) {
                // Gradual return or instant? Let's do instant snap back on release
                // Only snap if this stick was being dragged (we'd need per-stick state, skip for now)
            }
        }

        // L3/R3 click
        if (ImGui::IsItemClicked()) {
            int btn_id = (label[0] == 'L') ? 10 : 11;
            state.button_states[btn_id] = !state.button_states[btn_id];
        }
    };

    drawInteractiveStick(l.l_stick_c, 0, 1, "L");
    drawInteractiveStick(l.r_stick_c, 2, 3, "R");

    // D-pad with clickable regions
    float ds = l.dpad_size;

    // Up triangle
    ImVec2 up_a(l.dpad_center.x, l.dpad_center.y - ds);
    ImVec2 up_b(l.dpad_center.x - ds, l.dpad_center.y + ds * 0.3f);
    ImVec2 up_c(l.dpad_center.x + ds, l.dpad_center.y + ds * 0.3f);
    ImU32 up_col = state.button_states[12] ? IM_COL32(200, 200, 200, 255) : IM_COL32(70, 70, 70, 255);
    dl->AddTriangleFilled(up_a, up_b, up_c, up_col);

    // Down triangle
    ImVec2 dn_a(l.dpad_center.x, l.dpad_center.y + ds);
    ImVec2 dn_b(l.dpad_center.x - ds, l.dpad_center.y - ds * 0.3f);
    ImVec2 dn_c(l.dpad_center.x + ds, l.dpad_center.y - ds * 0.3f);
    ImU32 dn_col = state.button_states[13] ? IM_COL32(200, 200, 200, 255) : IM_COL32(70, 70, 70, 255);
    dl->AddTriangleFilled(dn_a, dn_b, dn_c, dn_col);

    // Left triangle
    ImVec2 lf_a(l.dpad_center.x - ds, l.dpad_center.y);
    ImVec2 lf_b(l.dpad_center.x + ds * 0.3f, l.dpad_center.y - ds);
    ImVec2 lf_c(l.dpad_center.x + ds * 0.3f, l.dpad_center.y + ds);
    ImU32 lf_col = state.button_states[14] ? IM_COL32(200, 200, 200, 255) : IM_COL32(70, 70, 70, 255);
    dl->AddTriangleFilled(lf_a, lf_b, lf_c, lf_col);

    // Right triangle
    ImVec2 rt_a(l.dpad_center.x + ds, l.dpad_center.y);
    ImVec2 rt_b(l.dpad_center.x - ds * 0.3f, l.dpad_center.y - ds);
    ImVec2 rt_c(l.dpad_center.x - ds * 0.3f, l.dpad_center.y + ds);
    ImU32 rt_col = state.button_states[15] ? IM_COL32(200, 200, 200, 255) : IM_COL32(70, 70, 70, 255);
    dl->AddTriangleFilled(rt_a, rt_b, rt_c, rt_col);

    // D-pad invisible buttons
    float dpad_btn_size = ds * 1.2f;
    ImVec2 dpad_btn_offset(dpad_btn_size, dpad_btn_size);

    const struct { const char* id; ImVec2 pos; int btn; } dpad_btns[] = {
        {"##dpad_up",    {l.dpad_center.x - dpad_btn_size * 0.5f, l.dpad_center.y - dpad_btn_size * 2.0f}, 12},
        {"##dpad_down",  {l.dpad_center.x - dpad_btn_size * 0.5f, l.dpad_center.y + dpad_btn_size * 0.7f}, 13},
        {"##dpad_left",  {l.dpad_center.x - dpad_btn_size * 2.0f, l.dpad_center.y - dpad_btn_size * 0.5f}, 14},
        {"##dpad_right", {l.dpad_center.x + dpad_btn_size * 0.7f, l.dpad_center.y - dpad_btn_size * 0.5f}, 15},
    };

    for (auto& db : dpad_btns) {
        ImGui::SetCursorScreenPos(db.pos);
        ImGui::InvisibleButton(db.id, dpad_btn_offset);
        if (ImGui::IsItemActive()) state.button_states[db.btn] = true;
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !ImGui::IsItemHovered()) state.button_states[db.btn] = false;
    }

    // Face buttons with invisible overlays
    const struct { ImVec2 off; int id; ImU32 col; const char* txt; } face_btns[] = {
        {{0, -l.fb_dist}, 3, IM_COL32(60, 180, 120, 200), "T"},
        {{l.fb_dist, 0},  1, IM_COL32(200, 60, 60, 200),  "O"},
        {{0, l.fb_dist},  0, IM_COL32(60, 120, 200, 200), "X"},
        {{-l.fb_dist, 0}, 2, IM_COL32(200, 120, 180, 200),"S"},
    };

    for (auto& fb : face_btns) {
        ImVec2 fc(l.fb_center.x + fb.off.x, l.fb_center.y + fb.off.y);
        ImU32 col = state.button_states[fb.id] ? IM_COL32(255, 255, 255, 255) : fb.col;
        dl->AddCircleFilled(fc, l.fb_radius, col, 16);
        dl->AddCircle(fc, l.fb_radius + 1, IM_COL32(120, 120, 120, 255), 16, 1.5f);
        auto ts = ImGui::CalcTextSize(fb.txt);
        dl->AddText(ImVec2(fc.x - ts.x * 0.5f, fc.y - ts.y * 0.5f), IM_COL32(0, 0, 0, 255), fb.txt);

        // Invisible clickable area
        float r2 = l.fb_radius + 4;
        ImGui::SetCursorScreenPos(ImVec2(fc.x - r2, fc.y - r2));
        ImGui::InvisibleButton((std::string("##face") + fb.txt).c_str(), ImVec2(r2 * 2, r2 * 2));
        if (ImGui::IsItemActive()) state.button_states[fb.id] = true;
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !ImGui::IsItemHovered()) state.button_states[fb.id] = false;
    }

    // Shoulder L1/R1
    auto drawShoulderInteractive = [&](ImVec2 sp, int id, const char* txt) {
        ImVec2 sz(l.size * 0.15f, l.size * 0.06f);
        ImU32 col = state.button_states[id] ? IM_COL32(180, 180, 180, 255) : IM_COL32(60, 60, 60, 255);
        dl->AddRectFilled(sp, ImVec2(sp.x + sz.x, sp.y + sz.y), col, 4.0f);
        dl->AddText(ImVec2(sp.x + l.size * 0.02f, sp.y), IM_COL32(200, 200, 200, 255), txt);

        ImGui::SetCursorScreenPos(sp);
        ImGui::InvisibleButton((std::string("##shldr") + txt).c_str(), sz);
        if (ImGui::IsItemActive()) state.button_states[id] = true;
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !ImGui::IsItemHovered()) state.button_states[id] = false;
    };

    drawShoulderInteractive(ImVec2(base.x + l.w * 0.05f, l.shoulder_y), 4, "L1");
    drawShoulderInteractive(ImVec2(base.x + l.w * 0.80f, l.shoulder_y), 5, "R1");

    // L2/R2 triggers
    float l2_val = state.trigger_l2 / 255.0f;
    float r2_val = state.trigger_r2 / 255.0f;
    ImVec2 l2_top(base.x + l.w * 0.05f, l.shoulder_y + l.size * 0.02f);
    ImVec2 l2_sz(l.trig_width, l.trig_height);
    ImVec2 r2_top(base.x + l.w * 0.85f, l.shoulder_y + l.size * 0.02f);
    ImVec2 r2_sz(l.trig_width, l.trig_height);

    dl->AddRectFilled(l2_top, l2_top + l2_sz, IM_COL32(50, 50, 50, 255), 4.0f);
    dl->AddRectFilled(ImVec2(l2_top.x, l2_top.y + l.trig_height * (1.0f - l2_val)),
                      ImVec2(l2_top.x + l.trig_width, l2_top.y + l.trig_height),
                      IM_COL32(150, 150, 150, 255), 4.0f);
    dl->AddText(ImVec2(l2_top.x, l2_top.y + l.trig_height + 4), IM_COL32(200, 200, 200, 255), "L2");

    dl->AddRectFilled(r2_top, r2_top + r2_sz, IM_COL32(50, 50, 50, 255), 4.0f);
    dl->AddRectFilled(ImVec2(r2_top.x, r2_top.y + l.trig_height * (1.0f - r2_val)),
                      ImVec2(r2_top.x + l.trig_width, r2_top.y + l.trig_height),
                      IM_COL32(150, 150, 150, 255), 4.0f);
    dl->AddText(ImVec2(r2_top.x, r2_top.y + l.trig_height + 4), IM_COL32(200, 200, 200, 255), "R2");

    // L2/R2 clickable
    ImGui::SetCursorScreenPos(l2_top);
    ImGui::InvisibleButton("##L2", l2_sz);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        float val = 1.0f - (ImGui::GetMousePos().y - l2_top.y) / l.trig_height;
        state.trigger_l2 = static_cast<uint8_t>(std::clamp(val * 255.0f, 0.0f, 255.0f));
    } else if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        state.trigger_l2 = 255;
    } else if (!ImGui::IsItemActive()) {
        state.trigger_l2 = 0;
    }

    ImGui::SetCursorScreenPos(r2_top);
    ImGui::InvisibleButton("##R2", r2_sz);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        float val = 1.0f - (ImGui::GetMousePos().y - r2_top.y) / l.trig_height;
        state.trigger_r2 = static_cast<uint8_t>(std::clamp(val * 255.0f, 0.0f, 255.0f));
    } else if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        state.trigger_r2 = 255;
    } else if (!ImGui::IsItemActive()) {
        state.trigger_r2 = 0;
    }

    // Center buttons (Create, PS, Options)
    float cy = base.y + l.h * 0.08f;
    float cbr = l.size * 0.035f;

    auto drawCenterInteractive = [&](float bx, int id, const char* lbl) {
        ImU32 col = state.button_states[id] ? IM_COL32(180, 180, 180, 255) : IM_COL32(60, 60, 60, 255);
        dl->AddCircleFilled(ImVec2(bx, cy), cbr, col, 8);
        dl->AddText(ImVec2(bx - 4, cy + cbr + 2), IM_COL32(160, 160, 160, 255), lbl);

        ImGui::SetCursorScreenPos(ImVec2(bx - cbr - 4, cy - cbr - 4));
        ImGui::InvisibleButton((std::string("##ctr") + lbl).c_str(), ImVec2((cbr + 4) * 2, (cbr + 4) * 2));
        if (ImGui::IsItemClicked()) state.button_states[id] = !state.button_states[id];
    };

    drawCenterInteractive(l.cx - l.size * 0.2f, 8, "Create");
    drawCenterInteractive(l.cx, 16, "PS");
    drawCenterInteractive(l.cx + l.size * 0.2f, 9, "Options");

    // Touchpad
    float touch_y = base.y + l.h * 0.15f;
    ImVec2 tp_min(l.cx - l.size * 0.3f, touch_y);
    ImVec2 tp_max(l.cx + l.size * 0.3f, touch_y + l.size * 0.08f);
    ImU32 touch_col = state.button_states[17] ? IM_COL32(140, 140, 140, 255) : IM_COL32(45, 45, 45, 255);
    dl->AddRectFilled(tp_min, tp_max, touch_col, 6.0f);
    dl->AddText(ImVec2(l.cx - 25, touch_y + l.size * 0.01f), IM_COL32(150, 150, 150, 255), "Touchpad");

    ImGui::SetCursorScreenPos(tp_min);
    ImGui::InvisibleButton("##touchpad", tp_max - tp_min);
    if (ImGui::IsItemClicked()) state.button_states[17] = !state.button_states[17];

    // L3/R3 indicators
    if (state.button_states[10])
        dl->AddText(ImVec2(l.l_stick_c.x - 8, l.l_stick_c.y - l.size * 0.2f), IM_COL32(100, 200, 140, 255), "L3");
    if (state.button_states[11])
        dl->AddText(ImVec2(l.r_stick_c.x - 8, l.r_stick_c.y - l.size * 0.2f), IM_COL32(100, 200, 140, 255), "R3");

    ImGui::Dummy(ImVec2(l.w, l.h + 10));
}

// Non-interactive version (for display only)
void renderPadVisualizer(const PadStateInput& state, float size) {
    PadStateInput copy = state;
    renderInteractivePadVisualizer(copy, size);
}

} // namespace ghostpad
