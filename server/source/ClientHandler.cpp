#include "ClientHandler.hpp"

ClientHandler::ClientHandler(const unsigned int& max_clients = 256) {
    //available_client_ids_.reserve(max_clients);
    clients_.resize(max_clients);
    for(int i = 0; i < max_clients; ++i) {
        available_client_ids_.push_back(i);
    }
}

unsigned int ClientHandler::AddClient(const struct sockaddr_in& client_addr) {
    Client client;
    client.client_addr = client_addr;
    {
        std::lock_guard<std::mutex> lock(mx_deque_ids_);
        client.id = available_client_ids_[0];
        available_client_ids_.pop_front();
    }
    std::cout << "Got client id: " << client.id << "\n";
    
    {
        std::lock_guard<std::mutex> lock(mx_deque_clients_);
        clients_[client.id] = client;
    }

    return client.id;
}

bool ClientHandler::RemoveClient(const unsigned int& client_id) {
    std::cout << "Removing client " << client_id << "\n";
    if (clients_[client_id].data != nullptr) {
        delete clients_[client_id].data;
    } else {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(mx_deque_ids_);
        available_client_ids_.push_back(client_id);
    }

    return true;
}

Client& ClientHandler::GetClient(const unsigned int& client_id) {
    return clients_[client_id];
}

char* Client::GetDataByOffset(const unsigned int& offset) {
    return data + offset;
}
/*Client::~Client() {
    delete data;
}*/

bool Client::AllocateMemory(const unsigned int& size) {
    try {
        data = new char[size]();
    } catch (...) {
        std::cout << "Caught exception\n";
    }
    return true;
}

bool Client::AllocateMemoryMissedPackets(const unsigned int& size) {
    try {
        missed_packets = new unsigned short[size]();
    } catch (...) {
        std::cout << "Caught exception\n";
    }
    return true;
}

ClientHandler::~ClientHandler() {
    for(auto client : clients_) {
        if (client.data != nullptr) {
            delete client.data;
        }
    }
}
