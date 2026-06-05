#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <cstdint>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include "protocol/gpad_packet.h"

#include "network/socket_util.h"

namespace ghostpad {

struct GhostpadStatus {
    bool is_connected = false;
    std::string ip;
    int port = GPAD_PORT;
};

struct ProbeResult {
    std::string ip;
    int port;
    bool reachable = false;
};

struct ScanHit {
    std::string ip;
    int port = 0;
    std::string service;
};

struct ScanResult {
    std::string ip;
    std::vector<int> ports;
    bool has_ghostpad = false;
    bool has_elfldr = false;
};

struct CtrlResult {
    bool ok = false;
    std::string error;
};

class GhostpadClient {
public:
    GhostpadClient();
    ~GhostpadClient();

    bool connect(const std::string& ip, int port = GPAD_PORT, int timeout_ms = 5000);
    void disconnect();
    bool sendPadState(const GpadNetworkState& state);
    GhostpadStatus getStatus() const;

    // Control port (6970)
    static CtrlResult sendType(const std::string& ip, uint32_t device_type, int timeout_ms = 3000);
    static CtrlResult disconnectVirtual(const std::string& ip);
    static CtrlResult terminatePayload(const std::string& ip, int timeout_ms = 3000);


    // Network scanning
    static std::vector<ScanResult> scanNetwork(const std::string& subnet = "", int timeout_ms = 600);
    static std::string getLocalSubnet();

private:
    socket_t sock_ = INVALID_SOCKET_VAL;
    bool connected_ = false;
    std::string host_;
    int port_ = GPAD_PORT;
    mutable std::mutex mutex_;

    static CtrlResult sendCtrlPacket(const std::string& ip, const uint8_t* data, size_t len, int timeout_ms);
    static bool probeHostPort(const std::string& ip, int port, int timeout_ms);
};

} // namespace ghostpad
