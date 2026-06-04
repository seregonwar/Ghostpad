#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <cstdint>
#include <cstring>
#include <array>
#include <string>

namespace ghostpad {

constexpr int GPAD_PORT = 6967;
constexpr int CTRL_PORT = 6970;
constexpr int BEEPER_PORT = 9111;
constexpr int SSM_PORT = 9112;
constexpr int ELF_LOAD_PORT = 9021;
constexpr int KLOG_PORT = 3434;
constexpr int ELF_LOAD_ALT_PORT = 9090;

constexpr int PROBE_PORTS[] = {6967, 9021, 9090};

constexpr uint32_t BTN_CROSS    = 0x00004000;
constexpr uint32_t BTN_CIRCLE   = 0x00002000;
constexpr uint32_t BTN_SQUARE   = 0x00008000;
constexpr uint32_t BTN_TRIANGLE = 0x00001000;

constexpr uint32_t BTN_L1       = 0x00000400;
constexpr uint32_t BTN_R1       = 0x00000800;
constexpr uint32_t BTN_L2       = 0x00000100;
constexpr uint32_t BTN_R2       = 0x00000200;

constexpr uint32_t BTN_CREATE   = 0x00000000; // unmapped / disabled for now
constexpr uint32_t BTN_OPTIONS  = 0x00000008;
constexpr uint32_t BTN_L3       = 0x00000002;
constexpr uint32_t BTN_R3       = 0x00000004;

constexpr uint32_t BTN_DPAD_UP    = 0x00000010;
constexpr uint32_t BTN_DPAD_DOWN  = 0x00000040;
constexpr uint32_t BTN_DPAD_LEFT  = 0x00000080;
constexpr uint32_t BTN_DPAD_RIGHT = 0x00000020;

constexpr uint32_t BTN_PS       = 0x00010000;
constexpr uint32_t BTN_TOUCHPAD = 0x00100000;

constexpr uint32_t PS5_BUTTON_BITS[18] = {
    BTN_CROSS, BTN_CIRCLE, BTN_SQUARE, BTN_TRIANGLE,
    BTN_L1, BTN_R1, BTN_L2, BTN_R2,
    BTN_CREATE, BTN_OPTIONS, BTN_L3, BTN_R3,
    BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT,
    BTN_PS, BTN_TOUCHPAD
};

struct GpadNetworkState {
    uint32_t buttons = 0;
    uint8_t lx = 128;
    uint8_t ly = 128;
    uint8_t rx = 128;
    uint8_t ry = 128;
    uint8_t l2 = 0;
    uint8_t r2 = 0;
};

struct PadStateInput {
    bool button_states[22] = {};
    uint8_t stick_states[4] = {128, 128, 128, 128}; // lx, ly, rx, ry
    uint8_t trigger_l2 = 0;
    uint8_t trigger_r2 = 0;
};

inline GpadNetworkState buildGpadState(const PadStateInput& input) {
    GpadNetworkState state;

    for (int i = 0; i < 18; i++) {
        if (input.button_states[i]) {
            state.buttons |= PS5_BUTTON_BITS[i];
        }
    }

    const uint8_t l2 = input.trigger_l2;
    const uint8_t r2 = input.trigger_r2;

    if (l2 > 0) {
        state.buttons |= BTN_L2;
    }

    if (r2 > 0) {
        state.buttons |= BTN_R2;
    }

    state.lx = input.stick_states[0];
    state.ly = input.stick_states[1];
    state.rx = input.stick_states[2];
    state.ry = input.stick_states[3];
    state.l2 = l2;
    state.r2 = r2;

    return state;
}

inline std::array<uint8_t, 16> buildGpadPacket(const GpadNetworkState& state) {
    std::array<uint8_t, 16> buf = {};

    buf[0] = 'G';
    buf[1] = 'P';
    buf[2] = 'A';
    buf[3] = 'D';

    buf[4] = static_cast<uint8_t>((state.buttons >> 24) & 0xFF);
    buf[5] = static_cast<uint8_t>((state.buttons >> 16) & 0xFF);
    buf[6] = static_cast<uint8_t>((state.buttons >> 8) & 0xFF);
    buf[7] = static_cast<uint8_t>(state.buttons & 0xFF);

    buf[8] = state.lx;
    buf[9] = state.ly;
    buf[10] = state.rx;
    buf[11] = state.ry;
    buf[12] = state.l2;
    buf[13] = state.r2;
    buf[14] = 0;
    buf[15] = 0;

    return buf;
}

inline std::array<uint8_t, 8> buildTypePacket(uint32_t device_type) {
    std::array<uint8_t, 8> buf = {};

    buf[0] = 'T';
    buf[1] = 'Y';
    buf[2] = 'P';
    buf[3] = 'E';

    buf[4] = static_cast<uint8_t>(device_type & 0xFF);
    buf[5] = static_cast<uint8_t>((device_type >> 8) & 0xFF);
    buf[6] = static_cast<uint8_t>((device_type >> 16) & 0xFF);
    buf[7] = static_cast<uint8_t>((device_type >> 24) & 0xFF);

    return buf;
}

inline std::array<uint8_t, 24> buildGbndPacket(uint32_t user, uint64_t virt, uint64_t phys) {
    std::array<uint8_t, 24> buf = {};

    buf[0] = 'G';
    buf[1] = 'B';
    buf[2] = 'N';
    buf[3] = 'D';

    buf[4] = static_cast<uint8_t>(user & 0xFF);
    buf[5] = static_cast<uint8_t>((user >> 8) & 0xFF);
    buf[6] = static_cast<uint8_t>((user >> 16) & 0xFF);
    buf[7] = static_cast<uint8_t>((user >> 24) & 0xFF);

    for (int i = 0; i < 8; i++) {
        buf[8 + i] = static_cast<uint8_t>((virt >> (i * 8)) & 0xFF);
    }

    for (int i = 0; i < 8; i++) {
        buf[16 + i] = static_cast<uint8_t>((phys >> (i * 8)) & 0xFF);
    }

    return buf;
}

inline std::array<uint8_t, 4> buildDiscPacket() {
    return {'D', 'I', 'S', 'C'};
}

inline std::array<uint8_t, 4> buildUnptPacket() {
    return {'U', 'N', 'P', 'T'};
}

} // namespace ghostpad
