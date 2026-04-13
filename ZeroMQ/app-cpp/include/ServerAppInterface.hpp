#ifndef SERVER_APP_INTERFACE_H
#define SERVER_APP_INTERFACE_H

#include <memory>

#include "ServerProcessingEngine.hpp"

class ServerAppInterface
{
    std::shared_ptr<ServerProcessingEngine> engine;

    public:
        // constructor
        ServerAppInterface(std::shared_ptr<ServerProcessingEngine> engine);

        // default virtual destructor
        virtual ~ServerAppInterface() = default;

        // read commands from the user
        void readCommands();
};

#endif