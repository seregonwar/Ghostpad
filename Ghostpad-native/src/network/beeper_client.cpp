// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "network/beeper_client.h"
#include <cstring>
#include <fstream>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

namespace ghostpad {

static void closeSocket(int sock) {
    if (sock < 0) return;
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

BeeperResult BeeperClient::sendCommand(const std::string& ip, const std::string& cmd, int timeout_ms) {
    BeeperResult result;

    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        result.response = "ERR socket creation failed";
        return result;
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9111);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        result.response = "ERR connect: " + std::string(strerror(errno));
        closeSocket(sock);
        return result;
    }

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    std::string full_cmd = cmd + "\n";
    ::send(sock, full_cmd.c_str(), full_cmd.size(), 0);

    char buf[256] = {};
    int n = ::recv(sock, buf, sizeof(buf) - 1, 0);
    closeSocket(sock);

    if (n > 0) {
        std::string response(buf, n);
        auto nl = response.find('\n');
        if (nl != std::string::npos) {
            response = response.substr(0, nl);
        }
        // Strip trailing \r
        if (!response.empty() && response.back() == '\r') {
            response.pop_back();
        }
        result.ok = response.rfind("OK", 0) == 0;
        result.response = response;
    } else {
        result.response = "ERR no response";
    }

    return result;
}

BeeperResult BeeperClient::buzz(const std::string& ip, int type, int timeout_ms) {
    return sendCommand(ip, "BUZZ " + std::to_string(type), timeout_ms);
}

BeeperResult BeeperClient::setVolume(const std::string& ip, int level, int timeout_ms) {
    return sendCommand(ip, "VOL " + std::to_string(level), timeout_ms);
}

BeeperResult BeeperClient::setMute(const std::string& ip, int mute, int timeout_ms) {
    return sendCommand(ip, "MUTE " + std::to_string(mute), timeout_ms);
}

BeeperResult BeeperClient::setLed(const std::string& ip, int level, int timeout_ms) {
    return sendCommand(ip, "LED " + std::to_string(level), timeout_ms);
}

BeeperResult BeeperClient::ping(const std::string& ip, int timeout_ms) {
    return buzz(ip, 1, timeout_ms);
}

BeeperResult BeeperClient::deployElf(const std::string& ip, const std::string& elf_path, int elf_loader_port) {
    BeeperResult result;

    std::ifstream file(elf_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        result.response = "Cannot read ELF: " + elf_path;
        return result;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> data(size);
    if (!file.read(data.data(), size)) {
        result.response = "Cannot read ELF data";
        return result;
    }

    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        result.response = "Socket creation failed";
        return result;
    }

    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(elf_loader_port));
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        result.response = "Connect failed: " + std::string(strerror(errno));
        closeSocket(sock);
        return result;
    }

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    ::send(sock, data.data(), data.size(), 0);
    closeSocket(sock);

    result.ok = true;
    result.response = "Deployed " + std::to_string(size) + " bytes";
    return result;
}

} // namespace ghostpad
