#ifndef INPUT_HANDLER_HPP
#define INPUT_HANDLER_HPP

#include <string>
#include "StringList.hpp"

class InputHandler {
    StringList history;
    size_t historyIndex;

public:
    InputHandler();
    virtual ~InputHandler() = default;
    
    InputHandler(const InputHandler&) = delete;
    InputHandler& operator=(const InputHandler&) = delete;
    InputHandler(InputHandler&&) = delete;
    InputHandler& operator=(InputHandler&&) = delete;

    void print(const std::string& message);
    void printLine(const std::string& message);
    void clearLine();
    std::string readLine();
    bool isInteractive();
};

#endif
