#include "ClientAppInterface.hpp"
#include "InputHandler.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

ClientAppInterface::ClientAppInterface(std::shared_ptr<ClientProcessingEngine> engine) : engine(engine) {
    // Constructor
}

void ClientAppInterface::readCommands() {
    InputHandler inputHandler;
    std::string command;
    
    while (true) {
        inputHandler.print("> ");
        
        // read from command line
        command = inputHandler.readLine();

        std::stringstream ss(command);
        std::string cmd;
        ss >> cmd;

        // if the command is quit, terminate the program       
        if (cmd == "quit") {
            // close the connection with the server
            engine->disconnect();
            break;
        }

        // if the command begins with connect, connect to the given server
        if (cmd == "connect") {
            std::string ip, port;
            if (ss >> ip) {
                if (!(ss >> port)) {
                    port = "5555";
                }
                engine->connect(ip, port);
                if (engine->getInfo() > 0) {
                    inputHandler.printLine("Connection successful!");
                }
            } else {
                inputHandler.printLine("Usage: connect <server_ip> [port]");
            }
            continue;
        }

        // if the command begins with get_info, print the client ID
        if (cmd == "get_info") {
            long id = engine->getInfo();
            if (id > 0) {
                inputHandler.printLine("client ID: " + std::to_string(id));
                inputHandler.printLine("Server: " + engine->getServerIP() + ":" + engine->getServerPort());
            } else {
                inputHandler.printLine("Not connected.");
            }
            continue;
        }
        
        // if the command begins with index, index the files from the specified directory
        if (cmd == "index") {
            std::string folder;
            if (ss >> folder) {
                IndexResult result = engine->indexFolder(folder);
                inputHandler.printLine("Completed indexing " + std::to_string(result.totalBytesRead) + " bytes of data");
                inputHandler.printLine("Completed indexing in " + std::to_string(result.executionTime) + " seconds");
            } else {
                inputHandler.printLine("Usage: index <folder>");
            }
            continue;
        }

        // if the command begins with search, search for files that matches the query
        if (cmd == "search") {
            std::vector<std::string> terms;
            std::string term;
            while (ss >> term) {
                if (term == "AND") continue;
                terms.push_back(term);
            }
            
            if (terms.empty()) {
                inputHandler.printLine("Usage: search <term1> [AND <term2>] [AND <term3>]");
                continue;
            }
            if (terms.size() > 3) {
                terms.resize(3);
            }

            std::string searchCommand = "Search";
            for (const auto& term : terms) {
                searchCommand += " " + term;
            }
            inputHandler.printLine("> " + searchCommand);
            
            SearchResult result = engine->search(terms);
            inputHandler.printLine("Search completed in " + std::to_string(result.excutionTime) + " seconds");
            inputHandler.printLine("Search results (top 10 out of " + std::to_string(result.totalHits) + "):");
            for (const auto& pair : result.documentFrequencies) {
                inputHandler.printLine("* " + pair.documentPath + ":" + std::to_string(pair.wordFrequency));
            }
            continue;
        }

        if (cmd.empty()) continue;

        inputHandler.printLine("unrecognized command!");
    }
}
