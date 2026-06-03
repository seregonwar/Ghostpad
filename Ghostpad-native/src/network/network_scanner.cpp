// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "network/network_scanner.h"
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <cstring>

namespace ghostpad {

std::string NetworkScanner::getSubnetFromAddress(const std::string& addr) {
    auto pos = addr.rfind('.');
    if (pos != std::string::npos) {
        return addr.substr(0, pos);
    }
    return "192.168.1";
}

std::string NetworkScanner::getLocalSubnet() {
    auto ifaces = getInterfaces();
    for (auto& iface : ifaces) {
        if (!iface.internal && !iface.address.empty() && iface.address != "127.0.0.1") {
            return getSubnetFromAddress(iface.address);
        }
    }
    return "192.168.1";
}

std::vector<NetworkInterface> NetworkScanner::getInterfaces() {
    std::vector<NetworkInterface> result;
    struct ifaddrs* ifaddr = nullptr;

    if (getifaddrs(&ifaddr) != 0) return result;

    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;

        NetworkInterface iface;
        iface.name = ifa->ifa_name ? ifa->ifa_name : "";
        iface.internal = (ifa->ifa_flags & IFF_LOOPBACK) != 0;

        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, buf, sizeof(buf));
        iface.address = buf;

        result.push_back(iface);
    }

    freeifaddrs(ifaddr);
    return result;
}

} // namespace ghostpad
