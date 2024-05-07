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
    unsigned int id;
    struct sockaddr_in client_addr;
    char* data;
    unsigned int data_size;
    unsigned short* missed_packets;
    bool AllocateMemory(const unsigned int& size);
    bool AllocateMemoryMissedPackets(const unsigned int& size);
};

class ClientHandler {
public:
    ClientHandler() {}
    ClientHandler(const unsigned int& max_clients);
    unsigned int AddClient(const struct sockaddr_in& client_addr);
    
    bool RemoveClient(const unsigned int& client_id);
    Client& GetClient(const unsigned int& client_id);
    ~ClientHandler();
    std::vector<Client> clients_;
private:
    std::mutex mx_deque_clients_;
    std::mutex mx_deque_ids_;
    std::deque<unsigned int> available_client_ids_;
    unsigned int last_client_id_;
};

#endif // CLIENT_HANDLER_HPP