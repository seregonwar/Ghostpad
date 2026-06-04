#include "ui/gif_export.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <map>

namespace ghostpad {

static void writeByte(FILE* f, uint8_t b) { fwrite(&b, 1, 1, f); }
static void writeShort(FILE* f, uint16_t s) { writeByte(f, s & 0xFF); writeByte(f, s >> 8); }

static void writeColorTable(FILE* f, const std::vector<uint32_t>& pal, int count) {
    for (int i = 0; i < count && i < (int)pal.size(); i++) {
        writeByte(f, (pal[i] >> 0) & 0xFF);
        writeByte(f, (pal[i] >> 8) & 0xFF);
        writeByte(f, (pal[i] >> 16) & 0xFF);
    }
    uint32_t black = 0x000000FF;
    for (int i = (int)pal.size(); i < count; i++) {
        writeByte(f, (black >> 0) & 0xFF);
        writeByte(f, (black >> 8) & 0xFF);
        writeByte(f, (black >> 16) & 0xFF);
    }
}

// Graphic Control Extension
static void writeGCE(FILE* f, int delay_cs, int transparent_idx) {
    writeByte(f, 0x21);  // Extension introducer
    writeByte(f, 0xF9);  // Graphic Control Label
    writeByte(f, 0x04);  // Block size
    uint8_t flags = 0;
    if (transparent_idx >= 0) flags |= 0x01;  // Transparent color flag
    writeByte(f, flags);
    writeShort(f, delay_cs);
    writeByte(f, transparent_idx >= 0 ? (uint8_t)transparent_idx : 0);
    writeByte(f, 0x00);  // Block terminator
}

// Image Descriptor
static void writeImageDesc(FILE* f, int x, int y, int w, int h, bool hasLocalCT, int ctSize) {
    writeByte(f, 0x2C);  // Image Separator
    writeShort(f, x);
    writeShort(f, y);
    writeShort(f, w);
    writeShort(f, h);
    uint8_t flags = hasLocalCT ? 0x87 : 0x00;  // local color table + sorted
    if (hasLocalCT) {
        flags |= (ctSize - 1);  // size = 2^(ctSize+1)
    }
    writeByte(f, flags);
}

// ---- LZW Encoder (GIF variant) ----

struct LzwDict {
    std::map<uint32_t, int> map;
    int next_code;
    int max_code;
};

static void lzwInit(LzwDict& d, int codeSize) {
    d.map.clear();
    d.next_code = (1 << codeSize) + 2;
    d.max_code = (1 << (codeSize + 1));
}

struct BitWriter {
    std::vector<uint8_t> data;
    int bit_buf = 0;
    int bit_count = 0;
    int byte_count = 0;

    void writeBits(int code, int bits) {
        bit_buf |= (code << bit_count);
        bit_count += bits;
        while (bit_count >= 8) {
            data.push_back(bit_buf & 0xFF);
            bit_buf >>= 8;
            bit_count -= 8;
            byte_count++;
        }
    }

    void flush() {
        if (bit_count > 0) {
            data.push_back(bit_buf & 0xFF);
            byte_count++;
            bit_buf = 0;
            bit_count = 0;
        }
    }
};

static void lzwCompress(const uint8_t* indices, int count, int minCodeSize, std::vector<uint8_t>& out) {
    int clearCode = 1 << minCodeSize;
    int endCode = clearCode + 1;
    int codeSize = minCodeSize + 1;

    LzwDict dict;
    lzwInit(dict, minCodeSize);

    BitWriter bw;
    bw.writeBits(clearCode, codeSize);

    if (count > 0) {
        uint32_t prefix = indices[0];
        for (int i = 1; i < count; i++) {
            uint32_t next = indices[i];
            uint32_t key = (prefix << 8) | next;

            auto it = dict.map.find(key);
            if (it != dict.map.end()) {
                prefix = it->second;
            } else {
                bw.writeBits(prefix, codeSize);
                dict.map[key] = dict.next_code;
                dict.next_code++;
                if (dict.next_code >= dict.max_code && codeSize < 12) {
                    codeSize++;
                    dict.max_code = 1 << codeSize;
                } else if (dict.next_code >= 4096) {
                    bw.writeBits(clearCode, codeSize);
                    lzwInit(dict, minCodeSize);
                    codeSize = minCodeSize + 1;
                }
                prefix = next;
            }
        }
        bw.writeBits(prefix, codeSize);
    }
    bw.writeBits(endCode, codeSize);
    bw.flush();

    out.swap(bw.data);
}

static void writeImageData(FILE* f, const uint8_t* indices, int w, int h) {
    const int minCodeSize = 8;  // 256-color palette = 8-bit

    std::vector<uint8_t> compressed;
    lzwCompress(indices, w * h, minCodeSize, compressed);

    writeByte(f, minCodeSize);

    size_t pos = 0;
    while (pos < compressed.size()) {
        int blockLen = (int)std::min(compressed.size() - pos, (size_t)255);
        writeByte(f, (uint8_t)blockLen);
        fwrite(compressed.data() + pos, 1, blockLen, f);
        pos += blockLen;
    }
    writeByte(f, 0x00);
}

// ---- GIF File Writing ----

static void writeNetscapeExt(FILE* f) {
    writeByte(f, 0x21);  // Extension introducer
    writeByte(f, 0xFF);  // Application Extension Label
    writeByte(f, 0x0B);  // Block size (11)
    fwrite("NETSCAPE2.0", 1, 11, f);
    writeByte(f, 0x03);  // Sub-block size
    writeByte(f, 0x00);  // Loop count lo (0 = infinite)
    writeByte(f, 0x00);  // Loop count hi
    writeByte(f, 0x00);  // Block terminator
}

// ---- Quantization ----

static uint32_t packRgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

static uint8_t colorDiff(uint32_t a, uint32_t b) {
    int dr = (int)((a >> 0) & 0xFF) - (int)((b >> 0) & 0xFF);
    int dg = (int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF);
    int db = (int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF);
    int da = (int)((a >> 24) & 0xFF) - (int)((b >> 24) & 0xFF);
    return (uint8_t)std::min(255, (std::abs(dr) + std::abs(dg) + std::abs(db) + std::abs(da)) / 4);
}

// ---- Public API ----

GifExporter::GifExporter() {}
GifExporter::~GifExporter() { cancel(); }

void GifExporter::beginCapture(int width, int height, int fps) {
    cancel();
    width_ = width;
    height_ = height;
    fps_ = fps;
    frames_.clear();
    total_frames_ = 0;
    capturing_ = true;
    cancelled_ = false;
}

void GifExporter::addFrame(const std::vector<uint8_t>& rgba_pixels) {
    if (!capturing_ || cancelled_) return;

    Frame f;
    f.delay_cs = (int)(100.0f / fps_ + 0.5f);
    quantizeFrame(rgba_pixels, f);
    frames_.push_back(std::move(f));
}

float GifExporter::progress() const {
    if (total_frames_ <= 0) return 0.0f;
    return (float)frames_.size() / (float)total_frames_;
}

void GifExporter::cancel() {
    capturing_ = false;
    cancelled_ = true;
    frames_.clear();
    width_ = height_ = 0;
}

bool GifExporter::isKeyColor(uint8_t r, uint8_t g, uint8_t b) const {
    return (r > 220 && g < 80 && b > 220);
}

uint32_t GifExporter::sampleColor(const std::vector<uint8_t>& rgba, int x, int y) const {
    int idx = (y * width_ + x) * 4;
    return packRgba(rgba[idx], rgba[idx + 1], rgba[idx + 2], rgba[idx + 3]);
}

void GifExporter::quantizeFrame(const std::vector<uint8_t>& rgba, Frame& out) {
    int total = width_ * height_;
    out.indexed.resize(total, 0);
    out.palette.clear();
    out.transparent_idx = 0;

    // Collect unique colors (sample up to 16K pixels)
    std::map<uint32_t, int> colorCount;
    int sample_step = std::max(1, total / 16384);
    int key_color_count = 0;

    for (int i = 0; i < total; i += sample_step) {
        uint8_t r = rgba[i * 4 + 0];
        uint8_t g = rgba[i * 4 + 1];
        uint8_t b = rgba[i * 4 + 2];
        uint8_t a = rgba[i * 4 + 3];

        if (a < 10 || isKeyColor(r, g, b)) {
            key_color_count++;
            continue;
        }
        colorCount[packRgba(r, g, b, 255)]++;
    }

    // Always reserve index 0 for transparency
    out.palette.push_back(0x00000000);
    out.transparent_idx = 0;

    if (!colorCount.empty()) {
        std::vector<std::pair<uint32_t, int>> sorted(colorCount.begin(), colorCount.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        int max_colors = std::min(255, (int)sorted.size());
        for (int i = 0; i < max_colors; i++) {
            out.palette.push_back(sorted[i].first);
        }

        // Fill remaining palette slots with a gradient of grays for better dithering
        while ((int)out.palette.size() < 256) {
            int idx = (int)out.palette.size();
            uint8_t gray = (uint8_t)(idx * 255 / 255);
            out.palette.push_back(packRgba(gray, gray, gray, 255));
        }
    } else {
        for (int i = 1; i < 256; i++) {
            uint8_t v = (uint8_t)i;
            out.palette.push_back(packRgba(v, v, v, 255));
        }
    }

    // Map pixels to palette indices
    for (int i = 0; i < total; i++) {
        uint8_t r = rgba[i * 4 + 0];
        uint8_t g = rgba[i * 4 + 1];
        uint8_t b = rgba[i * 4 + 2];
        uint8_t a = rgba[i * 4 + 3];

        if (a < 10 || isKeyColor(r, g, b)) {
            out.indexed[i] = 0;
            continue;
        }

        uint32_t px = packRgba(r, g, b, 255);
        int best_idx = 0;
        uint8_t best_diff = 255;

        for (int j = 1; j < (int)out.palette.size(); j++) {
            uint8_t d = colorDiff(px, out.palette[j]);
            if (d < best_diff) {
                best_diff = d;
                best_idx = j;
                if (d == 0) break;
            }
        }
        out.indexed[i] = (uint8_t)best_idx;
    }
}

bool GifExporter::finishExport(const std::string& filepath) {
    capturing_ = false;
    if (cancelled_ || frames_.empty()) return false;
    return writeGifFile(filepath);
}

bool GifExporter::writeGifFile(const std::string& filepath) {
    FILE* f = fopen(filepath.c_str(), "wb");
    if (!f) return false;

    // Header
    fwrite("GIF89a", 1, 6, f);

    // Logical Screen Descriptor
    int ctSize = 8;  // 256 colors = 2^(7+1)
    writeShort(f, (uint16_t)width_);
    writeShort(f, (uint16_t)height_);
    uint8_t flags = 0xF0 | (ctSize - 1);  // global color table, 8 bits per primary, size
    writeByte(f, flags);
    writeByte(f, 0);  // Background color index
    writeByte(f, 0);  // Pixel aspect ratio (1:1)
    writeColorTable(f, frames_[0].palette, 256);

    writeNetscapeExt(f);

    for (auto& frame : frames_) {
        writeGCE(f, frame.delay_cs, frame.transparent_idx);
        writeImageDesc(f, 0, 0, width_, height_, true, ctSize);
        writeColorTable(f, frame.palette, 256);
        writeImageData(f, frame.indexed.data(), width_, height_);
    }

    writeByte(f, 0x3B);  // Trailer
    fclose(f);
    return true;
}

} // namespace ghostpad
