#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <numeric>
#include <functional>

#include "ClientProcessingEngine.hpp"

void runWorker(std::shared_ptr<ClientProcessingEngine> engine, std::string serverIP, std::string serverPort, std::string datasetPath, long& bytesRead)
{
    engine->connect(serverIP, serverPort);
    if (engine->getInfo() > 0) {
        if (!datasetPath.empty()) {
            IndexResult result = engine->indexFolder(datasetPath);
            bytesRead = result.totalBytesRead;
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::cerr << "Usage: ./file-retrieval-benchmark <server IP> <server port> <number of clients> [<dataset path>...]" << std::endl;
        return 1;
    }

    std::string serverIP = argv[1];
    std::string serverPort = argv[2];
    int numberOfClients = std::stoi(argv[3]);
    std::vector<std::string> clientsDatasetPath;

    for (int i = 4; i < argc; ++i) {
        clientsDatasetPath.push_back(argv[i]);
    }

    std::vector<std::shared_ptr<ClientProcessingEngine>> engines;
    std::vector<std::thread> threads;
    std::vector<long> bytesRead(numberOfClients, 0);

    for (int i = 0; i < numberOfClients; ++i) {
        engines.push_back(std::make_shared<ClientProcessingEngine>());
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numberOfClients; ++i) {
        std::string path = (i < clientsDatasetPath.size()) ? clientsDatasetPath[i] : "";
        threads.emplace_back(runWorker, engines[i], serverIP, serverPort, path, std::ref(bytesRead[i]));
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    long totalBytes = std::accumulate(bytesRead.begin(), bytesRead.end(), 0L);
    std::cout << "Completed indexing " << totalBytes << " bytes of data" << std::endl;
    std::cout << "Completed indexing in " << elapsed.count() << " seconds" << std::endl;

    // Run search queries on the first client
    if (!engines.empty()) {
        auto& engine = engines[0];
        
        // Example queries from prompt
        std::vector<std::vector<std::string>> queries = {
            {"the"},
            {"reconcilable"},
            {"war-whoops"},
            {"amazing", "sixty-eight"}
        };

        for (const auto& query : queries) {
            std::cout << "Searching";
            for(const auto& t : query) std::cout << " " << t;
            std::cout << std::endl;

            if (query.size() > 1 && query[1] == "AND") {
                 // Handle explicit "AND" if present in vector? No, here we pass terms directly.
            }

            SearchResult result = engine->search(query);
            std::cout << "Search completed in " << result.excutionTime << " seconds" << std::endl;
            std::cout << "Search results (top 10 out of " << result.totalHits << "):" << std::endl;
             for (const auto& pair : result.documentFrequencies) {
                std::cout << "* " << pair.documentPath << ":" << pair.wordFrequency << std::endl;
            }
        }
    }

    // Disconnect all
    for (auto& engine : engines) {
        engine->disconnect();
    }

    return 0;
}
