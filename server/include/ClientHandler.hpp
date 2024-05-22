#ifndef CLIENT_HANDLER_HPP
#define CLIENT_HANDLER_HPP

#include <iostream>
#include <thread>
#include <deque>
#include <vector>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

struct Client {
    uint32_t id;
    struct sockaddr_in client_addr;
    std::vector<double> data;
    uint32_t data_size; 
    uint16_t* missed_packets;
};

class ClientHandler {
public:
    ClientHandler() {}
    ClientHandler(const uint32_t& max_clients);
    uint32_t AddClient(const struct sockaddr_in& client_addr);
    
    bool RemoveClient(const uint32_t& client_id);
    Client& GetClient(const uint32_t& client_id);
    std::vector<Client> clients_;
private:
    std::mutex mx_deque_clients_;
    std::mutex mx_deque_ids_;
    std::deque<uint32_t> available_client_ids_;
    uint32_t last_client_id_;
};

#endif // CLIENT_HANDLER_HPP