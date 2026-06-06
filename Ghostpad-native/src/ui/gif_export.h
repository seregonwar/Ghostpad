#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace ghostpad {

struct PadStateInput;

class GifExporter {
public:
    GifExporter();
    ~GifExporter();

    void beginCapture(int width, int height, int fps);
    void addFrame(const std::vector<uint8_t>& rgba_pixels);
    bool finishExport(const std::string& filepath);

    int frameCount() const { return static_cast<int>(frames_.size()); }
    float progress() const;

    bool isCapturing() const { return capturing_; }
    void cancel();

    void setTotalFrames(int n) { total_frames_ = n; }

private:
    bool capturing_ = false;
    bool cancelled_ = false;
    int width_ = 0;
    int height_ = 0;
    int fps_ = 30;
    int total_frames_ = 0;

    struct Frame {
        int delay_cs;
        std::vector<uint8_t> indexed;   // palette indices
        std::vector<uint32_t> palette;  // RGBA8888 palette (256 colors)
        int transparent_idx;
    };
    std::vector<Frame> frames_;

    void quantizeFrame(const std::vector<uint8_t>& rgba, Frame& out);
    uint32_t sampleColor(const std::vector<uint8_t>& rgba, int x, int y) const;
    bool writeGifFile(const std::string& filepath);
};

} // namespace ghostpad
