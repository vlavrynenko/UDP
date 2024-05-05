#include "Server.hpp"

constexpr int BUFFER_SIZE = 2048;
const unsigned int MAX_PACKET_SIZE = 2048;

Server::Server(const std::string& path)
    : reader_(path)
    , sockfd(0)
    , client_handler_(256) {
    ReadConfigs();
    StartServer();
    std::cout << "Max threads: " << std::thread::hardware_concurrency() << "\n";
}

bool Server::Initialize() {
    #ifdef _WIN32
    WSADATA wsa;
    // Initialize WinSocket
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return false;
    }

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Error creating socket\n";
        return false;
    }
    #else
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Error creating socket\n";
        return false;
    }
    #endif

    memset(&server_addr_, 0, sizeof(server_addr_));

    // Server information
    const int port_ = server_conf_.server_port;
    std::cout << "port: " << server_conf_.server_port << "\n";
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
        std::cerr << "Error binding socket\n";
        #ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
        #else
        close(sockfd);
        #endif
        return false;
    }

    std::cout << "UDP server listening on port_ " << port_ << std::endl;

    return true;
}

void Server::Run() {
    std::cout << "Run()\n";
    receiving_thread_ = std::thread([this](){
        char buffer[BUFFER_SIZE];
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        Packet packet;
        while (true) {
            signed int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &len);
            if (n < sizeof(ProtocolHeader)) {
                std::cout << "Invalid packet: header size = " << sizeof(ProtocolHeader) << " received bytes = " << n << "\n";
                continue;
            }
            //short            sin_family;   // e.g. AF_INET
            //unsigned short   sin_port;     // e.g. htons(3490)
            //struct in_addr   sin_addr;     // see struct in_addr, below
            //char             sin_zero[8];
            std::cout << "sin_family: " << client_addr.sin_family << " sin_port: " << client_addr.sin_port << " addr: " << client_addr.sin_addr.s_addr << "\n";
            packet.client_addr = client_addr;
            packet.buffer = new char[n];
            memcpy(packet.buffer, buffer, n);
            packet.buffer_size = n;

            {
                std::lock_guard<std::mutex> lock(mx_deque_packets_);
                packets_.emplace_back(std::move(packet));
            }
        }
    });
    std::cout << "Started receiving\n";
    sending_thread_ = std::thread([this](){
        ToSend to_send;
        while(true) {
            if (!sending_data_.empty()) {
                std::lock_guard<std::mutex> lock(mx_deque_sending_data_);
                to_send = std::move(sending_data_[0]);
                sending_data_.pop_front();
            } else {
                continue;
            }

            Client client = client_handler_.GetClient(to_send.client_id);
            /*unsigned int buffer_size;
            char* buffer;
            switch(to_send.type) {
                case MessageType::ACKNOWLEDGE: {
                    AcknowledgeHeader header;
                    header.received_packet_number = to_send.data.get()[0];
                    header.client_id = to_send.client_id;
                    buffer = new char[sizeof(AcknowledgeHeader) + to_send.buffer_size];
                    memcpy(buffer, &header, sizeof(header));
                    memcpy(buffer + sizeof(header), to_send.data, to_send.data_size);
                    break;
                }
                case MessageType::RESPONSE:
                case MessageType::ERROR:
                    buffer = new char[to_send.buffer_size];
                    memcpy(buffer, to_send.data, sizeof(header));
                    break;
                }
                default: {
                    std::cout << "Unexpected type\n"; 
                }
            }*/
            SendMessage(client.client_addr, to_send.data, to_send.data_size, to_send.type);
        }
    });

    //processing_thread_ = std::thread([this](){
    Packet packet;
    ProtocolHeader* header;
    while (true) {
        if (packets_.size() > 0) {
            std::lock_guard<std::mutex> lock(mx_deque_packets_);
            packet = packets_[0];
            packets_.pop_front();
        } else {
            continue;
        }
        header = reinterpret_cast<ProtocolHeader*>(packet.buffer);
        switch(header->type) {
            case MessageType::REQUEST: {
                ProcessRequest(packet.client_addr, packet.buffer + sizeof(ProtocolHeader), packet.buffer_size);
                break;
            }
            case MessageType::ACKNOWLEDGE: {
                ProcessAcknowledge(packet.client_addr, packet.buffer + sizeof(ProtocolHeader), packet.buffer_size);
                break;
            }
            case MessageType::ERROR: {
                std::cout << "Error\n";
                break;
            }
            case MessageType::RESPONSE: {
                std::cout << "Unexpected packet type\n";
                break;
            }
            case MessageType::MISSED_PACKETS: {
                std::cout << "Got missed packets\n";
                //ProcessMissedPackets(packet.client_addr, buffer + sizeof(ProtocolHeader), packet.buffer_size);
                break;
            }
            default: {
                break;
            }
        }
    }
    //});
    std::cout << "Started processing\n";

    std::cout << "Started sending\n";

    /*working_thread = std::thread([this](){
        Client client;
        while (true) {
            if(clients.empty() == false) {
                int value = 0;
                {
                    std::lock_guard<std::mutex> lock(mx_deque);
                    client = clients[0];
                    clients.pop_front();
                }
                std::cout << "Client popped #" << client.id << "\n";
                auto data = std::async(DoBusinessLogic, value);

                std::vector<double> arr;
                arr.reserve(protocol_conf_.values_amount + 1);
                arr.push_back(sizeof(double) * protocol_conf_.values_amount);
                for(int i = 1; i < protocol_conf_.values_amount + 1; ++i) {
                    arr.push_back(i);
                    if (i % 100000 == 0) {
                        std::cout << arr.size() << "\n";
                    }
                }
                std::cout << "Vector filled " << arr[0] << "\n";

                struct sockaddr_in client_addr = client.client_addr;
                socklen_t len = sizeof(client_addr);

                int sent_bytes = 0;
                int remaining_bytes = arr.size() * sizeof(double);
                std::cout << "Arr size: " << remaining_bytes << "\n";
                char *buffer = reinterpret_cast<char*>(arr.data());
                int counter = 0;
                while (remaining_bytes > 0) {
                    ++counter;
                    //std::cout << "remaining_bytes: " << remaining_bytes << "\n";
                    int bytes_to_send = std::min(remaining_bytes, MAX_PACKET_SIZE);
                    //std::cout << "bytes_to_send: " << bytes_to_send << "\n";
                    int bytes_sent = sendto(sockfd, buffer + sent_bytes, bytes_to_send, 0, 
                                            (struct sockaddr *)&client_addr, sizeof(client_addr));
                    //std::cout << "bytes_sent: " << bytes_sent << "\n";
                    if (bytes_sent < 0) {
                        std::cerr << "Error sending data\n";
                        break;
                    }
                    sent_bytes += bytes_sent;
                    remaining_bytes -= bytes_sent;
                    sleep(0.01);
                }
                std::cout << "packets sent: " << counter << "\n";

                std::cout << "Message sent " << client.id << "\n";
            }
        }
    });*/
}

void Server::ProcessAcknowledge(const struct sockaddr_in client_addr, char* buffer, const unsigned int& buffer_size) {
    AcknowledgeHeader* header = reinterpret_cast<AcknowledgeHeader*>(buffer);
    client_handler_.RemoveClient(header->client_id);
}

bool Server::StartServer() {
    if (Initialize()) {
        Run();
    }

    return true;
}
void Server::ReadConfigs() {
    server_conf_ = reader_.ReadServerConfig();
    protocol_conf_ = reader_.ReadProtocolConfig();
}

bool Server::CheckVersion(const unsigned int& version_major, const unsigned int& version_minor) {
    return ((version_major == protocol_conf_.version_major) && (version_minor == protocol_conf_.version_minor));
}

bool Server::ProcessRequest(const struct sockaddr_in client_addr, char* buffer, const unsigned int& buffer_size) {
    RequestHeader* header = reinterpret_cast<RequestHeader*>(buffer);
    //int version_major = static_cast<int>(buffer[0]);
    //int version_minor = static_cast<int>(buffer[1]);
    //int value = *(reinterpret_cast<int*>(buffer + 2));
    if (!CheckVersion(header->version_major, header->version_minor)) {
        std::cout << "Wrong Protocol Version\n";
        //return false;
    }

    auto client_id = client_handler_.AddClient(client_addr, header->value);
    std::cout << "Confirm client id: " << client_id << "\n";
    ToSend ack_to_send;
    AcknowledgeHeader a_header;
    a_header.client_id = client_id;
    a_header.received_packet_number = 1;
    char a_buffer[sizeof(AcknowledgeHeader)];
    memcpy(a_buffer, &a_header, sizeof(AcknowledgeHeader));
    ack_to_send.type = MessageType::ACKNOWLEDGE;
    ack_to_send.client_id = client_id;
    ack_to_send.data_size = sizeof(AcknowledgeHeader);
    ack_to_send.data = a_buffer;
    {
        std::lock_guard<std::mutex> lock(mx_deque_sending_data_);
        sending_data_.emplace_back(ack_to_send);
    }
    ToSend result = DoBusinessLogic(client_id, header->value);
    delete (buffer - sizeof(ProtocolHeader));

    result.type = MessageType::RESPONSE;

    {
        std::lock_guard<std::mutex> lock(mx_deque_sending_data_);
        sending_data_.emplace_back(result);
    }
    return true;
}

bool Server::SendMessage(const struct sockaddr_in addr, char* buffer, const unsigned int& buffer_size, const MessageType& type) {
    struct sockaddr_in client_addr = addr;
    std::cout << "sin_family: " << client_addr.sin_family << " sin_port: " << client_addr.sin_port << " addr: " << client_addr.sin_addr.s_addr << "\n";
    socklen_t len = sizeof(client_addr);

    unsigned int sent_bytes = 0;
    unsigned int remaining_bytes = buffer_size;
    
    unsigned int counter = 0;
    unsigned int data_size = MAX_PACKET_SIZE - sizeof(ProtocolHeader);
    signed char mbuffer[MAX_PACKET_SIZE];

    ProtocolHeader header;
    ProtocolHeader* test;
    header.packets_total = buffer_size / data_size;
    if (buffer_size % MAX_PACKET_SIZE > 0) {
        ++header.packets_total;
    }
    std::cout << "server packets total: " << header.packets_total << "\n";
    header.packet_number = 0;
    header.data_size = data_size;
    header.type = type;

    unsigned int offset = 0;
    while (remaining_bytes > 0) {
        ++header.packet_number;
        if (header.packet_number == header.packets_total) {
            std::cout << "sending last packet\n";
        }
        memset(mbuffer, 0, MAX_PACKET_SIZE / sizeof(int));
        memcpy(mbuffer, &header, sizeof(ProtocolHeader));
        memcpy(mbuffer + sizeof(ProtocolHeader), buffer + sent_bytes, data_size);
        test = reinterpret_cast<ProtocolHeader*>(mbuffer);
        if (header.packet_number == header.packets_total) {
            std::cout << "Data copied to buffer\n";
        }
        //std::cout << "remaining_bytes: " << remaining_bytes << "\n";
        int bytes_to_send = std::min(static_cast<unsigned int>(remaining_bytes + sizeof(ProtocolHeader)), MAX_PACKET_SIZE);
        //std::cout << "bytes_to_send: " << bytes_to_send << "\n";
        int bytes_sent = sendto(sockfd, mbuffer, bytes_to_send, 0, 
                                (struct sockaddr *)&client_addr, sizeof(client_addr));
        //std::cout << "bytes_sent: " << bytes_sent << "\n";
        if (bytes_sent < 0) {
            std::cerr << "Error sending data\n";
            break;
        }
        sent_bytes += bytes_sent - sizeof(ProtocolHeader);
        remaining_bytes -= bytes_sent - sizeof(ProtocolHeader);
        sleep(0.01);
    }

    return true;
}

/*bool Server::AddClient() {
    int version_major = static_cast<int>(buffer[0]);
    int version_minor = static_cast<int>(buffer[1]);
    int value = *(reinterpret_cast<int*>(buffer + 2));
    char *hostaddrp;
    /*struct hostent *hostp;
    hostp = gethostbyaddr((const char *)&client_addr.sin_addr.s_addr, 
                          sizeof(client_addr.sin_addr.s_addr), AF_INET);
    if (hostp == nullptr) {}
    hostaddrp = inet_ntoa(client_addr.sin_addr);
    if (hostaddrp == nullptr) {}
    printf("server received datagram from %s\n", hostaddrp);
    std::cout << "V" << version_major << "." << version_minor << " value: " << value << "\n";
    Client client;
    client.value = value;
    client.client_addr = client_addr;
    client.id = ++client_id_;
    {
        std::lock_guard<std::mutex> lock(mx_deque);
        clients.push_back(client);
    }
    std::cout << "New client #" << client_id_ << "\n";
    return true;
}*/
/*
bool Server::SendData(const Client& client, const Data& data) {
    return true;
}*/

ToSend Server::DoBusinessLogic(const unsigned int& client_id, const signed int& value) {
    ToSend to_send;

    std::vector<double> arr;
    arr.reserve(protocol_conf_.values_amount);
    //arr.push_back(sizeof(double) * protocol_conf_.values_amount);
    for(int i = 0; i < protocol_conf_.values_amount; ++i){
        arr.push_back(i);
        if (i % 100000 == 0) {
            std::cout << arr.size() << "\n";
        }
    }
    Client client = client_handler_.GetClient(client_id);
    client.AllocateMemory(arr.size() * sizeof(double));
    std::cout << "delete Data pointer: " << client.data << "\n";
    //delete *client.data;
    std::cout << "Data pointer: " << client.data << "\n";
    memset(client.data, 0, arr.size());
    client.data_size = arr.size() * sizeof(double);
    memcpy(client.data, arr.data(), arr.size() * sizeof(double));

    to_send.data = client.data;
    to_send.data_size = arr.size() * sizeof(double);
    to_send.client_id = client.id;
    return to_send;
}

Server::~Server() {
    receiving_thread_.join();
    sending_thread_.join();
    //working_thread.join();
    processing_thread_.join();
    #ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
    #else
    close(sockfd);
    #endif
}
