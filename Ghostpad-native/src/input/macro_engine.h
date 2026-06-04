#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <string>
#include <vector>
#include <chrono>
#include "protocol/gpad_packet.h"
#include "storage/project_store.h"

namespace ghostpad {

class MacroEngine {
public:
    MacroEngine();

    void startRecording();
    void stopRecording();
    bool isRecording() const;
    void recordSignal(int button_id, int value);

    void startPlayback(const std::vector<MacroSignal>& signals);
    void stopPlayback();
    bool isPlaying() const;
    void updatePlayback(double delta_ms);
    PadStateInput getPlaybackState() const;

    std::vector<MacroSignal> getRecordedSignals() const;
    void loadSignals(const std::vector<MacroSignal>& signals);

    static std::vector<MacroSignal> normalizeSignals(const std::vector<MacroSignal>& signals);
    static std::string exportAsPython(const std::vector<MacroSignal>& signals, const std::string& name);

private:
    bool recording_ = false;
    bool playing_ = false;
    std::chrono::steady_clock::time_point record_start_;
    std::chrono::steady_clock::time_point playback_start_;
    std::vector<MacroSignal> recorded_signals_;
    std::vector<MacroSignal> playback_signals_;
    size_t playback_index_ = 0;
    PadStateInput current_state_;
};

} // namespace ghostpad
