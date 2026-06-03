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
#include <vector>

namespace ghostpad {

struct NetworkInterface {
    std::string name;
    std::string address;
    bool internal = false;
};

class NetworkScanner {
public:
    static std::string getLocalSubnet();
    static std::vector<NetworkInterface> getInterfaces();

private:
    static std::string getSubnetFromAddress(const std::string& addr);
};

} // namespace ghostpad
