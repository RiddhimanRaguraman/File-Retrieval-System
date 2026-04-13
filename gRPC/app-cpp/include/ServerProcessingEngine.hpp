#ifndef SERVER_SIDE_ENGINE_H
#define SERVER_SIDE_ENGINE_H

#include <memory>
#include <thread>

#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpc/support/time.h>

#include "IndexStore.hpp"
#include "FileRetrievalEngineImpl.hpp"

class ServerProcessingEngine {
    std::shared_ptr<IndexStore> store;
    FileRetrievalEngineImpl service;
    std::unique_ptr<grpc::Server> server;
    std::thread serverThread;

    public:
        // constructor
        ServerProcessingEngine(std::shared_ptr<IndexStore> store);

        // default virtual destructor
        virtual ~ServerProcessingEngine() = default;

        // Initialize the gRPC Server thread
        void initialize(int serverPort);

        // Create and run the gRPC Server
        void rungRPCServer(int serverPort);
        
        // Get list of connected clients (dummy implementation for now as gRPC is stateless)
        std::vector<std::string> getConnectedClients();

        // TO-DO shutdown the gRPC Server
        void shutdown();
};

#endif