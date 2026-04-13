#include "ClientProcessingEngine.hpp"
#include "Protocol.hpp"

#include <iostream>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include <unordered_map>
#include <regex>
#include <algorithm> // For std::sort if needed
#include <cctype> // For isalnum

#include <string_view>
#include <vector>
#include <zmq.h>

namespace fs = std::filesystem;

ClientProcessingEngine::ClientProcessingEngine() : clientId(0), connected(false) {
}

ClientProcessingEngine::~ClientProcessingEngine() {
    disconnect();
}

IndexResult ClientProcessingEngine::indexFolder(std::string folderPath) {
    IndexResult result = {0.0, 0};
    auto start = std::chrono::high_resolution_clock::now();

    if (!connected) {
        std::cerr << "Not connected to server!" << std::endl;
        return result;
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                
                // Read file content
                std::ifstream file(filePath);
                if (!file.is_open()) continue;

                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string content = buffer.str();
                result.totalBytesRead += content.length();

                // Tokenize and count
                std::unordered_map<std::string, int> wordCounts;
                std::string word;
                for (char c : content) {
                    if (isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
                        word += c;
                    } else {
                        if (word.length() > 3) {
                            wordCounts[word]++;
                        }
                        word.clear();
                    }
                }
                if (word.length() > 3) {
                    wordCounts[word]++;
                }

                if (wordCounts.empty()) continue;

                std::vector<TermFrequency> terms;
                terms.reserve(wordCounts.size());
                for (const auto& pair : wordCounts) {
                    TermFrequency tf;
                    tf.term = pair.first;
                    tf.frequency = static_cast<std::uint32_t>(pair.second);
                    terms.push_back(std::move(tf));
                }

                std::vector<std::vector<std::uint8_t>> requestFrames;
                requestFrames.push_back(encodeIndexRequest(clientId, filePath, terms));
                if (!zmqSendMultipart(reqSocket, requestFrames)) {
                    break;
                }

                std::vector<std::vector<std::uint8_t>> replyFrames;
                if (!zmqRecvMultipart(reqSocket, replyFrames) || replyFrames.empty()) {
                    break;
                }

                std::uint32_t status = 0;
                if (!decodeIndexReply(std::span<const std::uint8_t>(replyFrames[0].data(), replyFrames[0].size()),
                                      status)) {
                    std::cerr << "Unexpected response to INDEX_REQUEST" << std::endl;
                    break;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error indexing: " << e.what() << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    result.executionTime = elapsed.count();

    return result;
}

SearchResult ClientProcessingEngine::search(std::vector<std::string> terms) {
    SearchResult result = {0.0, 0, { }};
    auto start = std::chrono::high_resolution_clock::now();

    if (!connected) {
        std::cerr << "Not connected to server!" << std::endl;
        return result;
    }

    std::vector<std::vector<std::uint8_t>> requestFrames;
    requestFrames.push_back(encodeSearchRequest(terms));
    if (!zmqSendMultipart(reqSocket, requestFrames)) {
        std::cerr << "Error sending search request" << std::endl;
        return result;
    }

    std::vector<std::vector<std::uint8_t>> replyFrames;
    if (!zmqRecvMultipart(reqSocket, replyFrames) || replyFrames.empty()) {
        std::cerr << "Error receiving search results" << std::endl;
        return result;
    }

    std::uint32_t totalHits = 0;
    std::vector<SearchResultEntry> topResults;
    if (!decodeSearchReply(std::span<const std::uint8_t>(replyFrames[0].data(), replyFrames[0].size()),
                           totalHits,
                           topResults)) {
        std::cerr << "Error receiving search results" << std::endl;
        return result;
    }

    result.totalHits = static_cast<long>(totalHits);

    for (const auto& e : topResults) {
        result.documentFrequencies.push_back({e.documentPath, static_cast<long>(e.combinedFrequency)});
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    result.excutionTime = elapsed.count();

    return std::move(result);
}

long ClientProcessingEngine::getInfo() {
    return static_cast<long>(clientId);
}

std::string ClientProcessingEngine::getServerIP() {
    return connectedServerIP;
}

std::string ClientProcessingEngine::getServerPort() {
    return connectedServerPort;
}

void ClientProcessingEngine::connect(std::string serverIP, std::string serverPort) {
    if (connected) {
        std::cout << "Already connected!" << std::endl;
        return;
    }

    zmqContext = zmq_ctx_new();
    if (!zmqContext) {
        std::cerr << "Error creating ZeroMQ context" << std::endl;
        return;
    }

    reqSocket = zmq_socket(zmqContext, ZMQ_REQ);
    if (!reqSocket) {
        std::cerr << "Error creating ZeroMQ REQ socket" << std::endl;
        disconnect();
        return;
    }

    const int lingerMs = 0;
    zmq_setsockopt(reqSocket, ZMQ_LINGER, &lingerMs, sizeof(lingerMs));

    const std::string endpoint = "tcp://" + serverIP + ":" + serverPort;
    if (zmq_connect(reqSocket, endpoint.c_str()) != 0) {
        std::cerr << "Connection failed" << std::endl;
        disconnect();
        return;
    }

    connected = true;
    connectedServerIP = std::move(serverIP);
    connectedServerPort = std::move(serverPort);

    std::vector<std::vector<std::uint8_t>> requestFrames;
    requestFrames.push_back(encodeRegisterRequest());
    if (!zmqSendMultipart(reqSocket, requestFrames)) {
        std::cerr << "Failed to register with server" << std::endl;
        disconnect();
        return;
    }

    std::vector<std::vector<std::uint8_t>> replyFrames;
    if (!zmqRecvMultipart(reqSocket, replyFrames) || replyFrames.empty()) {
        std::cerr << "Failed to register with server" << std::endl;
        disconnect();
        return;
    }

    std::uint32_t id = 0;
    if (!decodeRegisterReply(std::span<const std::uint8_t>(replyFrames[0].data(), replyFrames[0].size()), id)) {
        std::cerr << "Failed to register with server" << std::endl;
        disconnect();
        return;
    }

    clientId = id;
}

void ClientProcessingEngine::disconnect() {
    if (connected) {
        connected = false;
        clientId = 0;
        connectedServerIP.clear();
        connectedServerPort.clear();
    }

    if (reqSocket) {
        zmq_close(reqSocket);
        reqSocket = nullptr;
    }
    if (zmqContext) {
        zmq_ctx_term(zmqContext);
        zmqContext = nullptr;
    }
}
