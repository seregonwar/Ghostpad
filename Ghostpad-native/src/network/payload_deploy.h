#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>

#include "network/socket_util.h"

namespace ghostpad {

struct DeployStatus {
    std::string phase;    // idle, deploying, waiting, klog, ready, error, warn
    std::string message;
    std::string host;
    std::string at;
};

using DeployStatusCallback = std::function<void(const DeployStatus&)>;

class PayloadDeployer {
public:
    PayloadDeployer();
    ~PayloadDeployer();

    struct Options {
        std::string elf_path;
        bool force_deploy = false;
        bool auto_bind_via_klog = true;
        int elf_loader_port = 9021;
        DeployStatusCallback status_callback;
    };

    struct Result {
        bool ok = false;
        std::string message;
        bool skipped = false;
        bool adopted = false;
        bool bound = false;
    };

    Result ensurePayloadRunning(const std::string& host, const Options& options);
    static bool deployElf(const std::string& host, const std::string& elf_path, int elf_loader_port = 9021);
    void stopKlogWatcher();
    DeployStatus getStatus() const;

    // Klog watcher callbacks
    bool auto_adopted = false;
    bool auto_bound = false;
    uint64_t auto_phys = 0;
    uint64_t auto_virt = 0;

private:
    static bool portOpen(const std::string& host, int port, int timeout_ms = 1000);
    static bool waitPort(const std::string& host, int port, int deadline_ms);
    static bool sendGbnd(const std::string& host, uint64_t virt, uint64_t phys, uint32_t user = 0);

    void processKlogLine(const std::string& line);
    void processKlogData(const std::string& host, socket_t sock);

    DeployStatus status_;
    mutable std::mutex mutex_;
    std::thread klog_thread_;
    std::atomic<bool> klog_running_{false};
    socket_t klog_sock_ = INVALID_SOCKET_VAL;
    std::string klog_host_;
    DeployStatusCallback status_callback_;
    bool auto_sent_gbnd_ = false;

    void emitStatus(const std::string& phase, const std::string& message);
};

} // namespace ghostpad
