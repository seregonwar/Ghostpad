// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "storage/console_store.h"
#include <fstream>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace ghostpad {

static std::string generateUUID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    uint64_t a = dis(gen);
    uint64_t b = dis(gen);

    oss << std::setw(8) << ((a >> 32) & 0xFFFFFFFF) << "-"
        << std::setw(4) << ((a >> 16) & 0xFFFF) << "-"
        << std::setw(4) << (a & 0xFFFF) << "-"
        << std::setw(4) << ((b >> 48) & 0xFFFF) << "-"
        << std::setw(12) << (b & 0xFFFFFFFFFFFF);

    return oss.str();
}

static std::string nowISO() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

ConsoleStore::ConsoleStore(const std::string& data_dir) {
    std::filesystem::create_directories(data_dir);
    file_path_ = data_dir + "/ghostpad-consoles.json";
}

std::vector<ConsoleEntry> ConsoleStore::readAll() const {
    std::ifstream file(file_path_);
    if (!file.is_open()) return {};

    try {
        nlohmann::json j;
        file >> j;
        if (j.is_array()) {
            return j.get<std::vector<ConsoleEntry>>();
        }
    } catch (...) {}

    return {};
}

void ConsoleStore::writeAll(const std::vector<ConsoleEntry>& entries) const {
    nlohmann::json j = entries;
    std::ofstream file(file_path_);
    file << j.dump(2);
}

std::vector<ConsoleEntry> ConsoleStore::list() const {
    return readAll();
}

ConsoleEntry ConsoleStore::add(const std::string& name, const std::string& ip, int port, int elf_loader_port) {
    auto entries = readAll();
    ConsoleEntry entry;
    entry.id = generateUUID();
    entry.name = name.empty() ? ("PS5 (" + ip + ")") : name;
    entry.ip = ip;
    entry.port = port;
    entry.elf_loader_port = elf_loader_port;
    entry.created_at = nowISO();
    entry.updated_at = entry.created_at;
    entries.push_back(entry);
    writeAll(entries);
    return entry;
}

ConsoleEntry ConsoleStore::update(const std::string& id, const ConsoleEntry& patch) {
    auto entries = readAll();
    for (auto& e : entries) {
        if (e.id == id) {
            if (!patch.name.empty()) e.name = patch.name;
            if (!patch.ip.empty()) e.ip = patch.ip;
            if (patch.port > 0) e.port = patch.port;
            if (patch.elf_loader_port > 0) e.elf_loader_port = patch.elf_loader_port;
            e.updated_at = nowISO();
            writeAll(entries);
            return e;
        }
    }
    return {};
}

bool ConsoleStore::remove(const std::string& id) {
    auto entries = readAll();
    auto it = std::remove_if(entries.begin(), entries.end(),
        [&](const ConsoleEntry& e) { return e.id == id; });
    if (it != entries.end()) {
        entries.erase(it, entries.end());
        writeAll(entries);
        return true;
    }
    return false;
}

} // namespace ghostpad
