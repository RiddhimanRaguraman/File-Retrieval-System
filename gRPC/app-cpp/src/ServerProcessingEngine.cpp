#include "ServerProcessingEngine.hpp"
#include <string>
#include <iostream>

ServerProcessingEngine::ServerProcessingEngine(std::shared_ptr<IndexStore> store) 
    : store(store), service(store) {
}

void ServerProcessingEngine::initialize(int serverPort) {
    serverThread = std::thread(&ServerProcessingEngine::rungRPCServer, this, serverPort);
}

void ServerProcessingEngine::rungRPCServer(int serverPort) {
    std::string server_address = "0.0.0.0:" + std::to_string(serverPort);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    server = builder.BuildAndStart();
    std::cout << "Server listening on 0.0.0.0:" << serverPort << std::endl;
    server->Wait();
}

std::vector<std::string> ServerProcessingEngine::getConnectedClients() {
    return {}; // gRPC is stateless, so we don't track persistent connections in this simple implementation
}

void ServerProcessingEngine::shutdown() {
    if (server) {
        server->Shutdown();
    }
    if (serverThread.joinable()) {
        serverThread.join();
    }
}
