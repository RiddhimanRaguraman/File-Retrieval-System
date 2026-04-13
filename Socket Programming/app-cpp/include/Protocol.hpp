#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>
#include <vector>
#include <string>

enum class MessageType : uint32_t {
    REGISTER_REQUEST = 1,
    REGISTER_REPLY = 2,
    INDEX_REQUEST = 3,
    INDEX_REPLY = 4,
    SEARCH_REQUEST = 5,
    SEARCH_REPLY = 6,
    QUIT = 7
};

// Helper function to read exactly n bytes from a socket
bool readNBytes(int socket, void* buffer, size_t n);

// Helper function to send exactly n bytes to a socket
bool sendNBytes(int socket, const void* buffer, size_t n);

// Send a string (length prefixed)
bool sendString(int socket, const std::string& str);

// Read a string (length prefixed)
bool readString(int socket, std::string& str);

// Send a uint32_t (network byte order)
bool sendUint32(int socket, uint32_t val);

// Read a uint32_t (network byte order)
bool readUint32(int socket, uint32_t& val);

#endif
