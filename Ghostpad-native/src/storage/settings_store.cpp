// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "storage/settings_store.h"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <cstring>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace ghostpad {

static std::string getAppRoot() {
    char path[4096];
#ifdef __APPLE__
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) return ".";
    char* resolved = realpath(path, nullptr);
    std::string result;
    if (resolved) {
        char* dir = dirname(resolved);
        result = std::string(dir) + "/..";
        free(resolved);
    } else {
        result = dirname(path);
    }
#else
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len == -1) return ".";
    path[len] = '\0';
    char* dir = dirname(path);
    std::string result(dir);
#endif
    return result;
}

SettingsStore::SettingsStore(const std::string& data_dir) {
    mkdir(data_dir.c_str(), 0755);
    file_path_ = data_dir + "/ghostpad-settings.json";

#ifdef __APPLE__
    // For macOS bundle, look in Resources/Ghostpad/payload
    char exe_path[4096];
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) == 0) {
        char* resolved = realpath(exe_path, nullptr);
        if (resolved) {
            char* dir = dirname(resolved);
            // Navigate from MacOS/ to Resources/
            std::string resources = std::string(dir) + "/../Resources/Ghostpad/payload/ghostpad.elf";
            app_root_ = std::string(dir) + "/..";
            free(resolved);
        }
    }
    if (app_root_.empty()) {
        app_root_ = getAppRoot();
    }
#else
    app_root_ = getAppRoot();
#endif
}

AppSettings SettingsStore::read() const {
    std::ifstream file(file_path_);
    if (!file.is_open()) {
        return AppSettings{};
    }

    try {
        nlohmann::json j;
        file >> j;
        return j.get<AppSettings>();
    } catch (...) {}

    return AppSettings{};
}

AppSettings SettingsStore::write(const AppSettings& patch) {
    // Merge with existing
    AppSettings current = read();

    if (!patch.payload_elf_path.empty()) current.payload_elf_path = patch.payload_elf_path;
    current.auto_deploy_on_connect = patch.auto_deploy_on_connect;
    current.auto_bind_via_klog = patch.auto_bind_via_klog;
    current.connect_beep_enabled = patch.connect_beep_enabled;
    current.connect_beep_type = patch.connect_beep_type;
    current.pad_layout = patch.pad_layout;

    nlohmann::json j = current;
    std::ofstream file(file_path_);
    file << j.dump(2);
    return current;
}

std::string SettingsStore::resolvePayloadPath() const {
    auto settings = read();

    // Check user-configured path
    if (!settings.payload_elf_path.empty()) {
        std::ifstream test(settings.payload_elf_path);
        if (test.good()) return settings.payload_elf_path;
    }

    // Check default locations
    std::vector<std::string> candidates = {
        app_root_ + "/Ghostpad/payload/ghostpad.elf",
        app_root_ + "/../Ghostpad/payload/ghostpad.elf",
        app_root_ + "/Ghostpad/payloadExamples/ghostpadOGpartial/payload/ghostpad.elf",
    };

    for (auto& candidate : candidates) {
        std::ifstream test(candidate);
        if (test.good()) return candidate;
    }

    return settings.payload_elf_path;
}

} // namespace ghostpad
