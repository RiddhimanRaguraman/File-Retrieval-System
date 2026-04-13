#include "Protocol.hpp"
#include <sys/types.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#include <cstring>
#include <iostream>

bool readNBytes(int socket, void* buffer, size_t n) {
    size_t totalRead = 0;
    char* ptr = static_cast<char*>(buffer);
    while (totalRead < n) {
#ifdef _WIN32
        int bytesRead = recv(socket, ptr + totalRead, n - totalRead, 0);
#else
        int bytesRead = read(socket, ptr + totalRead, n - totalRead);
#endif
        if (bytesRead <= 0) {
            return false;
        }
        totalRead += bytesRead;
    }
    return true;
}

bool sendNBytes(int socket, const void* buffer, size_t n) {
    size_t totalSent = 0;
    const char* ptr = static_cast<const char*>(buffer);
    while (totalSent < n) {
#ifdef _WIN32
        int bytesSent = send(socket, ptr + totalSent, n - totalSent, 0);
#else
        int bytesSent = write(socket, ptr + totalSent, n - totalSent);
#endif
        if (bytesSent <= 0) {
            return false;
        }
        totalSent += bytesSent;
    }
    return true;
}

bool sendUint32(int socket, uint32_t val) {
    uint32_t netVal = htonl(val);
    return sendNBytes(socket, &netVal, sizeof(netVal));
}

bool readUint32(int socket, uint32_t& val) {
    uint32_t netVal;
    if (!readNBytes(socket, &netVal, sizeof(netVal))) {
        return false;
    }
    val = ntohl(netVal);
    return true;
}

bool sendString(int socket, const std::string& str) {
    if (!sendUint32(socket, static_cast<uint32_t>(str.length()))) {
        return false;
    }
    if (str.length() > 0) {
        return sendNBytes(socket, str.data(), str.length());
    }
    return true;
}

bool readString(int socket, std::string& str) {
    uint32_t len;
    if (!readUint32(socket, len)) {
        return false;
    }
    if (len == 0) {
        str = "";
        return true;
    }
    std::vector<char> buffer(len);
    if (!readNBytes(socket, buffer.data(), len)) {
        return false;
    }
    str.assign(buffer.data(), len);
    return true;
}
