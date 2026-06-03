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
#include <nlohmann/json.hpp>

namespace ghostpad {

struct AppSettings {
    std::string payload_elf_path;
    bool auto_deploy_on_connect = true;
    bool auto_bind_via_klog = true;
    bool connect_beep_enabled = false;
    int connect_beep_type = 1;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AppSettings, payload_elf_path, auto_deploy_on_connect,
                                   auto_bind_via_klog, connect_beep_enabled, connect_beep_type)
};

class SettingsStore {
public:
    explicit SettingsStore(const std::string& data_dir);
    AppSettings read() const;
    AppSettings write(const AppSettings& patch);
    std::string resolvePayloadPath() const;

private:
    std::string file_path_;
    std::string app_root_;
};

} // namespace ghostpad
