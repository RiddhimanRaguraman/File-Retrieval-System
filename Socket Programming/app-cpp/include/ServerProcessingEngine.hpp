#ifndef SERVER_PROCESSING_ENGINE_H
#define SERVER_PROCESSING_ENGINE_H

#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>

#include "IndexStore.hpp"

class ServerProcessingEngine {
    std::shared_ptr<IndexStore> store;
    
    // Thread management
    std::thread dispatcherThread;
    std::vector<std::thread> workerThreads;
    std::atomic<bool> running;
    
    // Client management
    mutable std::mutex clientsMutex;
    struct ClientInfo {
        int socket;
        std::string ip;
        int port;
        int id;
    };
    std::map<int, ClientInfo> connectedClients; // Map client ID -> ClientInfo
    std::vector<int> activeSockets; // Track ALL connected sockets, registered or not
    int nextClientId;

    int serverSocket;

    public:
        // constructor
        ServerProcessingEngine(std::shared_ptr<IndexStore> store);

        // default virtual destructor
        virtual ~ServerProcessingEngine() = default;

        void initialize(int serverPort);

        void runDispatcher(int port);

        void runWorker(int clientSocket, std::string clientIP, int clientPort);
        
        void shutdown();

        std::vector<std::string> getConnectedClients();
};

#endif
