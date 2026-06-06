// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "storage/settings_store.h"
#include <fstream>
#include <filesystem>
#include <vector>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <limits.h>
    #include <stdlib.h>
#else
    #include <unistd.h>
#endif

namespace ghostpad {

static std::string getAppRoot() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, sizeof(path));
    std::filesystem::path p(path);
    return p.parent_path().string();
#elif defined(__APPLE__)
    char path[4096];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) return ".";
    char* resolved = realpath(path, nullptr);
    std::string result;
    if (resolved) {
        std::filesystem::path p(resolved);
        result = p.parent_path().parent_path().string();
        free(resolved);
    } else {
        std::filesystem::path p(path);
        result = p.parent_path().string();
    }
    return result;
#else
    char path[4096];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len == -1) return ".";
    path[len] = '\0';
    std::filesystem::path p(path);
    return p.parent_path().string();
#endif
}

SettingsStore::SettingsStore(const std::string& data_dir) {
    std::filesystem::create_directories(data_dir);
    file_path_ = data_dir + "/ghostpad-settings.json";

#ifdef __APPLE__
    // For macOS bundle, look in Resources/Ghostpad/payload
    char exe_path[4096];
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) == 0) {
        char* resolved = realpath(exe_path, nullptr);
        if (resolved) {
            std::filesystem::path p(resolved);
            app_root_ = p.parent_path().parent_path().string();
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
    if (loaded_) {
        return cache_;
    }

    std::ifstream file(file_path_);
    if (!file.is_open()) {
        std::vector<std::string> candidates = {
            app_root_ + "/ghostpad-settings.json",
            app_root_ + "/Resources/ghostpad-settings.json",
            app_root_ + "/../Resources/ghostpad-settings.json",
            app_root_ + "/../../ghostpad-settings.json"
        };
        for (const auto& path : candidates) {
            std::ifstream default_file(path);
            if (default_file.is_open()) {
                try {
                    nlohmann::json j;
                    default_file >> j;
                    cache_ = j.get<AppSettings>();
                    loaded_ = true;
                    return cache_;
                } catch (...) {}
            }
        }
        cache_ = AppSettings{};
        loaded_ = true;
        return cache_;
    }

    try {
        nlohmann::json j;
        file >> j;
        cache_ = j.get<AppSettings>();
        loaded_ = true;
        return cache_;
    } catch (...) {}

    cache_ = AppSettings{};
    loaded_ = true;
    return cache_;
}

AppSettings SettingsStore::write(const AppSettings& patch) {
    AppSettings current = read();

    if (!patch.payload_elf_path.empty()) current.payload_elf_path = patch.payload_elf_path;
    current.auto_deploy_on_connect = patch.auto_deploy_on_connect;
    current.auto_bind_via_klog = patch.auto_bind_via_klog;
    current.connect_beep_enabled = patch.connect_beep_enabled;
    current.connect_beep_type = patch.connect_beep_type;
    current.pad_layout = patch.pad_layout;
    if (!patch.active_profile_id.empty()) current.active_profile_id = patch.active_profile_id;
    current.ui_scale = patch.ui_scale;

    nlohmann::json j = current;
    std::ofstream file(file_path_);
    file << j.dump(2);

    cache_ = current;
    loaded_ = true;
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
        app_root_ + "/Ghostpad/payload/ghostpad-ps4.elf",
        app_root_ + "/Ghostpad/payload/ghostpad-ps5.elf",
        app_root_ + "/Ghostpad/payload/ghostpad.elf",
        app_root_ + "/../Ghostpad/payload/ghostpad-ps4.elf",
        app_root_ + "/../Ghostpad/payload/ghostpad-ps5.elf",
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
