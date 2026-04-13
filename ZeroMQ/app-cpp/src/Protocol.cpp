#include "Protocol.hpp"
#include <array>
#include <cstring>
#include <limits>
#include <zmq.h>

void ByteWriter::writeU32(std::uint32_t v) {
    std::array<std::uint8_t, 4> bytes = {
        static_cast<std::uint8_t>((v >> 24) & 0xFF),
        static_cast<std::uint8_t>((v >> 16) & 0xFF),
        static_cast<std::uint8_t>((v >> 8) & 0xFF),
        static_cast<std::uint8_t>(v & 0xFF),
    };
    buffer.insert(buffer.end(), bytes.begin(), bytes.end());
}

void ByteWriter::writeString(const std::string& s) {
    if (s.size() > std::numeric_limits<std::uint32_t>::max()) {
        writeU32(0);
        return;
    }
    writeU32(static_cast<std::uint32_t>(s.size()));
    buffer.insert(buffer.end(),
                  reinterpret_cast<const std::uint8_t*>(s.data()),
                  reinterpret_cast<const std::uint8_t*>(s.data()) + s.size());
}

const std::vector<std::uint8_t>& ByteWriter::bytes() const {
    return buffer;
}

std::vector<std::uint8_t> ByteWriter::takeBytes() {
    return std::move(buffer);
}

ByteReader::ByteReader(std::span<const std::uint8_t> data) : data(data) {}

bool ByteReader::readU32(std::uint32_t& out) {
    if (offset + 4 > data.size()) {
        return false;
    }
    std::uint32_t be = (static_cast<std::uint32_t>(data[offset]) << 24) |
                       (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
                       (static_cast<std::uint32_t>(data[offset + 2]) << 8) |
                       (static_cast<std::uint32_t>(data[offset + 3]));
    offset += 4;
    out = be;
    return true;
}

bool ByteReader::readString(std::string& out) {
    std::uint32_t len = 0;
    if (!readU32(len)) {
        return false;
    }
    if (offset + len > data.size()) {
        return false;
    }
    out.assign(reinterpret_cast<const char*>(data.data() + offset), len);
    offset += len;
    return true;
}

bool ByteReader::eof() const {
    return offset >= data.size();
}

bool zmqRecvMultipart(void* socket, std::vector<std::vector<std::uint8_t>>& frames) {
    frames.clear();
    while (true) {
        zmq_msg_t msg;
        if (zmq_msg_init(&msg) != 0) {
            return false;
        }
        int rc = zmq_msg_recv(&msg, socket, 0);
        if (rc < 0) {
            zmq_msg_close(&msg);
            return false;
        }

        const std::size_t size = zmq_msg_size(&msg);
        std::vector<std::uint8_t> frame(size);
        if (size > 0) {
            std::memcpy(frame.data(), zmq_msg_data(&msg), size);
        }
        const bool more = zmq_msg_more(&msg) != 0;
        zmq_msg_close(&msg);

        frames.push_back(std::move(frame));
        if (!more) {
            return true;
        }
    }
}

bool zmqSendMultipart(void* socket, const std::vector<std::vector<std::uint8_t>>& frames) {
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const int flags = (i + 1 < frames.size()) ? ZMQ_SNDMORE : 0;
        const auto& frame = frames[i];
        zmq_msg_t msg;
        if (zmq_msg_init_size(&msg, frame.size()) != 0) {
            return false;
        }
        if (!frame.empty()) {
            std::memcpy(zmq_msg_data(&msg), frame.data(), frame.size());
        }
        const int rc = zmq_msg_send(&msg, socket, flags);
        zmq_msg_close(&msg);
        if (rc < 0) {
            return false;
        }
    }
    return true;
}

void writeMessageType(ByteWriter& w, MessageType type) {
    w.writeU32(static_cast<std::uint32_t>(type));
}

bool readMessageType(ByteReader& r, MessageType& type) {
    std::uint32_t v = 0;
    if (!r.readU32(v)) {
        return false;
    }
    type = static_cast<MessageType>(v);
    return true;
}

std::vector<std::uint8_t> encodeRegisterRequest() {
    ByteWriter w;
    writeMessageType(w, MessageType::REGISTER_REQUEST);
    return w.takeBytes();
}

bool decodeRegisterReply(std::span<const std::uint8_t> payload, std::uint32_t& clientId) {
    ByteReader r(payload);
    MessageType type;
    std::uint32_t id = 0;
    if (!readMessageType(r, type) || type != MessageType::REGISTER_REPLY || !r.readU32(id) || id == 0) {
        return false;
    }
    clientId = id;
    return true;
}

std::vector<std::uint8_t> encodeIndexRequest(std::uint32_t clientId,
                                             const std::string& documentPath,
                                             const std::vector<TermFrequency>& terms) {
    ByteWriter w;
    writeMessageType(w, MessageType::INDEX_REQUEST);
    w.writeU32(clientId);
    w.writeString(documentPath);
    w.writeU32(static_cast<std::uint32_t>(terms.size()));
    for (const auto& tf : terms) {
        w.writeString(tf.term);
        w.writeU32(tf.frequency);
    }
    return w.takeBytes();
}

bool decodeIndexReply(std::span<const std::uint8_t> payload, std::uint32_t& status) {
    ByteReader r(payload);
    MessageType type;
    std::uint32_t s = 0;
    if (!readMessageType(r, type) || type != MessageType::INDEX_REPLY || !r.readU32(s)) {
        return false;
    }
    status = s;
    return true;
}

std::vector<std::uint8_t> encodeSearchRequest(const std::vector<std::string>& terms) {
    ByteWriter w;
    writeMessageType(w, MessageType::SEARCH_REQUEST);
    w.writeU32(static_cast<std::uint32_t>(terms.size()));
    for (const auto& t : terms) {
        w.writeString(t);
    }
    return w.takeBytes();
}

bool decodeSearchReply(std::span<const std::uint8_t> payload,
                       std::uint32_t& totalHits,
                       std::vector<SearchResultEntry>& topResults) {
    ByteReader r(payload);
    MessageType type;
    std::uint32_t hits = 0;
    std::uint32_t count = 0;
    if (!readMessageType(r, type) || type != MessageType::SEARCH_REPLY || !r.readU32(hits) || !r.readU32(count)) {
        return false;
    }
    totalHits = hits;
    topResults.clear();
    topResults.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        SearchResultEntry e;
        if (!r.readString(e.documentPath) || !r.readU32(e.combinedFrequency)) {
            return false;
        }
        topResults.push_back(std::move(e));
    }
    return true;
}
