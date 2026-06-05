#pragma once

// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    using socklen_t = int;
    using ssize_t = int;
    #define INVALID_SOCKET_VAL INVALID_SOCKET
    #define SOCKET_ERROR_VAL SOCKET_ERROR
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/select.h>
    #include <errno.h>
    using socket_t = int;
    #define INVALID_SOCKET_VAL (-1)
    #define SOCKET_ERROR_VAL (-1)
#endif

#include <string>
#include <cstring>

namespace ghostpad {

inline std::string getLastSocketErrorString() {
#ifdef _WIN32
    int err = ::WSAGetLastError();
    char* s = nullptr;
    ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&s, 0, nullptr
    );
    std::string msg = s ? s : "Unknown socket error";
    if (s) {
        ::LocalFree(s);
    }
    while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n' || msg.back() == ' ')) {
        msg.pop_back();
    }
    return msg + " (code: " + std::to_string(err) + ")";
#else
    return std::strerror(errno);
#endif
}

inline void closeSocket(socket_t sock) {
#ifdef _WIN32
    if (sock != INVALID_SOCKET) {
        ::closesocket(sock);
    }
#else
    if (sock >= 0) {
        ::close(sock);
    }
#endif
}

inline void setNonBlocking(socket_t sock, bool nb) {
#ifdef _WIN32
    u_long mode = nb ? 1 : 0;
    ::ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = ::fcntl(sock, F_GETFL, 0);
    if (nb) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    ::fcntl(sock, F_SETFL, flags);
#endif
}

inline void setNoSigPipe(socket_t sock) {
    (void)sock;
#ifdef __APPLE__
    int opt = 1;
    ::setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif
}

inline void setSocketTimeout(socket_t sock, int optname, int timeout_ms) {
#ifdef _WIN32
    DWORD timeout = static_cast<DWORD>(timeout_ms);
    ::setsockopt(sock, SOL_SOCKET, optname, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    ::setsockopt(sock, SOL_SOCKET, optname, reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif
}

} // namespace ghostpad
