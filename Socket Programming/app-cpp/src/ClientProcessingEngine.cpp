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
#include <cstring> // For memset
#include <algorithm> // For std::sort if needed
#include <cctype> // For isalnum

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // For close
#include <netdb.h>

namespace fs = std::filesystem;

ClientProcessingEngine::ClientProcessingEngine() : clientSocket(-1), clientId(-1), connected(false) {
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

                // Send INDEX_REQUEST
                //std::cout << "Sending INDEX_REQUEST for " << filePath << " with " << wordCounts.size() << " terms" << std::endl;
                sendUint32(clientSocket, static_cast<uint32_t>(MessageType::INDEX_REQUEST));
                sendUint32(clientSocket, clientId); // Client ID                
                sendString(clientSocket, filePath);
                sendUint32(clientSocket, static_cast<uint32_t>(wordCounts.size()));

                for (const auto& pair : wordCounts) {
                    sendString(clientSocket, pair.first);
                    sendUint32(clientSocket, pair.second);
                }

                // Wait for INDEX_REPLY
                uint32_t typeVal;
                if (!readUint32(clientSocket, typeVal)) break;
                if (static_cast<MessageType>(typeVal) != MessageType::INDEX_REPLY) {
                    std::cerr << "Unexpected response to INDEX_REQUEST" << std::endl;
                    break;
                }
                uint32_t status;
                readUint32(clientSocket, status);
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

    sendUint32(clientSocket, static_cast<uint32_t>(MessageType::SEARCH_REQUEST));
    sendUint32(clientSocket, static_cast<uint32_t>(terms.size()));
    for (const auto& term : terms) {
        sendString(clientSocket, term);
    }

    uint32_t typeVal;
    if (readUint32(clientSocket, typeVal) && static_cast<MessageType>(typeVal) == MessageType::SEARCH_REPLY) {
        uint32_t totalHits;
        readUint32(clientSocket, totalHits);
        result.totalHits = totalHits;

        uint32_t numResults;
        readUint32(clientSocket, numResults);
        
        for (uint32_t i = 0; i < numResults; ++i) {
            std::string docPath;
            readString(clientSocket, docPath);
            uint32_t freq;
            readUint32(clientSocket, freq);
            result.documentFrequencies.push_back({docPath, (long)freq});
        }
    } else {
        std::cerr << "Error receiving search results" << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    result.excutionTime = elapsed.count();

    return std::move(result);
}

long ClientProcessingEngine::getInfo() {
    return clientId;
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

    struct addrinfo hints, *res;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(serverIP.c_str(), serverPort.c_str(), &hints, &res) != 0) {
        std::cerr << "Invalid address or port" << std::endl;
        return;
    }

    clientSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (clientSocket < 0) {
        std::cerr << "Error creating socket" << std::endl;
        freeaddrinfo(res);
        return;
    }

    if (::connect(clientSocket, res->ai_addr, res->ai_addrlen) < 0) {
        std::cerr << "Connection failed" << std::endl;
        close(clientSocket);
        clientSocket = -1;
        freeaddrinfo(res);
        return;
    }

    freeaddrinfo(res);
    connected = true;
    connectedServerIP = serverIP;
    connectedServerPort = serverPort;

    // Send REGISTER_REQUEST
    sendUint32(clientSocket, static_cast<uint32_t>(MessageType::REGISTER_REQUEST));
    
    // Receive REGISTER_REPLY
    uint32_t typeVal;
    if (readUint32(clientSocket, typeVal) && static_cast<MessageType>(typeVal) == MessageType::REGISTER_REPLY) {
        uint32_t id;
        readUint32(clientSocket, id);
        clientId = id;
        // std::cout << "Connection successful!" << std::endl; // Printed by AppInterface
    } else {
        std::cerr << "Failed to register with server" << std::endl;
        disconnect();
    }
}

void ClientProcessingEngine::disconnect() {
    if (connected) {
        sendUint32(clientSocket, static_cast<uint32_t>(MessageType::QUIT));
        close(clientSocket);
        clientSocket = -1;
        connected = false;
        clientId = -1;
    }
}
