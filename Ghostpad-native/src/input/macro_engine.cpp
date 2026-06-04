// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "input/macro_engine.h"
#include <algorithm>
#include <sstream>
#include <regex>

namespace ghostpad {

MacroEngine::MacroEngine() {}

void MacroEngine::startRecording() {
    recording_ = true;
    recorded_signals_.clear();
    record_start_ = std::chrono::steady_clock::now();
}

void MacroEngine::stopRecording() {
    recording_ = false;
}

bool MacroEngine::isRecording() const {
    return recording_;
}

void MacroEngine::recordSignal(int button_id, int value) {
    if (!recording_) return;
    auto now = std::chrono::steady_clock::now();
    int elapsed_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - record_start_).count()
    );
    recorded_signals_.push_back({button_id, value, elapsed_ms});
}

void MacroEngine::startPlayback(const std::vector<MacroSignal>& signals) {
    playback_signals_ = normalizeSignals(signals);
    playback_index_ = 0;
    playing_ = true;
    playback_start_ = std::chrono::steady_clock::now();
    current_state_ = {};
}

void MacroEngine::stopPlayback() {
    playing_ = false;
    current_state_ = {};
}

bool MacroEngine::isPlaying() const {
    return playing_;
}

void MacroEngine::updatePlayback(double delta_ms) {
    if (!playing_) return;

    auto now = std::chrono::steady_clock::now();
    int elapsed = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - playback_start_).count()
    );

    current_state_ = {};

    // Apply all signals that are due
    while (playback_index_ < playback_signals_.size() &&
           playback_signals_[playback_index_].time_ms <= elapsed) {
        auto& signal = playback_signals_[playback_index_];
        if (signal.button_id < 18) {
            current_state_.button_states[signal.button_id] = (signal.value > 0);
        } else if (signal.button_id == 18) {
            current_state_.stick_states[0] = static_cast<uint8_t>(signal.value);
        } else if (signal.button_id == 19) {
            current_state_.stick_states[1] = static_cast<uint8_t>(signal.value);
        } else if (signal.button_id == 20) {
            current_state_.stick_states[2] = static_cast<uint8_t>(signal.value);
        } else if (signal.button_id == 21) {
            current_state_.stick_states[3] = static_cast<uint8_t>(signal.value);
        } else if (signal.button_id == 22) {
            current_state_.trigger_l2 = static_cast<uint8_t>(signal.value);
        } else if (signal.button_id == 23) {
            current_state_.trigger_r2 = static_cast<uint8_t>(signal.value);
        }
        playback_index_++;
    }

    if (playback_index_ >= playback_signals_.size()) {
        playing_ = false;
    }
}

PadStateInput MacroEngine::getPlaybackState() const {
    return current_state_;
}

std::vector<MacroSignal> MacroEngine::getRecordedSignals() const {
    return recorded_signals_;
}

void MacroEngine::loadSignals(const std::vector<MacroSignal>& signals) {
    recorded_signals_ = signals;
}

std::vector<MacroSignal> MacroEngine::normalizeSignals(const std::vector<MacroSignal>& signals) {
    auto result = signals;
    std::sort(result.begin(), result.end(), [](const MacroSignal& a, const MacroSignal& b) {
        return a.time_ms < b.time_ms;
    });
    for (auto& sig : result) {
        sig.value = std::clamp(sig.value, 0, 255);
    }
    return result;
}

std::string MacroEngine::exportAsPython(const std::vector<MacroSignal>& signals, const std::string& name) {
    std::ostringstream oss;
    oss << "# Ghostpad Macro: " << name << "\n";
    oss << "# Auto-generated Python replay script\n\n";
    oss << "import socket\n";
    oss << "import time\n";
    oss << "import struct\n\n";
    oss << "GPAD_PORT = 6967\n\n";
    oss << "def build_packet(buttons, lx, ly, rx, ry, l2, r2):\n";
    oss << "    return struct.pack('>4sI6B2x', b'GPAD', buttons, lx, ly, rx, ry, l2, r2)\n\n";
    oss << "def play(ip):\n";
    oss << "    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)\n";
    oss << "    sock.connect((ip, GPAD_PORT))\n";
    oss << "    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)\n\n";
    oss << "    signals = [\n";

    for (auto& sig : signals) {
        oss << "        (" << sig.time_ms << ", " << sig.button_id << ", " << sig.value << "),\n";
    }

    oss << "    ]\n\n";
    oss << "    buttons = 0\n";
    oss << "    lx = ly = rx = ry = 128\n";
    oss << "    l2 = r2 = 0\n";
    oss << "    last_time = 0\n\n";
    oss << "    for t, btn, val in signals:\n";
    oss << "        delay = (t - last_time) / 1000.0\n";
    oss << "        if delay > 0:\n";
    oss << "            time.sleep(delay)\n";
    oss << "        last_time = t\n";
    oss << "        # Apply signal (simplified - full button bitmask logic needed)\n";
    oss << "        pkt = build_packet(buttons, lx, ly, rx, ry, l2, r2)\n";
    oss << "        sock.send(pkt)\n\n";
    oss << "    sock.close()\n\n";
    oss << "if __name__ == '__main__':\n";
    oss << "    play('PS5_IP_HERE')\n";

    return oss.str();
}

std::vector<MacroSignal> MacroEngine::importSignalsFromJson(const std::string& json) {
    std::vector<MacroSignal> signals;
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        if (j.is_array()) {
            for (auto& item : j) {
                MacroSignal sig;
                sig.button_id = item.value("button_id", 0);
                sig.value = item.value("value", 0);
                sig.time_ms = item.value("time_ms", 0);
                signals.push_back(sig);
            }
        }
    } catch (...) {}
    return normalizeSignals(signals);
}

std::vector<MacroSignal> MacroEngine::importSignalsFromPython(const std::string& py) {
    std::vector<MacroSignal> signals;
    std::regex tuple_re(R"(\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\))");
    std::sregex_iterator it(py.begin(), py.end(), tuple_re);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        try {
            int time_ms = std::stoi((*it)[1].str());
            int button_id = std::stoi((*it)[2].str());
            int value = std::stoi((*it)[3].str());
            signals.push_back({button_id, value, time_ms});
        } catch (...) {}
    }
    return normalizeSignals(signals);
}

} // namespace ghostpad
