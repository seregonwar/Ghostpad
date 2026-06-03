// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "network/ghostpad_client.h"
#include <cstring>
#include <vector>
#include <thread>
#include <algorithm>
#include <map>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

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

static void setNonBlocking(int sock, bool nb) {
#ifdef _WIN32
    u_long mode = nb ? 1 : 0;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (nb) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    fcntl(sock, F_SETFL, flags);
#endif
}

static void closeSocket(int sock) {
    if (sock < 0) return;
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

GhostpadClient::GhostpadClient() {
    initSockets();
}

GhostpadClient::~GhostpadClient() {
    disconnect();
}

bool GhostpadClient::connect(const std::string& ip, int port, int timeout_ms) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (connected_) disconnect();

    sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ < 0) return false;

    setNonBlocking(static_cast<int>(sock_), true);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    int ret = ::connect(static_cast<int>(sock_), (struct sockaddr*)&addr, sizeof(addr));

    if (ret < 0) {
#ifdef _WIN32
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            closeSocket(static_cast<int>(sock_));
            sock_ = -1;
            return false;
        }
#else
        if (errno != EINPROGRESS) {
            closeSocket(static_cast<int>(sock_));
            sock_ = -1;
            return false;
        }
#endif
    }

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(static_cast<int>(sock_), &fdset);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    ret = select(static_cast<int>(sock_) + 1, nullptr, &fdset, nullptr, &tv);
    if (ret <= 0) {
        closeSocket(static_cast<int>(sock_));
        sock_ = -1;
        return false;
    }

    int so_error = 0;
    socklen_t len = sizeof(so_error);
    getsockopt(static_cast<int>(sock_), SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
    if (so_error != 0) {
        closeSocket(static_cast<int>(sock_));
        sock_ = -1;
        return false;
    }

    setNonBlocking(static_cast<int>(sock_), false);

    // Disable Nagle's algorithm
    int flag = 1;
    setsockopt(static_cast<int>(sock_), IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    connected_ = true;
    host_ = ip;
    port_ = port;
    return true;
}

void GhostpadClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) return;

    try {
        auto disc = buildDiscPacket();
        ::send(static_cast<int>(sock_), (const char*)disc.data(), disc.size(), 0);
    } catch (...) {}

    closeSocket(static_cast<int>(sock_));
    sock_ = -1;
    connected_ = false;
    host_.clear();
    port_ = GPAD_PORT;
}

bool GhostpadClient::sendPadState(const GpadNetworkState& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || sock_ < 0) return false;

    auto packet = buildGpadPacket(state);
    int sent = ::send(static_cast<int>(sock_), (const char*)packet.data(), packet.size(), 0);
    return sent == static_cast<int>(packet.size());
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

    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        result.error = "Socket creation failed";
        return result;
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(CTRL_PORT));
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        result.error = "Connection failed: " + std::string(strerror(errno));
        closeSocket(sock);
        return result;
    }

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    int sent = ::send(sock, (const char*)data, len, 0);
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

bool GhostpadClient::probeHostPort(const std::string& ip, int port, int timeout_ms) {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

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

    int ret = select(sock + 1, nullptr, &fdset, nullptr, &tv);

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

    std::vector<ProbeResult> hits;

    for (int i = 1; i <= 254; i++) {
        std::string host = base + "." + std::to_string(i);
        for (int port : PROBE_PORTS) {
            if (probeHostPort(host, port, timeout_ms)) {
                hits.push_back({host, port, true});
            }
        }
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
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0) return "192.168.1";

    std::string subnet = "192.168.1";
    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;

        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, buf, sizeof(buf));
        std::string addr(buf);
        auto pos = addr.rfind('.');
        if (pos != std::string::npos) {
            subnet = addr.substr(0, pos);
        }
    }

    freeifaddrs(ifaddr);
    return subnet;
}

} // namespace ghostpad
