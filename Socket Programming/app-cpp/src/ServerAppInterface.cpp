#include "ServerAppInterface.hpp"
#include "InputHandler.hpp"

#include <iostream>
#include <vector>
#include <string>

ServerAppInterface::ServerAppInterface(std::shared_ptr<ServerProcessingEngine> engine) : engine(engine) { }

void ServerAppInterface::readCommands() {
    InputHandler inputHandler;
    std::string command;
    
    while (true) {
        inputHandler.print("> ");
        
        // read from command line
        command = inputHandler.readLine();

        // if the command is quit, terminate the program       
        if (command == "quit") {
            if (engine->getConnectedClients().size() > 0) {
                inputHandler.printLine("clients are active");
                continue;
            }
            engine->shutdown();
            break;
        }

        // if the command begins with list, list all the connected clients
        if (command == "list") {
            auto clients = engine->getConnectedClients();
            inputHandler.printLine("client ID:IP:PORT");
            for (const auto& client : clients) {
                inputHandler.printLine(client);
            }
            continue;
        }

        inputHandler.printLine("unrecognized command!");
    }
}
