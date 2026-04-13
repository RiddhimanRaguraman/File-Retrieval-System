#include "ClientProcessingEngine.hpp"
#include "File.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <iostream>

namespace fs = std::filesystem;

ClientProcessingEngine::ClientProcessingEngine() {}

bool ClientProcessingEngine::connect(std::string serverIP, std::string serverPort) {
    std::string address = serverIP + ":" + serverPort;
    channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    stub = fre::FileRetrievalEngine::NewStub(channel);
    
    grpc::ClientContext context;
    google::protobuf::Empty request;
    fre::RegisterRep reply;
    
    grpc::Status status = stub->Register(&context, request, &reply);
    
    if (status.ok()) {
        clientId = reply.client_id();
        return true;
    } else {
        clientId = -1;
        std::cerr << "Failed to register client: " << status.error_message() << std::endl;
        return false;
    }
}

long ClientProcessingEngine::getInfo() {
    return clientId;
}

IndexResult ClientProcessingEngine::indexFolder(std::string folderPath) {
    auto start = std::chrono::high_resolution_clock::now();
    long long totalBytes = 0;
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                
                // Get file size
                uintmax_t fileSize = 0;
                try {
                    fileSize = fs::file_size(entry.path());
                } catch (const fs::filesystem_error& e) {
                    continue;
                }
                
                totalBytes += fileSize;
                
                std::string content;
                if (fileSize > 0) {                    
                    std::ifstream file(filePath, std::ios::binary);
                    if (!file) {
                        continue;
                    }
                    
                    std::string fileContent(fileSize, '\0');
                    file.read(&fileContent[0], fileSize);
                    content = std::move(fileContent);
                }

                // Process words
                std::unordered_map<std::string, long> wordFreqs;
                std::string word;
                for (char c : content) {
                    if (isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
                        word += c;
                    } else {
                        if (word.length() > 3) {
                            wordFreqs[word]++;
                        }
                        word.clear();
                    }
                }
                if (word.length() > 3) {
                    wordFreqs[word]++;
                }

                // Strip folderPath from filePath for cleaner storage
                std::string relPath = filePath;
                if (filePath.find(folderPath) == 0) {
                    relPath = filePath.substr(folderPath.length());
                    // Remove leading separator if present
                    if (!relPath.empty() && (relPath[0] == '/' || relPath[0] == '\\')) {
                        relPath = relPath.substr(1);
                    }
                }
                
                // RPC
                grpc::ClientContext context;
                fre::IndexReq request;
                request.set_client_id(clientId);
                request.set_document_path(relPath);
                for (const auto& pair : wordFreqs) {
                    (*request.mutable_word_frequencies())[pair.first] = pair.second;
                }
                fre::IndexRep reply;
                
                grpc::Status status = stub->ComputeIndex(&context, request, &reply);
                if (!status.ok()) {
                    std::cerr << "Failed to index file " << filePath << ": " << status.error_message() << std::endl;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error indexing folder: " << e.what() << std::endl;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    return {diff.count(), totalBytes};
}

SearchResult ClientProcessingEngine::search(std::vector<std::string> terms) {
    auto start = std::chrono::high_resolution_clock::now();
    
    grpc::ClientContext context;
    fre::SearchReq request;
    for (const auto& term : terms) {
        request.add_terms(term);
    }
    fre::SearchRep reply;
    
    grpc::Status status = stub->ComputeSearch(&context, request, &reply);
    
    SearchResult result;
    if (status.ok()) {
        for (const auto& pair : reply.search_results()) {
            result.documentFrequencies.push_back({pair.first, pair.second});
        }
        
        // Sort again because map is unordered
        std::sort(result.documentFrequencies.begin(), result.documentFrequencies.end(),
            [](const auto& a, const auto& b) {
                if (a.wordFrequency != b.wordFrequency) {
                    return a.wordFrequency > b.wordFrequency;
                }
                return a.documentPath < b.documentPath;
            });
    } else {
        std::cerr << "Search failed: " << status.error_message() << std::endl;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    result.excutionTime = diff.count();
    
    return result;
}
