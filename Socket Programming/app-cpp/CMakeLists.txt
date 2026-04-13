cmake_minimum_required(VERSION 3.22)

project(file-retrieval-engine
        VERSION 1.0
        DESCRIPTION "C++ File Retrieval Engine"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(file-retrieval-client
               src/file-retrieval-client.cpp
               src/ClientAppInterface.cpp
               src/ClientProcessingEngine.cpp
               src/Protocol.cpp
               src/File.cpp
               src/InputHandler.cpp
               src/StringList.cpp)

target_include_directories(file-retrieval-client PUBLIC include)

add_executable(file-retrieval-benchmark
               src/file-retrieval-benchmark.cpp
               src/ClientAppInterface.cpp
               src/ClientProcessingEngine.cpp
               src/Protocol.cpp
               src/File.cpp
               src/InputHandler.cpp
               src/StringList.cpp)

target_include_directories(file-retrieval-benchmark PUBLIC include)

add_executable(file-retrieval-server
               src/file-retrieval-server.cpp
               src/ServerAppInterface.cpp
               src/ServerProcessingEngine.cpp
               src/IndexStore.cpp
               src/InvertedIndexMap.cpp
               src/DocFreqList.cpp
               src/StringList.cpp
               src/Protocol.cpp
               src/File.cpp
               src/InputHandler.cpp)

target_include_directories(file-retrieval-server PUBLIC include)

if(WIN32)
    target_link_libraries(file-retrieval-client ws2_32)
    target_link_libraries(file-retrieval-server ws2_32)
    target_link_libraries(file-retrieval-benchmark ws2_32)
endif()
