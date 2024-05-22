#include "Server.hpp"

Server::Server(const std::string& path)
    : reader_(path)
    , sockfd(0)
    , client_handler_(256)
    , logger_("logs.txt") {
    ReadConfigs();
    StartServer();
    std::cout << "Max threads: " << std::thread::hardware_concurrency() << "\n";
}

bool Server::Initialize() {
    #ifdef _WIN32
    WSADATA wsa;
    // Initialize WinSocket
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        logger_.Log("WSAStartup failed");
        return false;
    }

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        logger_.Log("Error creating socket");
        return false;
    }
    #else
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        logger_.Log("Error creating socket");
        return false;
    }
    #endif

    memset(&server_addr_, 0, sizeof(server_addr_));

    // Server information
    port_ = server_conf_.server_port;
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr_.sin_port = htons(port_);

    // Bind socket
    if (bind(sockfd, (struct sockaddr *)&server_addr_,
        #ifdef _WIN32
        sizeof(server_addr_)) == SOCKET_ERROR
        #else
        sizeof(server_addr_)) < 0
        #endif
        ) {
        logger_.Log("Error binding socket");
        #ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
        #else
        close(sockfd);
        #endif
        return false;
    }

    std::cout << "UDP server listening on port_ " << port_ << "\n";

    return true;
}

void Server::Run() {
    logger_.Log(__func__);
    Packet packet;
    ProtocolHeader* header;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mx_deque_sending_data_);
            cv_recv.wait(lock, [this](){ return !packets_.empty();});
            packet = packets_[0];
            packets_.pop_front();
        }

        header = reinterpret_cast<ProtocolHeader*>(packet.buffer);
        switch(header->type) {
            case MessageType::REQUEST: {
                ProcessRequest(packet.client_addr, packet.buffer, packet.buffer_size);
                break;
            }
            case MessageType::ACKNOWLEDGE: {
                ProcessAcknowledge(packet.client_addr, packet.buffer, packet.buffer_size);
                break;
            }
            case MessageType::ERROR_CODE: {
                std::cout << "Error\n";
                break;
            }
            case MessageType::RESPONSE: {
                std::cout << "Unexpected packet type\n";
                break;
            }
            case MessageType::MISSED_PACKETS: {
                std::cout << "Got missed packets\n";
                ProcessMissedPackets(packet.client_addr, packet.buffer, packet.buffer_size);
                break;
            }
            case MessageType::CONNECT: {
                std::cout << "Connection request\n";
                ProcessConnect(packet.client_addr, packet.buffer, packet.buffer_size);
                break;
            }
            default: {
                break;
            }
        }
        delete[] packet.buffer;
    }
}

void Server::ProcessMissedPackets(const struct sockaddr_in& client_addr, char* buffer, const uint32_t& buffer_size) {
    logger_.Log(__func__);
    ProtocolHeader* p_header = reinterpret_cast<ProtocolHeader*>(buffer);
    MissedPacketsHeader* m_header = reinterpret_cast<MissedPacketsHeader*>(buffer + sizeof(ProtocolHeader));
    uint32_t packet_data_size = MAX_PACKET_SIZE - sizeof(ProtocolHeader);
    Client client = client_handler_.GetClient(m_header->client_id);

    char* m_buffer = new char[m_header->total_packets_missed * packet_data_size];
    uint16_t packet_number = 0;
    uint32_t offset = 0;
    uint32_t data_offset = 0;
    uint32_t copy_amount = packet_data_size;
    uint16_t* packet_numbers = new uint16_t[m_header->total_packets_missed];
    for(uint32_t i = 0; i < m_header->total_packets_missed; ++i) {
        memcpy(&packet_number, buffer + sizeof(ProtocolHeader) + sizeof(MissedPacketsHeader) + (offset * sizeof(uint16_t)), sizeof(uint16_t));
        packet_numbers[i] = packet_number;
        ++offset;
        if (packet_number * packet_data_size > client.data_size) {
            copy_amount = client.data_size - (packet_number - 1) * packet_data_size;
        }
        memcpy(m_buffer + data_offset, client.data.data() + ((packet_number - 1) * packet_data_size), copy_amount);
        data_offset += packet_data_size;
    }

    ToSend to_send;
    to_send.type = MessageType::RESPONSE;
    to_send.client_addr = client_addr;
    to_send.data_size = (m_header->total_packets_missed - 1) * (MAX_PACKET_SIZE - sizeof(ProtocolHeader)) + copy_amount;
    to_send.data = m_buffer;
    to_send.delete_data = true;
    to_send.custom_packet_number = true;
    to_send.packet_numbers = packet_numbers;

    {
        std::lock_guard<std::mutex> lock(mx_deque_sending_data_);
        sending_data_.emplace_back(to_send);
        cv_send.notify_one();
    }
}

void Server::ProcessConnect(const struct sockaddr_in& client_addr, char* buffer, const uint32_t& buffer_size) {
    logger_.Log(__func__);
    ProtocolHeader* p_header = reinterpret_cast<ProtocolHeader*>(buffer);
    ConnectHeader* c_header = reinterpret_cast<ConnectHeader*>(buffer + sizeof(ProtocolHeader));
    if (!CheckVersion(c_header->version_major, c_header->version_minor)) {
        SendError(client_addr, ErrorCode::INVALID_VERSION);
        return;
    }

    uint32_t client_id = client_handler_.AddClient(client_addr);
    SendAcknowledge(client_addr, client_id, p_header->packet_number);
}

void Server::SendAcknowledge(const struct sockaddr_in& client_addr, const uint32_t& client_id, const uint32_t& packet_number) {
    logger_.Log(__func__);
    ToSend ack_to_send;
    AcknowledgeHeader a_header;
    a_header.client_id = client_id;
    a_header.received_packet_number = packet_number;
    char a_buffer[sizeof(AcknowledgeHeader)];
    memcpy(a_buffer, &a_header, sizeof(AcknowledgeHeader));
    ack_to_send.type = MessageType::ACKNOWLEDGE;
    ack_to_send.client_addr = client_addr;
    ack_to_send.data_size = sizeof(AcknowledgeHeader);
    ack_to_send.data = a_buffer;
    ack_to_send.delete_data = false;
    {
        std::lock_guard<std::mutex> lock(mx_deque_sending_data_);
        sending_data_.push_back(ack_to_send);
        cv_send.notify_one();
    }
}

void Server::ProcessAcknowledge(const struct sockaddr_in& client_addr, char* buffer, const uint32_t& buffer_size) {
    logger_.Log(__func__);
    AcknowledgeHeader* header = reinterpret_cast<AcknowledgeHeader*>(buffer + sizeof(ProtocolHeader));
    logger_.Log("Remove client: " + header->client_id);
    client_handler_.RemoveClient(header->client_id);
}

bool Server::StartServer() {
    logger_.Log(__func__);
    if (Initialize()) {
        StartReceiving();
        StartSending(); 
        Run();
    }

    return true;
}

void Server::StartReceiving() {
    logger_.Log(__func__);
    receiving_thread_ = std::thread([this](){
        char buffer[BUFFER_SIZE];
        struct sockaddr_in client_addr;
        #ifdef _WIN32
        signed int len = sizeof(client_addr);
        #else
        socklen_t len = sizeof(client_addr);
        #endif
        Packet packet;
        while (true) {
            signed int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &len);
            std::ostringstream oss;
            oss << "sin_family: " << client_addr.sin_family << " sin_port: " << client_addr.sin_port << " addr: " << client_addr.sin_addr.s_addr;
            logger_.Log("Message from: " + oss.str());
            if (n < sizeof(ProtocolHeader)) {
                SendError(client_addr, ErrorCode::INVALID_HEADER);
                logger_.Log("Invalid header received");
                continue;
            }

            packet.client_addr = client_addr;
            packet.buffer = new char[n];
            memcpy(packet.buffer, buffer, n);
            packet.buffer_size = n;

            {
                std::lock_guard<std::mutex> lock(mx_deque_packets_);
                packets_.emplace_back(std::move(packet));
                cv_recv.notify_one();
            }
        }
    });
}

void Server::StartSending() {
    logger_.Log(__func__);
    sending_thread_ = std::thread([this](){
        ToSend to_send;
        while(true) {
            {
                std::unique_lock<std::mutex> lock(mx_deque_sending_data_);
                cv_send.wait(lock, [this](){ return !sending_data_.empty();});
                to_send = std::move(sending_data_[0]);
                sending_data_.pop_front();
            }
            
            uint16_t* packet_numbers;
            if (to_send.custom_packet_number == true) {
                packet_numbers = to_send.packet_numbers;
            } else {
                packet_numbers = nullptr;
            }

            SendMessage(to_send.client_addr, to_send.data, to_send.data_size, to_send.type, packet_numbers);
            if (to_send.delete_data == true) {
                delete to_send.data;
                delete[] to_send.packet_numbers;
            }
        }
    });
}

void Server::ReadConfigs() {
    logger_.Log(__func__);
    server_conf_ = reader_.ReadServerConfig();
    protocol_conf_ = reader_.ReadProtocolConfig();
}

bool Server::CheckVersion(const uint32_t& version_major, const uint32_t& version_minor) {
    logger_.Log(__func__);
    return ((version_major == PROTOCOL_VERSION_MAJOR) && (version_minor == PROTOCOL_VERSION_MINOR));
}

bool Server::ProcessRequest(const struct sockaddr_in& client_addr, char* buffer, const uint32_t& buffer_size) {
    logger_.Log(__func__);
    RequestHeader* header = reinterpret_cast<RequestHeader*>(buffer + sizeof(ProtocolHeader));

    ToSend result = DoBusinessLogic(header->client_id, header->value);
    if (result.data == nullptr) {
        return false;
    }

    result.type = MessageType::RESPONSE;

    {
        std::lock_guard<std::mutex> lock(mx_deque_sending_data_);
        sending_data_.push_back(result);
        cv_send.notify_one();
    }
    return true;
}

bool Server::SendMessage(const struct sockaddr_in& addr, char* buffer, const uint32_t& buffer_size, const MessageType& type, uint16_t* packet_numbers) {
    logger_.Log(__func__);
    struct sockaddr_in client_addr = addr;
    #ifdef _WIN32
    signed int len = sizeof(client_addr);
    #else
    socklen_t len = sizeof(client_addr);
    #endif

    uint32_t sent_bytes = 0;
    uint32_t remaining_bytes = buffer_size;
    
    uint32_t counter = 0;
    uint32_t data_size = MAX_PACKET_SIZE - sizeof(ProtocolHeader);
    signed char mbuffer[MAX_PACKET_SIZE];

    ProtocolHeader header;
    ProtocolHeader* test;
    header.packets_total = buffer_size / data_size;
    if (buffer_size % MAX_PACKET_SIZE > 0) {
        ++header.packets_total;
    }
    std::ostringstream oss;
    oss << __func__ << ": server packets total: " << header.packets_total;
            
    logger_.Log(oss.str());
    header.packet_number = 0;
    header.data_size = data_size;
    header.type = type;

    uint32_t offset = 0;
    while (remaining_bytes > 0) {
        if (packet_numbers == nullptr) {
            ++header.packet_number;            
        } else {
            header.packet_number = packet_numbers[counter];
            //std::cout << "sending packet: " << header.packet_number << "\n";
        }
        ++counter;
        memcpy(mbuffer, &header, sizeof(ProtocolHeader));
        memcpy(mbuffer + sizeof(ProtocolHeader), buffer + sent_bytes, data_size);
        test = reinterpret_cast<ProtocolHeader*>(mbuffer);

        int bytes_to_send = std::min(static_cast<uint32_t>(remaining_bytes + sizeof(ProtocolHeader)), MAX_PACKET_SIZE);
        int bytes_sent = sendto(sockfd, mbuffer, bytes_to_send, 0, 
                                (struct sockaddr *)&client_addr, sizeof(client_addr));
        if (bytes_sent < 0) {
            logger_.Log("Error sending data");
            break;
        }
        sent_bytes += bytes_sent - sizeof(ProtocolHeader);
        remaining_bytes -= bytes_sent - sizeof(ProtocolHeader);
        sleep(0.01);
    }

    return true;
}

void Server::SendError(const struct sockaddr_in& client_addr, const ErrorCode& code) {
    logger_.Log(__func__);
    ToSend error_to_send;
    ErrorHeader e_header;
    e_header.error = code;
    e_header.version_major = PROTOCOL_VERSION_MAJOR;
    e_header.version_minor = PROTOCOL_VERSION_MINOR;
    char e_buffer[sizeof(ErrorHeader)];
    memcpy(e_buffer, &e_header, sizeof(ErrorHeader));
    error_to_send.type = MessageType::ERROR_CODE;
    error_to_send.client_addr = client_addr;
    error_to_send.data_size = sizeof(ErrorHeader);
    error_to_send.data = e_buffer;
    error_to_send.delete_data = false;
    {
        std::lock_guard<std::mutex> lock(mx_deque_sending_data_);
        sending_data_.push_back(error_to_send);
        cv_send.notify_one();
    }
}

ToSend Server::DoBusinessLogic(const uint32_t& client_id, const double& value) {
    logger_.Log(__func__);
    ToSend to_send;

    Client& client = client_handler_.GetClient(client_id);
    uint32_t count = protocol_conf_.values_amount;
    double min = 0;
    double max = 0;
    if (value > 0) {
        min -= value;
        max = value;
    }
    if (value < 0) {
        min = value;
        max -= value;
    }
    if (value == 0) {
        SendError(client.client_addr, ErrorCode::INVALID_VALUE);
        to_send.data = nullptr;
        return to_send;
    }
    // Seed random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(min, max);

    // Use unordered_set to ensure uniqueness
    std::unordered_set<double> unique_set;

    while (unique_set.size() < static_cast<size_t>(count)) {
        unique_set.insert(dis(gen));
    }

    // Convert unordered_set to vector
    client.data = std::move(std::vector<double>(unique_set.begin(), unique_set.end()));
    client.data_size = client.data.size() * sizeof(double);

    to_send.data = reinterpret_cast<char*>(client.data.data());
    to_send.data_size = client.data_size;
    to_send.client_addr = client.client_addr;
    to_send.delete_data = false;
    return to_send;
}

Server::~Server() {
    receiving_thread_.join();
    sending_thread_.join();
    processing_thread_.join();
    #ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
    #else
    close(sockfd);
    #endif
}
