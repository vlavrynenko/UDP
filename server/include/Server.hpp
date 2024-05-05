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

/*struct Client {
    unsigned int ip;
    unsigned int port;
    unsigned int id;
    unsigned int value;
    struct sockaddr_in client_addr;
};*/

enum class MessageType {
    ERROR = 0,
    REQUEST = 1,
    ACKNOWLEDGE = 2,
    RESPONSE = 3,
    MISSED_PACKETS = 4,
    CONNECT = 5
};

struct ConnectHeader {
    unsigned int version_major : 8;
    unsigned int version_minor : 8;
};

struct ProtocolHeader {
    unsigned int packet_number : 16;
    unsigned int packets_total : 16;
    MessageType type : 8;
    unsigned int data_size : 16;
};

struct RequestHeader {
    unsigned int client_id;
    signed int value;
};

struct AcknowledgeHeader {
    unsigned int client_id;
    unsigned int received_packet_number;
};

struct ResponseHeader {
    char* data;
};

struct MissedPacketsHeader {
    unsigned int client_id;
    unsigned int total_packets_missed;
    char* packet_numbers_missed;
};

struct Packet {
    struct sockaddr_in client_addr;
    char* buffer;
    unsigned int buffer_size;
};

struct ToSend {
    MessageType type;
    unsigned int client_id;
    unsigned int data_size;
    char* data;
};

class Server {
public:
    Server(const std::string& path);
    bool Initialize();
    Server(const Server&) = delete;
    void Run();

    bool StartServer();
    void ReadConfigs();
    //bool AddClient(const struct sockaddr_in& client_addr, char* buffer, const int& buffer_size);
    bool SendMessage(const struct sockaddr_in client_addr, char* buffer, const unsigned int& buffer_size, const MessageType& type);
    bool CheckVersion(const unsigned int& version_major, const unsigned int& version_minor);
    bool ProcessRequest(const struct sockaddr_in client_addr, char* buffer, const unsigned int& buffer_size);
    void ProcessMissedPackets(const struct sockaddr_in client_addr, char* buffer, const unsigned int& buffer_size);
    void ProcessAcknowledge(const struct sockaddr_in client_addr, char* buffer, const unsigned int& buffer_size);
    void ProcessConnect(const struct sockaddr_in client_addr, char* buffer, const unsigned int& buffer_size);
    void SendAcknowledge(const unsigned int& client_id, const unsigned int& packet_number);
    ToSend DoBusinessLogic(const unsigned int& client_id, const signed int& value);

    ~Server();
private:
    ClientHandler client_handler_;
    //int client_id_;
    //std::deque<Client> clients;
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
};

#endif // SERVER_HPP