#ifndef CLIENT_PROCESSING_ENGINE_H
#define CLIENT_PROCESSING_ENGINE_H

#include <memory>
#include <string>
#include <vector>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "proto/file_retrieval_engine.grpc.pb.h"
#include "ResultList.hpp"

struct IndexResult {
    double executionTime;
    long long totalBytesRead;
};

struct SearchResult {
    double excutionTime;
    std::vector<DocPathFreqPair> documentFrequencies;
};

class ClientProcessingEngine {
    // keep track of the connection
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<fre::FileRetrievalEngine::Stub> stub;
    int clientId = -1;

    public:
        // constructor
        ClientProcessingEngine();

        // default virtual destructor
        virtual ~ClientProcessingEngine() = default;

        IndexResult indexFolder(std::string folderPath);
        
        SearchResult search(std::vector<std::string> terms);

        long getInfo();
        
        bool connect(std::string serverIP, std::string serverPort);
};

#endif