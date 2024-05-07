cmake_minimum_required(VERSION 3.10)
enable_language(CXX)
set(CMAKE_CXX_FLAGS -pthread)
project(UDPClient)
include_directories(include)
add_executable(client
               main.cpp
               source/Client.cpp
               source/ConfReader.cpp
               source/Logger.cpp)