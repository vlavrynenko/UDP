#include "ClientHandler.hpp"

ClientHandler::ClientHandler(const uint32_t& max_clients = 256) {
    clients_.resize(max_clients);
    for(int i = 0; i < max_clients; ++i) {
        available_client_ids_.push_back(i);
    }
}

uint32_t ClientHandler::AddClient(const struct sockaddr_in& client_addr) {
    Client client;
    client.client_addr = client_addr;
    {
        std::lock_guard<std::mutex> lock(mx_deque_ids_);
        client.id = available_client_ids_[0];
        available_client_ids_.pop_front();
    }
    
    {
        std::lock_guard<std::mutex> lock(mx_deque_clients_);
        clients_[client.id] = client;
    }

    return client.id;
}

bool ClientHandler::RemoveClient(const uint32_t& client_id) {
    {
        std::lock_guard<std::mutex> lock(mx_deque_ids_);
        available_client_ids_.push_back(client_id);
    }

    return true;
}

Client& ClientHandler::GetClient(const uint32_t& client_id) {
    return clients_[client_id];
}