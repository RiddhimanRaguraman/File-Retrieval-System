#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

#include "ClientProcessingEngine.hpp"

void runWorker(std::string serverIP, std::string serverPort, std::string datasetPath, std::shared_ptr<ClientProcessingEngine> engine, IndexResult& result)
{
    engine->connect(serverIP, serverPort);
    result = engine->indexFolder(datasetPath);
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::cerr << "Usage: ./file-retrieval-benchmark <serverIP> <serverPort> <numberOfClients> <datasetPath1> ..." << std::endl;
        return 1;
    }

    std::string serverIP = argv[1];
    std::string serverPort = argv[2];
    int numberOfClients = std::stoi(argv[3]);
    
    if (argc < 4 + numberOfClients) {
        std::cerr << "Not enough dataset paths provided. Expected " << numberOfClients << " paths." << std::endl;
        return 1;
    }

    std::vector<std::string> clientsDatasetPath;
    for (int i = 0; i < numberOfClients; ++i) {
        clientsDatasetPath.push_back(argv[4 + i]);
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::shared_ptr<ClientProcessingEngine>> engines;
    std::vector<std::thread> threads;
    std::vector<IndexResult> results(numberOfClients);

    for (int i = 0; i < numberOfClients; ++i) {
        auto engine = std::make_shared<ClientProcessingEngine>();
        engines.push_back(engine);
        threads.emplace_back(runWorker, serverIP, serverPort, clientsDatasetPath[i], engine, std::ref(results[i]));
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    long long totalBytesRead = 0;
    for (const auto& res : results) {
        totalBytesRead += res.totalBytesRead;
    }
    
    double throughput = (totalBytesRead / (1024.0 * 1024.0)) / diff.count();

    std::cout << "Completed indexing " << numberOfClients << " clients in " << diff.count() << " seconds" << std::endl;
    std::cout << "Total bytes read: " << totalBytesRead << std::endl;
    std::cout << "Indexing throughput: " << throughput << " MB/s" << std::endl;

    if (!engines.empty()) {
        std::cout << "\n--- Performing Post-Indexing Search Queries ---\n" << std::endl;

        auto printResults = [](const SearchResult& res) {
            std::cout << "Time: " << res.excutionTime << "s" << std::endl;
            int count = 0;
            for (const auto& doc : res.documentFrequencies) {
                if (count >= 10) break;
                std::cout << doc.documentPath << " (" << doc.wordFrequency << ")" << std::endl;
                count++;
            }
        };

        // Query 1: Single term
        std::cout << "Query 1: 'distributed'" << std::endl;
        SearchResult res1 = engines[0]->search({"distributed"});
        printResults(res1);
        std::cout << "-----------------------------------" << std::endl;

        // Query 2: Two terms
        std::cout << "Query 2: 'distributed', 'systems'" << std::endl;
        SearchResult res2 = engines[0]->search({"distributed", "systems"});
        printResults(res2);
        std::cout << "-----------------------------------" << std::endl;

        // Query 3: Three terms
        std::cout << "Query 3: 'distributed', 'systems', 'performance'" << std::endl;
        SearchResult res3 = engines[0]->search({"distributed", "systems", "performance"});
        printResults(res3);
        std::cout << "-----------------------------------" << std::endl;
    }

    return 0;
}
