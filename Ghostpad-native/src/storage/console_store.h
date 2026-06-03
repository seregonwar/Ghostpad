#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2024  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ghostpad {

struct ConsoleEntry {
    std::string id;
    std::string name;
    std::string ip;
    int port = 6967;
    int elf_loader_port = 9021;
    std::string created_at;
    std::string updated_at;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConsoleEntry, id, name, ip, port, elf_loader_port, created_at, updated_at)
};

class ConsoleStore {
public:
    explicit ConsoleStore(const std::string& data_dir);
    std::vector<ConsoleEntry> list() const;
    ConsoleEntry add(const std::string& name, const std::string& ip, int port = 6967, int elf_loader_port = 9021);
    ConsoleEntry update(const std::string& id, const ConsoleEntry& patch);
    bool remove(const std::string& id);

private:
    std::string file_path_;
    std::vector<ConsoleEntry> readAll() const;
    void writeAll(const std::vector<ConsoleEntry>& entries) const;
};

} // namespace ghostpad
