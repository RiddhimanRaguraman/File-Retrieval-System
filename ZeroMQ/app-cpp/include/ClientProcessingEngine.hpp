#ifndef CLIENT_PROCESSING_ENGINE_H
#define CLIENT_PROCESSING_ENGINE_H

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

struct IndexResult {
    double executionTime;
    long totalBytesRead;
};

struct DocPathFreqPair {
    std::string documentPath;
    long wordFrequency;
};

struct SearchResult {
    double excutionTime;
    long totalHits;
    std::vector<DocPathFreqPair> documentFrequencies;
};

class ClientProcessingEngine {
    void* zmqContext = nullptr;
    void* reqSocket = nullptr;
    std::uint32_t clientId;
    bool connected;
    std::string connectedServerIP;
    std::string connectedServerPort;

    public:
        // constructor
        ClientProcessingEngine();

        // default virtual destructor
        virtual ~ClientProcessingEngine();

        IndexResult indexFolder(std::string folderPath);
        
        SearchResult search(std::vector<std::string> terms);
        
        long getInfo();
        std::string getServerIP();
        std::string getServerPort();

        void connect(std::string serverIP, std::string serverPort);
        
        void disconnect();
};

#endif
