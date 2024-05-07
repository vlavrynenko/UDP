cmake_minimum_required(VERSION 3.10)
enable_language(CXX)
set(CMAKE_CXX_FLAGS -pthread)
find_package(Threads REQUIRED)
project(UDPServer)
include_directories(include)
add_executable(server
               main.cpp
               source/Server.cpp
               source/ConfReader.cpp
               source/ClientHandler.cpp
               source/Logger.cpp)