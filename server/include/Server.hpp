#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <cstring>
#include <deque>
#include <thread>
#include <memory>
#include <mutex>
#include <array>
#include <unordered_set>
#include <random>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#include "ConfReader.hpp"
#include "ClientHandler.hpp"
#include "Logger.hpp"
#include "Constants.hpp"
#include "Protocol.hpp"

struct Packet {
    struct sockaddr_in client_addr;
    char* buffer;
    unsigned int buffer_size;
};

struct ToSend {
    MessageType type;
    struct sockaddr_in client_addr;
    unsigned int data_size;
    char* data;
    bool custom_packet_number;
    bool delete_data;
    unsigned short* packet_numbers;
};

class Server {
public:
    Server(const std::string& path);
    bool Initialize();
    Server(const Server&) = delete;
    void Run();

    bool StartServer();
    void StartReceiving();
    void StartSending();
    void ReadConfigs();
    bool SendMessage(const struct sockaddr_in& client_addr, char* buffer, const unsigned int& buffer_size, const MessageType& type, unsigned short* packet_numbers = nullptr);
    bool CheckVersion(const unsigned int& version_major, const unsigned int& version_minor);
    bool ProcessRequest(const struct sockaddr_in& client_addr, char* buffer, const unsigned int& buffer_size);
    void ProcessMissedPackets(const struct sockaddr_in& client_addr, char* buffer, const unsigned int& buffer_size);
    void ProcessAcknowledge(const struct sockaddr_in& client_addr, char* buffer, const unsigned int& buffer_size);
    void ProcessConnect(const struct sockaddr_in& client_addr, char* buffer, const unsigned int& buffer_size);
    void SendAcknowledge(const struct sockaddr_in& client_addr, const unsigned int& client_id, const unsigned int& packet_number);
    ToSend DoBusinessLogic(const unsigned int& client_id, const double& value);
    void SendError(const struct sockaddr_in& client_addr, const ErrorCode& code);

    ~Server();
private:
    ClientHandler client_handler_;
    std::vector<std::thread> processing_threads_;
    std::thread receiving_thread_;
    std::thread sending_thread_;
    std::thread processing_thread_;
    std::deque<ToSend> sending_data_;
    std::deque<Packet> packets_;
    ConfReader reader_;
    ServerConfig server_conf_;
    ProtocolConfig protocol_conf_;
    std::mutex mx_deque_packets_;
    std::mutex mx_deque_sending_data_;
    int port_;
    int sockfd;
    struct sockaddr_in server_addr_;
    Logger logger_;
    std::string log;
};

#endif // SERVER_HPP