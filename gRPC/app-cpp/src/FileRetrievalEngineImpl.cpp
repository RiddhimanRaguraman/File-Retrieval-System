#include "FileRetrievalEngineImpl.hpp"
#include <vector>
#include <algorithm>
#include <map>

FileRetrievalEngineImpl::FileRetrievalEngineImpl(std::shared_ptr<IndexStore> store) : store(store) {
}

grpc::Status FileRetrievalEngineImpl::Register(
        grpc::ServerContext* context,
        const google::protobuf::Empty* request,
        fre::RegisterRep* reply) {
    int id = nextClientId++;
    reply->set_client_id(id);
    return grpc::Status::OK;
}

grpc::Status FileRetrievalEngineImpl::ComputeIndex(
        grpc::ServerContext* context,
        const fre::IndexReq* request,
        fre::IndexRep* reply) {
    // Format: client<id>:<path>
    std::string fullPath = "client" + std::to_string(request->client_id()) + ":" + request->document_path();
    long docNum = store->putDocument(fullPath);
    
    std::unordered_map<std::string, long> wordFreqs;
    for (auto const& [word, freq] : request->word_frequencies()) {
        wordFreqs[word] = freq;
    }
    
    store->updateIndex(docNum, wordFreqs);
    
    reply->set_ack("Indexed");
    return grpc::Status::OK;
}

grpc::Status FileRetrievalEngineImpl::ComputeSearch(
        grpc::ServerContext* context,
        const fre::SearchReq* request,
        fre::SearchRep* reply) {
    
    if (request->terms().empty()) {
        return grpc::Status::OK;
    }

    std::map<long, long> docScores;
    bool first = true;

    for (const auto& term : request->terms()) {
        DocFreqList results = store->lookupIndex(term);
        
        if (first) {
            for (int i = 0; i < results.size(); ++i) {
                docScores[results.getDocId(i)] = results.getFreq(i);
            }
            first = false;
        } else {
            std::map<long, long> currentScores;
            for (int i = 0; i < results.size(); ++i) {
                long docId = results.getDocId(i);
                if (docScores.count(docId)) {
                    currentScores[docId] = docScores[docId] + results.getFreq(i);
                }
            }
            docScores = std::move(currentScores);
        }
        
        if (docScores.empty()) break;
    }

    // Return all results (client will sort and limit)
    for (const auto& entry : docScores) {
        std::string path = store->getDocument(entry.first);
        (*reply->mutable_search_results())[path] = entry.second;
    }

    return grpc::Status::OK;
}
