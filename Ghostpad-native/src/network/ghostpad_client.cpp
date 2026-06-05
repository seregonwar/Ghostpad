// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "network/ghostpad_client.h"
#include "network/network_scanner.h"
#include "network/socket_util.h"
#include <cstring>
#include <vector>
#include <thread>
#include <algorithm>
#include <map>
#include <mutex>
#include <atomic>

namespace ghostpad {

static bool sockets_initialized = false;

static void initSockets() {
    if (sockets_initialized) return;
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    sockets_initialized = true;
}

GhostpadClient::GhostpadClient() {
    initSockets();
}

GhostpadClient::~GhostpadClient() {
    disconnect();
}

bool GhostpadClient::connect(const std::string& ip, int port, int timeout_ms) {
    /*
     *    [ NON-BLOCKING MUTEX-FREE CONNECT ]
     *    Probes connection on a local socket descriptor first to avoid locking
     *    the instance mutex during select(), which would freeze the GUI thread.
     */
    socket_t temp_sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (temp_sock == INVALID_SOCKET_VAL) return false;

    setNoSigPipe(temp_sock);
    setNonBlocking(temp_sock, true);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    int ret = ::connect(temp_sock, (struct sockaddr*)&addr, sizeof(addr));

    if (ret < 0) {
#ifdef _WIN32
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            closeSocket(temp_sock);
            return false;
        }
#else
        if (errno != EINPROGRESS) {
            closeSocket(temp_sock);
            return false;
        }
#endif
    }

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(temp_sock, &fdset);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    ret = select(static_cast<int>(temp_sock) + 1, nullptr, &fdset, nullptr, &tv);
    if (ret <= 0) {
        closeSocket(temp_sock);
        return false;
    }

    int so_error = 0;
    socklen_t len = sizeof(so_error);
    getsockopt(temp_sock, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
    if (so_error != 0) {
        closeSocket(temp_sock);
        return false;
    }

    setNonBlocking(temp_sock, false);

    // Disable Nagle's algorithm
    int flag = 1;
    setsockopt(temp_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    // Lock the instance state mutex briefly only to commit the successfully opened socket
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connected_ && sock_ != INVALID_SOCKET_VAL) {
            try {
                auto disc = buildDiscPacket();
                ::send(sock_, (const char*)disc.data(), static_cast<int>(disc.size()), 0);
            } catch (...) {}
            closeSocket(sock_);
        }
        sock_ = temp_sock;
        connected_ = true;
        host_ = ip;
        port_ = port;
    }
    return true;
}

void GhostpadClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) return;

    if (sock_ != INVALID_SOCKET_VAL) {
        try {
            auto disc = buildDiscPacket();
            ::send(sock_, (const char*)disc.data(), static_cast<int>(disc.size()), 0);
        } catch (...) {}

        closeSocket(sock_);
        sock_ = INVALID_SOCKET_VAL;
    }
    connected_ = false;
    host_.clear();
    port_ = GPAD_PORT;
}

bool GhostpadClient::sendPadState(const GpadNetworkState& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || sock_ == INVALID_SOCKET_VAL) return false;

    auto packet = buildGpadPacket(state);
    int sent = ::send(sock_, (const char*)packet.data(), static_cast<int>(packet.size()), 0);
    if (sent == static_cast<int>(packet.size())) return true;

    closeSocket(sock_);
    sock_ = INVALID_SOCKET_VAL;
    connected_ = false;
    return false;
}

GhostpadStatus GhostpadClient::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    GhostpadStatus s;
    s.is_connected = connected_;
    s.ip = host_;
    s.port = port_;
    return s;
}

CtrlResult GhostpadClient::sendCtrlPacket(const std::string& ip, const uint8_t* data, size_t len, int timeout_ms) {
    CtrlResult result;

    socket_t sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET_VAL) {
        result.error = "Socket creation failed";
        return result;
    }

    setNoSigPipe(sock);
    setSocketTimeout(sock, SO_SNDTIMEO, timeout_ms);
    setSocketTimeout(sock, SO_RCVTIMEO, timeout_ms);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(CTRL_PORT));
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        result.error = "Connection failed: " + getLastSocketErrorString();
        closeSocket(sock);
        return result;
    }

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    int sent = ::send(sock, (const char*)data, static_cast<int>(len), 0);
    closeSocket(sock);

    if (sent == static_cast<int>(len)) {
        result.ok = true;
    } else {
        result.error = "Send failed";
    }
    return result;
}

CtrlResult GhostpadClient::sendType(const std::string& ip, uint32_t device_type, int timeout_ms) {
    auto packet = buildTypePacket(device_type);
    return sendCtrlPacket(ip, packet.data(), packet.size(), timeout_ms);
}

CtrlResult GhostpadClient::disconnectVirtual(const std::string& ip) {
    auto gpad_disc = buildDiscPacket();
    auto ctrl_disc = buildDiscPacket();

    CtrlResult r1 = sendCtrlPacket(ip, gpad_disc.data(), gpad_disc.size(), 3000);
    CtrlResult r2 = sendCtrlPacket(ip, ctrl_disc.data(), ctrl_disc.size(), 3000);

    CtrlResult result;
    result.ok = r1.ok || r2.ok;
    return result;
}

CtrlResult GhostpadClient::terminatePayload(const std::string& ip, int timeout_ms) {
    auto packet = buildUnptPacket();
    return sendCtrlPacket(ip, packet.data(), packet.size(), timeout_ms);
}

bool GhostpadClient::probeHostPort(const std::string& ip, int port, int timeout_ms) {
    socket_t sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET_VAL) return false;

    setNonBlocking(sock, true);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(static_cast<int>(sock) + 1, nullptr, &fdset, nullptr, &tv);

    bool reachable = false;
    if (ret > 0) {
        int so_error = 0;
        socklen_t len = sizeof(so_error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
        reachable = (so_error == 0);
    }

    closeSocket(sock);
    return reachable;
}

std::vector<ScanResult> GhostpadClient::scanNetwork(const std::string& subnet, int timeout_ms) {
    std::string base = subnet.empty() ? getLocalSubnet() : subnet;
    std::vector<ScanResult> scanned;

    /*
     *    [ MULTI-THREADED SUBNET SCANNER ]
     *    Uses worker threads to check IPs concurrently.
     */
    std::vector<ProbeResult> hits;
    std::mutex hits_mutex;
    std::atomic<int> next_ip{1};

    const int NUM_THREADS = 64;
    std::vector<std::thread> workers;
    for (int t = 0; t < NUM_THREADS; t++) {
        workers.emplace_back([&]() {
            while (true) {
                int i = next_ip++;
                if (i > 254) break;
                
                std::string host = base + "." + std::to_string(i);
                for (int port : PROBE_PORTS) {
                    if (probeHostPort(host, port, timeout_ms)) {
                        std::lock_guard<std::mutex> lock(hits_mutex);
                        hits.push_back({host, port, true});
                    }
                }
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    // Group by IP
    std::map<std::string, ScanResult> by_ip;
    for (auto& hit : hits) {
        auto& entry = by_ip[hit.ip];
        entry.ip = hit.ip;
        bool found = false;
        for (auto p : entry.ports) {
            if (p == hit.port) { found = true; break; }
        }
        if (!found) entry.ports.push_back(hit.port);
        if (hit.port == 6967) entry.has_ghostpad = true;
        if (hit.port == 9021 || hit.port == 9090) entry.has_elfldr = true;
    }

    for (auto& [_, v] : by_ip) {
        scanned.push_back(v);
    }

    std::sort(scanned.begin(), scanned.end(), [](const ScanResult& a, const ScanResult& b) {
        int sa = (a.has_ghostpad ? 2 : 0) + (a.has_elfldr ? 1 : 0);
        int sb = (b.has_ghostpad ? 2 : 0) + (b.has_elfldr ? 1 : 0);
        return sa > sb;
    });

    return scanned;
}

std::string GhostpadClient::getLocalSubnet() {
    return NetworkScanner::getLocalSubnet();
}

} // namespace ghostpad
