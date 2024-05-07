#include "Client.hpp"

Client::Client()
    : counter(0)
    , logger("logs.txt")
    , reader("./") {
    if (Initialize()) {
        Run();
    }
}

bool Client::Initialize() {
    #ifdef _WIN32
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        logger.Log("WSAStartup failed");
        return false;
    }

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        logger.Log("Error creating socket");
        return false;
    }
    #else
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        logger.Log("Error creating socket");
        return false;
    }
    #endif
    conf = reader.ReadServerConfig();

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(conf.server_ip.c_str()); // Change to server IP address
    server_addr.sin_port = htons(conf.server_port);

    return true;
}

bool Client::Run() {
    logger.Log(__func__);
    arr.resize(BUFFER_SIZE / sizeof(double));
    bool result = false;

    int packet_num = 1;
    int offset = 0;
    unsigned int total_packets = 0;
    ProtocolHeader* rheader;
    bool ack_received = false;
    unsigned int retries = 5;

    //Send connection request
    ConnectHeader c_header;
    c_header.version_major = 1;
    c_header.version_minor = 0;
    logger.Log("Send Connect");

    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds(10);
    for(int i = 0; i < retries; ++i) {
        if (ack_received == false) {
            PrepareDataToSend(c_header, MessageType::CONNECT);
            logger.Log("Wait for ack");
            len = sizeof(server_addr);

            pollStruct[0].fd = sockfd;
            pollStruct[0].events = POLLIN;
            
            start = std::chrono::system_clock::now();
            while (true) {
                if (poll(pollStruct, 1, 1000) == 1) {
                    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len);
                    ProtocolHeader* p_header = reinterpret_cast<ProtocolHeader*>(buffer);
                    if (p_header->type != MessageType::ACKNOWLEDGE) {
                        if (p_header->type == MessageType::ERROR) {
                            ErrorHeader* e_header = reinterpret_cast<ErrorHeader*>(buffer + sizeof(ProtocolHeader));
                            HandleError(*e_header);
                            break;   
                        }
                        logger.Log("Unexpected Message");
                        break;
                    }
                    AcknowledgeHeader* a_header = reinterpret_cast<AcknowledgeHeader*>(buffer + sizeof(ProtocolHeader));
                    client_id = a_header->client_id;
                    ack_received = true;
                    logger.Log("Ack received, client_id = " + client_id);
                    break;
                } else {
                    end = std::chrono::system_clock::now();
                    if (end - start >= elapsed_seconds) {
                        logger.Log("Stop waiting");
                        break;
                    }
                    sleep(0.5);
                    logger.Log("No response message");
                }

            }
        } else {
            break;
        }
    }
    if (ack_received == false) {
        return false;
    }
    sleep(3);

    // Send message to server
    RequestHeader r_header;
    r_header.client_id = client_id;
    r_header.value = conf.value;;
    PrepareDataToSend(r_header, MessageType::REQUEST);

    unsigned int packet_data_size = BUFFER_SIZE - sizeof(ProtocolHeader);
    unsigned int total_packets_expected = 0;
    len = sizeof(server_addr);

    ProtocolHeader* p_header;
    start = std::chrono::system_clock::now();
    while (true) {
        pollStruct[0].fd = sockfd;
        pollStruct[0].events = POLLIN;
        if (poll(pollStruct, 1, 1000) == 1) {
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len);
            p_header = reinterpret_cast<ProtocolHeader*>(buffer);
            if (p_header->type != MessageType::RESPONSE) {
                if (p_header->type == MessageType::ERROR) {
                    ErrorHeader* e_header = reinterpret_cast<ErrorHeader*>(buffer + sizeof(ProtocolHeader));
                    HandleError(*e_header);
                    break;
                }
            }
            
            ++counter;
            if (counter == 1) {
                arr.resize(p_header->packets_total * packet_data_size / sizeof(double));
                packets_received.resize(p_header->packets_total + 1);
                total_packets_expected = p_header->packets_total;
                elapsed_seconds = std::chrono::duration<double>(1);
            }

            offset = packet_data_size * (p_header->packet_number - 1);
            memcpy(arr.data() + (offset/sizeof(double)), buffer + sizeof(ProtocolHeader), n - sizeof(ProtocolHeader));
            packets_received[p_header->packet_number] = true;
            std::ostringstream oss;
            oss << "On " << offset << " Received bytes " << n - sizeof(ProtocolHeader) << " Packet number " << p_header->packet_number;
            logger.Log(oss.str());
            if (p_header->packets_total == p_header->packet_number) {
                arr.resize((((p_header->packets_total - 1) * packet_data_size) + n - sizeof(ProtocolHeader)) / sizeof(double));
                break;
            }
            start = std::chrono::system_clock::now();
        } else {
            end = std::chrono::system_clock::now();
            if (end - start >= elapsed_seconds) {
                break;
            }
        }
    }
    std::vector<unsigned short> missed_packets;
    for(short i = 1; i < packets_received.size(); ++i) {
        if(packets_received[i] == false) {
            missed_packets.push_back(i);
        }
    }
    if (missed_packets.size() == 0) {
        AcknowledgeHeader a_header;
        a_header.client_id = client_id;
        PrepareDataToSend(a_header, MessageType::ACKNOWLEDGE);
    } else {
        for(int i = 0; i < retries; ++i) {
            PrepareMissingPackets(missed_packets, client_id);
            start = std::chrono::system_clock::now();
            while (true) {
                pollStruct[0].fd = sockfd;
                pollStruct[0].events = POLLIN;
                if (poll(pollStruct, 1, 1000) == 1) {
                    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len);
                    p_header = reinterpret_cast<ProtocolHeader*>(buffer);
                    if (p_header->type != MessageType::RESPONSE) {
                        if (p_header->type == MessageType::ERROR) {
                            ErrorHeader* e_header = reinterpret_cast<ErrorHeader*>(buffer + sizeof(ProtocolHeader));
                            HandleError(*e_header);
                            break;
                        }
                    }
                    
                    ++counter;
                    offset = packet_data_size * (p_header->packet_number - 1);
                    memcpy(arr.data() + (offset/sizeof(double)), buffer + sizeof(ProtocolHeader), n - sizeof(ProtocolHeader));
                    packets_received[p_header->packet_number] = true;
                    std::ostringstream oss;
                    oss << "On " << offset << " Received bytes " << n - sizeof(ProtocolHeader) << " Packet number " << p_header->packet_number;
                    logger.Log(oss.str());
                    if (total_packets_expected == p_header->packet_number) {
                        arr.resize((((p_header->packets_total - 1) * packet_data_size) + n - sizeof(ProtocolHeader)) / sizeof(double));
                        break;
                    }
                    start = std::chrono::system_clock::now();
                } else {
                    end = std::chrono::system_clock::now();
                    if (end - start >= elapsed_seconds) {
                        break;
                    }
                }
            }
            missed_packets.clear();
            for(short i = 1; i < packets_received.size(); ++i) {
                if(packets_received[i] == false) {
                    missed_packets.push_back(i);
                }
            }
            if (missed_packets.size() == 0) {
                break;
            }
        }
    }

    if (missed_packets.size() == 0) {
        AcknowledgeHeader a_header;
        a_header.client_id = client_id;
        PrepareDataToSend(a_header, MessageType::ACKNOWLEDGE);
    } else {
        logger.Log("Can't retrieve missing packets");
        return false;
    }

    std::sort(arr.begin(), arr.end(), std::greater<>());

    std::ofstream fout("result");
    std::string binary;
    for(int i = 0; i < arr.size(); ++i) {
        std::string binary = std::bitset<sizeof(double) * 8>(arr[i]).to_string();
        fout.write(binary.c_str(), binary.length());
    }
    
    fout.close();

    return true;
}

template<class T>
bool Client::PrepareDataToSend(const T& header, const MessageType& type) {
    logger.Log(__func__);
    ProtocolHeader p_header;
    p_header.packet_number = packet_num;
    p_header.packets_total = 1;
    p_header.type = type;
    p_header.data_size = sizeof(T);

    char buffer[sizeof(ProtocolHeader) + sizeof(T)];
    memcpy(buffer, &p_header, sizeof(ProtocolHeader));
    memcpy(buffer + sizeof(ProtocolHeader), &header, sizeof(T));
    SendMessage(buffer, sizeof(ProtocolHeader) + sizeof(T));

    return true;
}

bool Client::PrepareMissingPackets(const std::vector<unsigned short>& missed_packets, const unsigned int id) {
    logger.Log(__func__);
    ProtocolHeader p_header;
    p_header.packet_number = packet_num;
    p_header.packets_total = 1;
    p_header.type = MessageType::MISSED_PACKETS;
    p_header.data_size = (missed_packets.size()) * sizeof(unsigned short) + sizeof(MissedPacketsHeader);
    const unsigned int buffer_size = (missed_packets.size()) * sizeof(unsigned short) + sizeof(MissedPacketsHeader) + sizeof(ProtocolHeader); 

    MissedPacketsHeader m_header;
    m_header.client_id = id;
    m_header.total_packets_missed = missed_packets.size();

    char* buffer = new char[buffer_size];
    memcpy(buffer, &p_header, sizeof(ProtocolHeader));
    memcpy(buffer + sizeof(ProtocolHeader), &m_header, sizeof(MissedPacketsHeader));
    memcpy(buffer + sizeof(ProtocolHeader) + sizeof(MissedPacketsHeader), missed_packets.data(), (missed_packets.size()) * sizeof(unsigned short));
    MissedPacketsHeader* m_header2 = reinterpret_cast<MissedPacketsHeader*>(buffer + sizeof(ProtocolHeader));
    SendMessage(buffer, buffer_size);
    delete buffer;
    return true;
}

bool Client::SendMessage(char* buffer, const unsigned int& buffer_size) {
    if (sendto(sockfd, buffer, buffer_size, 0, (struct sockaddr *)&server_addr,
        #ifdef _WIN32
        sizeof(server_addr)) == SOCKET_ERROR
        #else
        sizeof(server_addr)) < 0
        #endif
        ) {
            logger.Log("Error sending message");
        #ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
        #else
        close(sockfd);
        #endif
        return false;
    }
    ++packet_num;
    return true;
}

bool Client::HandleError(const ErrorHeader& error) {
    bool can_continue;
    std::ostringstream oss;
    oss << "Client Version: " << PROTOCOL_VERSION_MAJOR << "." << PROTOCOL_VERSION_MINOR << " Server Version: " << error.version_major << "." << error.version_minor;
    switch(error.error) {
        case ErrorCode::INVALID_HEADER: {
            oss << " Invalid Message sent";
            can_continue = false;
            break;
        }
        case ErrorCode::INVALID_VERSION: {
            oss << "Invalid Protocol Version";
            can_continue = false;
            break;
        }
        case ErrorCode::INVALID_VALUE: {
            oss << "Invalid Value sent";
            can_continue = true;
            break;
        }
        default: {
            oss << "Unexpected error code";
            can_continue = false;
            break;
        }
    }
    logger.Log(oss.str());

    return can_continue;
}

Client::~Client() {
    #ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
    #else
    close(sockfd);
    #endif

    logger.Log("END");
}