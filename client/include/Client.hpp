#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <chrono>
#include <poll.h>
#include <bitset>
#include <random>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "Protocol.hpp"
#include "ConfReader.hpp"
#include "Logger.hpp"

constexpr int PORT = 8888;
constexpr int BUFFER_SIZE = 2048;

class Client {
public:
    Client();

    bool Run();
    bool Initialize();
    template<class T>
    bool PrepareDataToSend(const T& header, const MessageType& type);
    bool RequestMissingPackets(const unsigned int& retries);
    bool HandleError(const ErrorHeader& error);

    bool PrepareMissingPackets(const std::vector<unsigned short>& missing_packets, const unsigned int id);
    bool SendMessage(char* buffer, const unsigned int& buffer_size);
    ~Client();
private:
    unsigned int client_id;
    struct sockaddr_in server_addr;
    #ifdef _WIN32
    WSADATA wsa;
    SOCKET sockfd;
    #else
    int sockfd;
    #endif
    char buffer[BUFFER_SIZE];
    int packet_num;
    std::vector<double> arr;
    struct pollfd pollStruct[1];
    std::vector<bool> packets_received;
    unsigned int len;
    unsigned int counter;
    Logger logger;
    ServerConfig conf;
    ConfReader reader;
};

#endif // CLIENT_HPP