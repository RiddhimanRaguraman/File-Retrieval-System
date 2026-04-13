#include <iostream>

#include "ClientProcessingEngine.hpp"
#include "ClientAppInterface.hpp"

int main(int argc, char** argv)
{
    std::shared_ptr<ClientProcessingEngine> engine = std::make_shared<ClientProcessingEngine>();
    std::shared_ptr<ClientAppInterface> interface = std::make_shared<ClientAppInterface>(engine);

    interface->readCommands();

    return 0;
}