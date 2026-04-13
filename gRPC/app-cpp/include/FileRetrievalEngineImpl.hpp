#ifndef MATH_FORMULA_H
#define MATH_FORMULA_H

#include <grpc/grpc.h>
#include <grpcpp/server_context.h>
#include <atomic>

#include "proto/file_retrieval_engine.grpc.pb.h"

#include "IndexStore.hpp"

class FileRetrievalEngineImpl final : public fre::FileRetrievalEngine::Service
{
    std::shared_ptr<IndexStore> store;
    std::atomic<int> nextClientId = 1;

    public:
        explicit FileRetrievalEngineImpl(std::shared_ptr<IndexStore> store);
        grpc::Status Register(
            grpc::ServerContext* context,
            const google::protobuf::Empty* request,
            fre::RegisterRep* reply);
        grpc::Status ComputeIndex(grpc::ServerContext* context, const fre::IndexReq* request, fre::IndexRep* reply);
        grpc::Status ComputeSearch(grpc::ServerContext* context, const fre::SearchReq* request, fre::SearchRep* reply);
};

#endif