#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <span>
#include <utility>

enum class MessageType : uint32_t {
    REGISTER_REQUEST = 1,
    REGISTER_REPLY = 2,
    INDEX_REQUEST = 3,
    INDEX_REPLY = 4,
    SEARCH_REQUEST = 5,
    SEARCH_REPLY = 6,
    QUIT = 7
};

struct TermFrequency {
    std::string term;
    std::uint32_t frequency = 0;
};

struct SearchResultEntry {
    std::string documentPath;
    std::uint32_t combinedFrequency = 0;
};

class ByteWriter {
    std::vector<std::uint8_t> buffer;

public:
    void writeU32(std::uint32_t v);
    void writeString(const std::string& s);
    const std::vector<std::uint8_t>& bytes() const;
    std::vector<std::uint8_t> takeBytes();
};

class ByteReader {
    std::span<const std::uint8_t> data;
    std::size_t offset = 0;

public:
    explicit ByteReader(std::span<const std::uint8_t> data);
    bool readU32(std::uint32_t& out);
    bool readString(std::string& out);
    bool eof() const;
};

bool zmqRecvMultipart(void* socket, std::vector<std::vector<std::uint8_t>>& frames);
bool zmqSendMultipart(void* socket, const std::vector<std::vector<std::uint8_t>>& frames);

void writeMessageType(ByteWriter& w, MessageType type);
bool readMessageType(ByteReader& r, MessageType& type);

std::vector<std::uint8_t> encodeRegisterRequest();
bool decodeRegisterReply(std::span<const std::uint8_t> payload, std::uint32_t& clientId);

std::vector<std::uint8_t> encodeIndexRequest(std::uint32_t clientId,
                                             const std::string& documentPath,
                                             const std::vector<TermFrequency>& terms);
bool decodeIndexReply(std::span<const std::uint8_t> payload, std::uint32_t& status);

std::vector<std::uint8_t> encodeSearchRequest(const std::vector<std::string>& terms);
bool decodeSearchReply(std::span<const std::uint8_t> payload,
                       std::uint32_t& totalHits,
                       std::vector<SearchResultEntry>& topResults);

#endif
