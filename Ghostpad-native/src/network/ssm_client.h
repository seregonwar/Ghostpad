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

struct SsmResult {
    bool ok = false;
    std::string response;
};

class SsmClient {
public:
    static SsmResult status(const std::string& ip, int timeout_ms = 5000);
    static SsmResult reboot(const std::string& ip, int timeout_ms = 8000);
    static SsmResult shutdown(const std::string& ip, int timeout_ms = 8000);
    static SsmResult restMode(const std::string& ip, int timeout_ms = 8000);
    static SsmResult ejectDisc(const std::string& ip, int timeout_ms = 5000);
    static SsmResult deployElf(const std::string& ip, const std::string& elf_path, int elf_loader_port = 9021);

private:
    static SsmResult sendCommand(const std::string& ip, const std::string& cmd, int timeout_ms);
};

} // namespace ghostpad
