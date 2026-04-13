#include <iostream>
#include <memory>

#include "IndexStore.hpp"
#include "ServerProcessingEngine.hpp"
#include "ServerAppInterface.hpp"

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "Usage: ./file-retrieval-server <port> <number of worker threads>" << std::endl;
        return 1;
    }
    int serverPort = std::stoi(argv[1]);
    int numberOfWorkerThreads = std::stoi(argv[2]);

    std::shared_ptr<IndexStore> store = std::make_shared<IndexStore>();
    std::shared_ptr<ServerProcessingEngine> engine = std::make_shared<ServerProcessingEngine>(store);
    std::shared_ptr<ServerAppInterface> interface = std::make_shared<ServerAppInterface>(engine);

    engine->initialize(serverPort, numberOfWorkerThreads);

    // read commands from the user
    interface->readCommands();

    return 0;
}
