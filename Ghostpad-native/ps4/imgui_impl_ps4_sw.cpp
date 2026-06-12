// ImGui PS4 Software Rasterizer Backend
// Renders ImGui draw data to an RGBA8888 framebuffer via CPU rasterization

#include "imgui_impl_ps4_sw.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

static uint32_t* g_fb = nullptr;
static int g_width = 0;
static int g_height = 0;
static const uint32_t* g_font_tex = nullptr;
static int g_font_tex_w = 0;
static int g_font_tex_h = 0;

// Edge function for barycentric coordinates
static inline float EdgeFunc(const ImVec2& a, const ImVec2& b, const ImVec2& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

// Unpack ImU32 to float RGBA
static inline void UnpackColor(ImU32 col, float& r, float& g, float& b, float& a) {
    r = ((col >> 0)  & 0xFF) / 255.0f;
    g = ((col >> 8)  & 0xFF) / 255.0f;
    b = ((col >> 16) & 0xFF) / 255.0f;
    a = ((col >> 24) & 0xFF) / 255.0f;
}

// Pack float RGBA to ImU32
static inline uint32_t PackColor(float r, float g, float b, float a) {
    int ir = (int)(r * 255.0f); if (ir < 0) ir = 0; if (ir > 255) ir = 255;
    int ig = (int)(g * 255.0f); if (ig < 0) ig = 0; if (ig > 255) ig = 255;
    int ib = (int)(b * 255.0f); if (ib < 0) ib = 0; if (ib > 255) ib = 255;
    int ia = (int)(a * 255.0f); if (ia < 0) ia = 0; if (ia > 255) ia = 255;
    return (uint32_t)((ir) | (ig << 8) | (ib << 16) | (ia << 24));
}

static void RasterizeTriangle(
    const ImDrawVert& v0, const ImDrawVert& v1, const ImDrawVert& v2,
    const ImVec4& clip_rect) 
{
    // Calculate AABB
    float minX_f = std::max({ clip_rect.x, std::min({v0.pos.x, v1.pos.x, v2.pos.x}) });
    float minY_f = std::max({ clip_rect.y, std::min({v0.pos.y, v1.pos.y, v2.pos.y}) });
    float maxX_f = std::min({ clip_rect.z, std::max({v0.pos.x, v1.pos.x, v2.pos.x}) });
    float maxY_f = std::min({ clip_rect.w, std::max({v0.pos.y, v1.pos.y, v2.pos.y}) });
    
    int minX = (int)minX_f; if (minX < 0) minX = 0;
    int minY = (int)minY_f; if (minY < 0) minY = 0;
    int maxX = (int)maxX_f; if (maxX >= g_width) maxX = g_width - 1;
    int maxY = (int)maxY_f; if (maxY >= g_height) maxY = g_height - 1;
    
    if (minX > maxX || minY > maxY) return;
    
    // Triangle area for barycentric
    float area = EdgeFunc(v0.pos, v1.pos, v2.pos);
    if (area <= 0.00001f) return;
    float invArea = 1.0f / area;
    
    // Unpack vertex colors
    float r0, g0, b0, a0, r1, g1, b1, a1, r2, g2, b2, a2;
    UnpackColor(v0.col, r0, g0, b0, a0);
    UnpackColor(v1.col, r1, g1, b1, a1);
    UnpackColor(v2.col, r2, g2, b2, a2);
    
    // Pre-multiply vertex alpha into RGB (ImGui uses this format)
    r0 *= a0; g0 *= a0; b0 *= a0;
    r1 *= a1; g1 *= a1; b1 *= a1;
    r2 *= a2; g2 *= a2; b2 *= a2;
    
    // Rasterize
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            ImVec2 p((float)x + 0.5f, (float)y + 0.5f);
            
            float w0 = EdgeFunc(v1.pos, v2.pos, p) * invArea;
            float w1 = EdgeFunc(v2.pos, v0.pos, p) * invArea;
            float w2 = EdgeFunc(v0.pos, v1.pos, p) * invArea;
            
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
            
            // Interpolate color and alpha
            float r = w0 * r0 + w1 * r1 + w2 * r2;
            float g = w0 * g0 + w1 * g1 + w2 * g2;
            float b = w0 * b0 + w1 * b1 + w2 * b2;
            float a = w0 * a0 + w1 * a1 + w2 * a2;
            
            // Texture sample (nearest neighbor)
            if (g_font_tex && g_font_tex_w > 0) {
                float u = w0 * v0.uv.x + w1 * v1.uv.x + w2 * v2.uv.x;
                float v = w0 * v0.uv.y + w1 * v1.uv.y + w2 * v2.uv.y;
                int tx = (int)(u * g_font_tex_w) % g_font_tex_w;
                int ty = (int)(v * g_font_tex_h) % g_font_tex_h;
                if (tx < 0) tx = 0; if (ty < 0) ty = 0;
                
                uint32_t texel = g_font_tex[ty * g_font_tex_w + tx];
                float tr, tg, tb, ta;
                UnpackColor(texel, tr, tg, tb, ta);
                
                r *= tr; g *= tg; b *= tb; a *= ta;
            }
            
            if (a <= 0.0f) continue;
            
            // Alpha blend with destination
            int fb_idx = y * g_width + x;
            uint32_t dst = g_fb[fb_idx];
            float dr, dg, db, da;
            UnpackColor(dst, dr, dg, db, da);
            
            float out_r = r + dr * (1.0f - a);
            float out_g = g + dg * (1.0f - a);
            float out_b = b + db * (1.0f - a);
            float out_a = a + da * (1.0f - a);
            
            g_fb[fb_idx] = PackColor(out_r, out_g, out_b, out_a);
        }
    }
}

// ── Public API ────────────────────────────────────────────────

bool ImGui_ImplPS4SW_Init(int width, int height) {
    g_width = width;
    g_height = height;
    
    g_fb = (uint32_t*)calloc(width * height, sizeof(uint32_t));
    if (!g_fb) {
        printf("[PS4SW] Failed to allocate framebuffer %dx%d\n", width, height);
        return false;
    }
    
    // Capture the font texture
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &g_font_tex_w, &g_font_tex_h);
    g_font_tex = (const uint32_t*)pixels;
    
    printf("[PS4SW] Initialized %dx%d, font tex %dx%d\n", width, height, g_font_tex_w, g_font_tex_h);
    return true;
}

void ImGui_ImplPS4SW_Shutdown() {
    if (g_fb) {
        free(g_fb);
        g_fb = nullptr;
    }
    g_font_tex = nullptr;
    printf("[PS4SW] Shutdown\n");
}

void ImGui_ImplPS4SW_NewFrame() {
    if (g_fb) {
        memset(g_fb, 0, g_width * g_height * sizeof(uint32_t));
    }
}

void ImGui_ImplPS4SW_RenderDrawData(ImDrawData* draw_data) {
    if (!g_fb || !draw_data) return;
    
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f) return;
    
    for (int cmd_list_idx = 0; cmd_list_idx < draw_data->CmdListsCount; cmd_list_idx++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[cmd_list_idx];
        const ImDrawVert* vtx_buf = cmd_list->VtxBuffer.Data;
        const ImDrawIdx* idx_buf = cmd_list->IdxBuffer.Data;
        
        for (int cmd_idx = 0; cmd_idx < cmd_list->CmdBuffer.Size; cmd_idx++) {
            const ImDrawCmd* cmd = &cmd_list->CmdBuffer[cmd_idx];
            
            // Skip if no triangles or clipped out
            if (cmd->ElemCount < 3) continue;
            
            // Only sample texture for textured draws (TextureId != nullptr)
            const uint32_t* saved_tex = g_font_tex;
            if (cmd->TexRef._TexID == ImTextureID_Invalid) {
                g_font_tex = nullptr;
            }
            
            // Get clip rect
            ImVec4 clip_rect(
                cmd->ClipRect.x,
                cmd->ClipRect.y,
                cmd->ClipRect.z,
                cmd->ClipRect.w
            );
            
            
            // Draw triangles
            for (unsigned int i = 0; i + 2 < cmd->ElemCount; i += 3) {
                ImDrawIdx i0 = idx_buf[cmd->IdxOffset + i];
                ImDrawIdx i1 = idx_buf[cmd->IdxOffset + i + 1];
                ImDrawIdx i2 = idx_buf[cmd->IdxOffset + i + 2];
                
                RasterizeTriangle(vtx_buf[i0], vtx_buf[i1], vtx_buf[i2], clip_rect);
            }
            
            g_font_tex = saved_tex;
        }
    }
}

const uint8_t* ImGui_ImplPS4SW_GetFramebuffer() {
    return (const uint8_t*)g_fb;
}

int ImGui_ImplPS4SW_GetWidth()  { return g_width; }
int ImGui_ImplPS4SW_GetHeight() { return g_height; }
