// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "network/payload_deploy.h"
#include "protocol/gpad_packet.h"
#include <cstring>
#include <fstream>
#include <regex>
#include <thread>
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

static void setNoSigPipe(int sock) {
#ifdef __APPLE__
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#elif !defined(_WIN32)
    (void)sock;
#endif
}

PayloadDeployer::PayloadDeployer() {
    status_.phase = "idle";
    status_.message = "Ready";
}

PayloadDeployer::~PayloadDeployer() {
    stopKlogWatcher();
}

void PayloadDeployer::emitStatus(const std::string& phase, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_.phase = phase;
    status_.message = message;
    status_.at = std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    if (status_callback_) {
        status_callback_(status_);
    }
}

DeployStatus PayloadDeployer::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

bool PayloadDeployer::portOpen(const std::string& host, int port, int timeout_ms) {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    bool result = (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    closeSocket(sock);
    return result;
}

bool PayloadDeployer::waitPort(const std::string& host, int port, int deadline_ms) {
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(deadline_ms);
    while (std::chrono::steady_clock::now() < end) {
        if (portOpen(host, port, 1000)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return false;
}

bool PayloadDeployer::deployElf(const std::string& host, const std::string& elf_path, int elf_loader_port) {
    std::ifstream file(elf_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> data(size);
    if (!file.read(data.data(), size)) return false;

    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    setNoSigPipe(sock);

    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(elf_loader_port));
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        closeSocket(sock);
        return false;
    }

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    ssize_t sent = ::send(sock, data.data(), data.size(), 0);
    closeSocket(sock);
    return sent == static_cast<ssize_t>(data.size());
}

bool PayloadDeployer::sendGbnd(const std::string& host, uint64_t virt, uint64_t phys, uint32_t user) {
    auto packet = buildGbndPacket(user, virt, phys);

    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    setNoSigPipe(sock);

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6970);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        closeSocket(sock);
        return false;
    }

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    ::send(sock, (const char*)packet.data(), packet.size(), 0);
    closeSocket(sock);
    return true;
}

void PayloadDeployer::processKlogLine(const std::string& line) {
    fprintf(stderr, "[klog] %s\n", line.c_str());
    fflush(stderr);
    try {
    // Match physical device
    static std::regex re_dev_phys(
        R"(DEVICE_ADDED|DeviceAdded.*?(?:DeviceId|DeviceID|deviceId|deviceID|device id)[:=]\s*0x([0-9a-fA-F]+))",
        std::regex::icase
    );
    static std::regex re_dev_virt(
        R"(DEVICE_ADDED|DeviceAdded|GetUnassignedDeviceInfo.*?(?:DeviceId|DeviceID|deviceId|deviceID|device id)[:=]\s*0x([0-9a-fA-F]+))",
        std::regex::icase
    );
    static std::regex re_owner(
        R"(DEVICE_OWNER_CHANGED.*?deviceId=0x([0-9a-fA-F]+).*?userId=0x([0-9a-fA-F]+))",
        std::regex::icase
    );
    static std::regex re_adopted(R"(VDI active|padHandle)", std::regex::icase);
    static std::regex re_bind_ok(R"(force_bind.*ret=0|MBusBindDeviceWithUserId.*ret=0)", std::regex::icase);

    std::smatch match;

    if (std::regex_search(line, match, re_dev_phys) && match.size() > 1) {
        uint64_t dev = 0;
        try { dev = std::stoull(match[1].str(), nullptr, 16); } catch (...) {}
        if (dev > 0 && (line.find("capabilityBattery:1") != std::string::npos ||
                         line.find("Physical") != std::string::npos)) {
            auto_phys = dev;
            emitStatus("klog", "Physical controller: 0x" + match[1].str());
        }
    }

    if (!auto_sent_gbnd_ && std::regex_search(line, match, re_dev_virt) && match.size() > 1) {
        uint64_t dev = 0;
        try { dev = std::stoull(match[1].str(), nullptr, 16); } catch (...) {}
        bool is_virtual = line.find("capabilityBattery:0") != std::string::npos ||
                          line.find("userId=0xffffffff") != std::string::npos ||
                          line.find("UserId:0xffffffff") != std::string::npos ||
                          line.find("remoteplay") != std::string::npos ||
                          line.find("RemotePlay") != std::string::npos ||
                          line.find("type:4") != std::string::npos ||
                          line.find("VDA candidate") != std::string::npos;
        if (is_virtual && dev > 0) {
            auto_virt = dev;
            auto_sent_gbnd_ = true;
            emitStatus("klog", "Virtual device 0x" + match[1].str() + " detected - sending GBND");

            std::thread([this, dev]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
                sendGbnd(klog_host_, auto_virt, auto_phys, 0);
            }).detach();
        }
    }

    if (std::regex_search(line, match, re_owner)) {
        auto_bound = true;
        emitStatus("klog", "Controller bound: dev=0x" + match[1].str());
    }

    if (std::regex_search(line, re_adopted)) {
        auto_adopted = true;
        emitStatus("klog", "VDI active - GPAD input should work");
    }

    if (std::regex_search(line, re_bind_ok)) {
        auto_bound = true;
        emitStatus("klog", "MBus bind OK");
    }
    } catch (...) {}
}

void PayloadDeployer::processKlogData(const std::string& host, int sock) {
    static std::string buffer;
    char buf[4096];

    while (klog_running_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 500000;

        int ret = select(sock + 1, &fds, nullptr, nullptr, &tv);
        if (ret < 0) break;
        if (ret == 0) continue;

        ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;

        buffer.append(buf, n);
        size_t idx;
        while ((idx = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, idx);
            buffer = buffer.substr(idx + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            processKlogLine(line);
        }
    }
}

void PayloadDeployer::stopKlogWatcher() {
    if (klog_running_) {
        klog_running_ = false;
        if (klog_sock_ >= 0) {
            closeSocket(klog_sock_);
            klog_sock_ = -1;
        }
        if (klog_thread_.joinable()) {
            klog_thread_.join();
        }
    }
}

PayloadDeployer::Result PayloadDeployer::ensurePayloadRunning(const std::string& host, const Options& options) {
    Result result;
    int elf_port = options.elf_loader_port;

    status_.host = host;
    status_callback_ = options.status_callback;

    stopKlogWatcher();
    auto_sent_gbnd_ = false;
    auto_phys = 0;
    auto_virt = 0;
    auto_adopted = false;
    auto_bound = false;

    // Start Klog watcher if auto-bind enabled
    if (options.auto_bind_via_klog) {
        klog_sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (klog_sock_ >= 0) {
            struct sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(KLOG_PORT);
            inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

            if (::connect(klog_sock_, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                klog_host_ = host;
                klog_running_ = true;
                klog_thread_ = std::thread(&PayloadDeployer::processKlogData, this, host, static_cast<int>(klog_sock_));
                emitStatus("klog", "Klog watcher started");
            } else {
                closeSocket(klog_sock_);
                klog_sock_ = -1;
                emitStatus("klog", "Klog port unavailable - continuing without auto-bind");
            }
        }
    }

    bool gp_already = portOpen(host, GPAD_PORT, 1000);

    if (options.force_deploy || !gp_already) {
        if (!portOpen(host, elf_port, 1000)) {
            emitStatus("warn", "ELF loader port " + std::to_string(elf_port) +
                       " is closed - skipping deploy (payload may already be running)");

            if (klog_running_) {
                emitStatus("waiting", "Waiting for direct-VDI adopt (auto-bind)...");
                auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(12000);
                while (std::chrono::steady_clock::now() < end && !auto_adopted) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(150));
                }
            }

            emitStatus("ready", "Ghostpad ready on " + host + ":" + std::to_string(GPAD_PORT));
            result.ok = true;
            result.skipped = true;
            result.message = "ELF loader port " + std::to_string(elf_port) + " closed - deploy skipped";
            result.adopted = auto_adopted;
            result.bound = auto_bound;
            return result;
        }

        if (options.elf_path.empty()) {
            stopKlogWatcher();
            emitStatus("error", "No payload ELF configured. Choose ghostpad.elf in Settings.");
            result.message = status_.message;
            return result;
        }

        emitStatus("deploying", "Deploying payload to " + host + ":" + std::to_string(elf_port) + "...");
        if (!deployElf(host, options.elf_path, elf_port)) {
            stopKlogWatcher();
            emitStatus("error", "ELF deployment failed");
            result.message = "ELF deployment failed";
            return result;
        }

        emitStatus("waiting", "Payload sent - waiting for GPAD port...");
        if (!waitPort(host, GPAD_PORT, 30000)) {
            stopKlogWatcher();
            emitStatus("error", "Payload deployed but GPAD port 6967 never opened");
            result.message = "Payload deployed but GPAD port 6967 never opened";
            return result;
        }

        if (klog_running_) {
            emitStatus("waiting", "Waiting for direct-VDI adopt (auto-bind)...");
            auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(12000);
            while (std::chrono::steady_clock::now() < end && !auto_adopted) {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
        }
    } else {
        emitStatus("ready", "Payload already running on " + host + " - reusing existing session");
    }

    emitStatus("ready", "Ghostpad ready on " + host + ":" + std::to_string(GPAD_PORT));
    result.ok = true;
    result.message = status_.message;
    result.adopted = auto_adopted;
    result.bound = auto_bound;
    return result;
}

} // namespace ghostpad
