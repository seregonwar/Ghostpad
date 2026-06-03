#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregowar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.


// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <string>

namespace ghostpad {

struct BeeperResult {
    bool ok = false;
    std::string response;
};

class BeeperClient {
public:
    static BeeperResult buzz(const std::string& ip, int type = 1, int timeout_ms = 3000);
    static BeeperResult setVolume(const std::string& ip, int level, int timeout_ms = 3000);
    static BeeperResult setMute(const std::string& ip, int mute, int timeout_ms = 3000);
    static BeeperResult setLed(const std::string& ip, int level, int timeout_ms = 3000);
    static BeeperResult ping(const std::string& ip, int timeout_ms = 3000);
    static BeeperResult deployElf(const std::string& ip, const std::string& elf_path, int elf_loader_port = 9021);

private:
    static BeeperResult sendCommand(const std::string& ip, const std::string& cmd, int timeout_ms);
};

} // namespace ghostpad
