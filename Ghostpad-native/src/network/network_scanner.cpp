// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "network/network_scanner.h"
#include <cstring>
#include <vector>
#include <string>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
#else
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <arpa/inet.h>
#endif

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

#ifdef _WIN32
    ULONG outBufLen = 15000;
    PIP_ADAPTER_ADDRESSES addresses = nullptr;
    ULONG iterations = 0;
    DWORD dwRetVal = 0;

    do {
        addresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
        if (addresses == nullptr) return result;
        dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, addresses, &outBufLen);
        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            free(addresses);
            addresses = nullptr;
        } else {
            break;
        }
        iterations++;
    } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (iterations < 3));

    if (dwRetVal == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES currAddresses = addresses; currAddresses != nullptr; currAddresses = currAddresses->Next) {
            bool internal = (currAddresses->IfType == IF_TYPE_SOFTWARE_LOOPBACK);
            for (PIP_ADAPTER_UNICAST_ADDRESS unicast = currAddresses->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
                if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
                    sockaddr_in* sa_in = (sockaddr_in*)unicast->Address.lpSockaddr;
                    char buf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(sa_in->sin_addr), buf, sizeof(buf));
                    
                    NetworkInterface iface;
                    std::wstring wname(currAddresses->FriendlyName);
                    iface.name = std::string(wname.begin(), wname.end());
                    iface.internal = internal;
                    iface.address = buf;
                    result.push_back(iface);
                }
            }
        }
    }
    if (addresses) free(addresses);
#else
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
#endif

    return result;
}

} // namespace ghostpad
