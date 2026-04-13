#ifndef SERVER_PROCESSING_ENGINE_H
#define SERVER_PROCESSING_ENGINE_H

#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstdint>

#include "IndexStore.hpp"

class ServerProcessingEngine {
    std::shared_ptr<IndexStore> store;
    
    void* zmqContext = nullptr;

    std::thread proxyThread;
    std::vector<std::thread> workerThreads;
    std::atomic<bool> running{false};
    std::atomic<std::uint32_t> nextClientId{1};

    std::mutex proxyReadyMutex;
    std::condition_variable proxyReadyCv;
    bool proxyReady = false;

    int port = 0;
    int workerCount = 0;

    void runProxy();
    void runWorker();

    public:
        // constructor
        ServerProcessingEngine(std::shared_ptr<IndexStore> store);

        // default virtual destructor
        virtual ~ServerProcessingEngine() = default;

        void initialize(int serverPort, int numberOfWorkerThreads);
        
        void shutdown();

        std::vector<std::string> getConnectedClients();
};

#endif
