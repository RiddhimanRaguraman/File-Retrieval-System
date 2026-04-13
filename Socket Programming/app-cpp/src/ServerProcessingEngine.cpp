#include "ServerProcessingEngine.hpp"
#include "Protocol.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <thread>
#include <mutex>
#include <vector>
#include <unordered_map>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


ServerProcessingEngine::ServerProcessingEngine(std::shared_ptr<IndexStore> store) 
    : store(store), running(false), nextClientId(1), serverSocket(-1) { }

void ServerProcessingEngine::initialize(int serverPort) {
    running = true;
    dispatcherThread = std::thread(&ServerProcessingEngine::runDispatcher, this, serverPort);
}

void ServerProcessingEngine::runDispatcher(int port) {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error binding socket" << std::endl;
        return;
    }

    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Error listening" << std::endl;
        return;
    }

    std::cout << "Server listening on port " << port << std::endl;

    while (running) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

        if (!running) {
            if (clientSocket >= 0) {
                close(clientSocket);
            }
            break;
        }

        if (clientSocket < 0) {
            continue; // Likely interrupted or error
        }

        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            activeSockets.push_back(clientSocket);
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
        int clientPort = ntohs(clientAddr.sin_port);

        // Spawn worker thread
        workerThreads.emplace_back(&ServerProcessingEngine::runWorker, this, clientSocket, std::string(clientIP), clientPort);
    }
    
    close(serverSocket);
}

void ServerProcessingEngine::runWorker(int clientSocket, std::string clientIP, int clientPort) {
    // Protocol loop
    int clientId = -1;

    while (running) {
        uint32_t typeVal;
        if (!readUint32(clientSocket, typeVal)) {
            break; // Connection closed or error
        }

        MessageType type = static_cast<MessageType>(typeVal);

        if (type == MessageType::REGISTER_REQUEST) {
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clientId = nextClientId++;
                connectedClients[clientId] = {clientSocket, clientIP, clientPort, clientId};
            }
            
            sendUint32(clientSocket, static_cast<uint32_t>(MessageType::REGISTER_REPLY));
            sendUint32(clientSocket, clientId);
        }
        else if (type == MessageType::INDEX_REQUEST) {
            // Read client ID (verify?)
            uint32_t reqClientId;
            readUint32(clientSocket, reqClientId);
            
            std::string docPath;
            readString(clientSocket, docPath);

            uint32_t numWords;
            readUint32(clientSocket, numWords);

            std::unordered_map<std::string, int> wordCounts;
            for (uint32_t i = 0; i < numWords; ++i) {
                std::string word;
                uint32_t freq;
                readString(clientSocket, word); // Read word
                readUint32(clientSocket, freq); // Read frequency
                
                wordCounts[word] = freq;
            }

            // Prefix doc path with client ID
            std::string fullPath = "client" + std::to_string(reqClientId) + ":" + docPath;
            
            //std::cout << "Received INDEX_REQUEST for " << fullPath << " with " << wordCounts.size() << " terms" << std::endl;

            // Update IndexStore
            long docNum = store->putDocument(fullPath);
            store->updateIndex(docNum, wordCounts);

            //std::cout << "Sending INDEX_REPLY for " << fullPath << std::endl;

            // Send Reply
            sendUint32(clientSocket, static_cast<uint32_t>(MessageType::INDEX_REPLY));
            sendUint32(clientSocket, 0); // Success
        }
        else if (type == MessageType::SEARCH_REQUEST) {
            uint32_t numTerms;
            readUint32(clientSocket, numTerms);
            
            std::vector<std::string> terms;
            for(uint32_t i=0; i<numTerms; ++i) {
                std::string term;
                readString(clientSocket, term);
                terms.push_back(term);
            }

            // Perform search
            std::map<long, long> docScores; // docNum -> totalFreq
            bool firstTerm = true;

            for (const auto& term : terms) {
                DocFreqList list = store->lookupIndex(term);
                std::map<long, long> currentTermDocs;
                
                for (int i=0; i<list.size(); ++i) {
                    currentTermDocs[list.getDocId(i)] = list.getFreq(i);
                }
                
                if (firstTerm) {
                    docScores = currentTermDocs;
                    firstTerm = false;
                } else {
                    // Intersect
                    std::map<long, long> nextDocScores;
                    for (const auto& pair : docScores) {
                        long docId = pair.first;
                        if (currentTermDocs.count(docId)) {
                            // Sum frequencies from all terms?
                            // The traces usually imply summing frequencies for OR queries, or keeping them for AND queries.
                            // The problem description usually implies AND semantics for "search term1 term2".
                            // If it's AND, the frequency is usually the sum of frequencies of all terms in that doc.
                            nextDocScores[docId] = pair.second + currentTermDocs[docId];
                        }
                    }
                    docScores = nextDocScores;
                }

                if (docScores.empty()) break;
            }

            // Sort by frequency (descending) and then by document path (ascending)
            struct DocResult {
                long docId;
                long freq;
                std::string path;
            };
            std::vector<DocResult> sortedDocs;
            sortedDocs.reserve(docScores.size());
            
            for (const auto& pair : docScores) {
                sortedDocs.push_back({pair.first, pair.second, store->getDocument(pair.first)});
            }

            std::sort(sortedDocs.begin(), sortedDocs.end(), [](const DocResult& a, const DocResult& b) {
                if (a.freq != b.freq) {
                    return a.freq > b.freq; // Descending freq
                }
                return a.path < b.path; // Ascending path
            });
            
            // Limit to top 10 results
            uint32_t totalHits = sortedDocs.size();
            if (sortedDocs.size() > 10) {
                sortedDocs.resize(10);
            }

            // Send Reply
            sendUint32(clientSocket, static_cast<uint32_t>(MessageType::SEARCH_REPLY));
            // IMPORTANT: The protocol might expect the total hits count BEFORE limiting to 10.
            // OR it might expect the count of sent items.
            // If the trace says "10" but we have 100 hits, usually we send 100 as total hits, and 10 as list size.
            // However, looking at the trace, it doesn't show the protocol headers, just the output.
            // Let's assume standard behavior: Total Hits = All Matches, List Size = Min(10, All Matches).
            sendUint32(clientSocket, totalHits);
            sendUint32(clientSocket, static_cast<uint32_t>(sortedDocs.size()));
            
            for (const auto& doc : sortedDocs) {
                sendString(clientSocket, doc.path);
                sendUint32(clientSocket, static_cast<uint32_t>(doc.freq));
            }
        }
        else if (type == MessageType::QUIT) {
            break;
        }
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        if (clientId != -1) {
            connectedClients.erase(clientId);
        }
        // Remove from activeSockets
        auto it = std::find(activeSockets.begin(), activeSockets.end(), clientSocket);
        if (it != activeSockets.end()) {
            activeSockets.erase(it);
        }
    }

    close(clientSocket);
}

void ServerProcessingEngine::shutdown() {
    running = false;
    
    // Close server socket to unblock accept()
    if (serverSocket >= 0) {
        // Shutdown RDWR first to ensure unblocking
        ::shutdown(serverSocket, SHUT_RDWR);
        close(serverSocket);
        serverSocket = -1;
    }

    // Force close all active client sockets to unblock workers
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (int sock : activeSockets) {
            ::shutdown(sock, SHUT_RDWR);
            close(sock);
        }
        activeSockets.clear();
        connectedClients.clear();
    }

    if (dispatcherThread.joinable()) {
        dispatcherThread.join();
    }

    for (auto& t : workerThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

std::vector<std::string> ServerProcessingEngine::getConnectedClients() {
    std::vector<std::string> result;
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto& pair : connectedClients) {
        const auto& info = pair.second;
        std::stringstream ss;
        ss << "client " << info.id << ":" << info.ip << ":" << info.port;
        result.push_back(ss.str());
    }
    return result;
}
