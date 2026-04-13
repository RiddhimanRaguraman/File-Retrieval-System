#include "ClientAppInterface.hpp"
#include "InputHandler.hpp"

#include <iostream>
#include <string>
#include <sstream>

ClientAppInterface::ClientAppInterface(std::shared_ptr<ClientProcessingEngine> engine) : engine(engine) {
    // Constructor
}

void ClientAppInterface::readCommands() {
    InputHandler inputHandler;
    std::string command;
    
    while (true) {
        if (inputHandler.isInteractive()) {
            inputHandler.print("> ");
        }
        command = inputHandler.readLine();

        std::stringstream ss(command);
        std::string cmd;
        ss >> cmd;

        if (cmd.empty()) {
            continue;
        }

        if (cmd == "quit") {
            break;
        }

        if (cmd == "connect") {
            std::string ip, port;
            ss >> ip >> port;
            if (!ip.empty() && !port.empty()) {
                if (engine->connect(ip, port)) {
                    inputHandler.printLine("Connection successful!");
                } else {
                    inputHandler.printLine("Connection failed!");
                }
            } else {
                inputHandler.printLine("Usage: connect <ip> <port>");
            }
            continue;
        }

        if (cmd == "get_info") {
            inputHandler.printLine("client ID: " + std::to_string(engine->getInfo()));
            continue;
        }
        
        if (cmd == "index") {
            std::string path;
            ss >> path;
            if (!path.empty()) {
                IndexResult res = engine->indexFolder(path);
                inputHandler.printLine("Completed indexing " + std::to_string(res.totalBytesRead) + " bytes of data");
                inputHandler.printLine("Completed indexing in " + std::to_string(res.executionTime) + " seconds");
            }
            continue;
        }

        if (cmd == "search") {
            std::vector<std::string> terms;
            std::string term;
            while (ss >> term) {
                if (term == "AND") {
                    continue;
                }
                terms.push_back(term);
            }
            
            if (!terms.empty()) {
                SearchResult res = engine->search(terms);
                inputHandler.printLine("Search completed in " + std::to_string(res.excutionTime) + " seconds");
                inputHandler.printLine("Search results (top 10 out of " + std::to_string(res.documentFrequencies.size()) + "):");
                int count = 0;
                for (const auto& doc : res.documentFrequencies) {
                    if (count >= 10) break;
                    inputHandler.printLine("* " + doc.documentPath + ":" + std::to_string(doc.wordFrequency));
                    count++;
                }
            } else {
                inputHandler.printLine("Usage: search <term1> [term2 ...]");
            }
            continue;
        }

        inputHandler.printLine("unrecognized command!");
    }
}
