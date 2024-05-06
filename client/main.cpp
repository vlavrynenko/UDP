#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <chrono>
#include <poll.h>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif

constexpr int PORT = 8888;
constexpr int BUFFER_SIZE = 2048;

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
    unsigned int client_id : 8;
    unsigned int total_packets_missed : 16;
};

class Client {
public:
    Client();
    template<class T>
    bool PrepareDataToSend(const T& header, const MessageType& type);
    bool Connect(const unsigned int& retries);
    bool Request(const unsigned int& retries);
    bool RequestMissingPackets(const unsigned int& retries);

    bool WaitForAcknowledge();
    bool WaitForResponse();
    bool PrepareMissingPackets(const std::vector<unsigned short>& missing_packets, const unsigned int id);
    bool SendMessage(char* buffer, const unsigned int& buffer_size);
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
};

Client::Client()
    : counter(0) {
    #ifdef _WIN32
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        // throw()
    }

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Error creating socket\n";
        // throw()
    }
    #else
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Error creating socket\n";
        // throw()
    }
    #endif

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Change to server IP address
    server_addr.sin_port = htons(PORT);

    arr.resize(BUFFER_SIZE / sizeof(double));

    int packet_num = 1;
    int offset = 0;
    int transaction_size = 0;
    bool end_of_translation = false;
    bool last_packet;
    unsigned int total_packets = 0;
    ProtocolHeader* rheader;
    int client_id = 0;
    bool ack_received = false;
    unsigned int retries = 5;


    //Send connection request
    ConnectHeader c_header;
    c_header.version_major = 1;
    c_header.version_minor = 0;
    std::cout << "Send Connect\n";
    //PrepareDataToSend(c_header, MessageType::CONNECT);

    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds(10);
    start = std::chrono::system_clock::now();
    for(int i = 0; i < retries; ++i) {
        if (ack_received == false) {
            PrepareDataToSend(c_header, MessageType::CONNECT);
            std::cout << "Wait for ack\n";
            len = sizeof(server_addr);

            pollStruct[0].fd = sockfd;
            pollStruct[0].events = POLLIN;
            
            while (true) {
                if (poll(pollStruct, 1, 1000) == 1) {
                    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len);
                    ProtocolHeader* p_header = reinterpret_cast<ProtocolHeader*>(buffer);
                    if (p_header->type != MessageType::ACKNOWLEDGE) {
                        if (p_header->type == MessageType::ERROR) {
                            std::cout << "Got an error\n";
                            break;   
                        }
                        std::cout << "Unexpected Message\n";
                        break;
                    }
                    AcknowledgeHeader* a_header = reinterpret_cast<AcknowledgeHeader*>(buffer + sizeof(ProtocolHeader));
                    client_id = a_header->client_id;
                    ack_received = true;
                    std::cout << "Ack received, client_id = " << client_id << "\n";
                    //sleep(3);
                    break;
                } else {
                    end = std::chrono::system_clock::now();
                    if (end - start >= elapsed_seconds) {
                        std::cout << "Stop waiting\n";
                        break;
                    }
                    sleep(0.5);
                    std::cout << "No response message\n";
                }

            }
        } else {
            break;
        }
    }
    start = std::chrono::system_clock::now();

    // Send message to server
    RequestHeader r_header;
    r_header.client_id = client_id;
    r_header.value = 12345678;
    PrepareDataToSend(r_header, MessageType::REQUEST);

    start = std::chrono::system_clock::now();
    unsigned int packet_data_size = BUFFER_SIZE - sizeof(ProtocolHeader);
    unsigned int total_packets_expected = 0;
    len = sizeof(server_addr);

    ProtocolHeader* p_header;
    start = std::chrono::system_clock::now();
    while (true) {
        pollStruct[0].fd = sockfd;
        pollStruct[0].events = POLLIN;
        //std::cout << "Array size: " << arr.size() << "\n";
        if (poll(pollStruct, 1, 1000) == 1) {
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len);
            p_header = reinterpret_cast<ProtocolHeader*>(buffer);
            if (p_header->type != MessageType::RESPONSE) {
                if (p_header->type == MessageType::ERROR) {
                    std::cout << "Error\n";
                    break;
                }
            }
            
            ++counter;
            //std::cout << "packets total: " << rheader->packets_total << "\n";
            if (counter == 1) {
                arr.resize(p_header->packets_total * packet_data_size / sizeof(double));
                packets_received.resize(p_header->packets_total + 1);
                total_packets_expected = p_header->packets_total;
                elapsed_seconds = std::chrono::duration<double>(1);
            }

            offset = packet_data_size * (p_header->packet_number - 1);
            memcpy(arr.data() + (offset/sizeof(double)), buffer + sizeof(ProtocolHeader), n - sizeof(ProtocolHeader));
            packets_received[p_header->packet_number] = true;
            std::cout << "On " << offset << " Received bytes" << n - sizeof(ProtocolHeader) << " Packet number" << p_header->packet_number << std::endl;
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
        while (true) {
            std::cout << "Missed packets total: " << missed_packets.size() << std::endl;
            std::cout << "Before sending missing client_id = " << client_id << "\n";
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
                            std::cout << "Error\n";
                            break;
                        }
                    }
                    
                    ++counter;
                    offset = packet_data_size * (p_header->packet_number - 1);
                    memcpy(arr.data() + (offset/sizeof(double)), buffer + sizeof(ProtocolHeader), n - sizeof(ProtocolHeader));
                    packets_received[p_header->packet_number] = true;
                    std::cout << "On " << offset << " Received bytes" << n - sizeof(ProtocolHeader) << " Packet number" << p_header->packet_number << std::endl;
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
    }

    unsigned int sum = 0;
    for(int i = 0; i < arr.size(); ++i) {
        sum += arr[i];
        //std::cout << arr[i] << " ";
    }
    std::cout << "Size: " << arr.size() << " Sum: " << sum << "\n";

    std::cout << "packets received " << counter << std::endl;

    #ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
    #else
    close(sockfd);
    #endif
}

bool Client::Connect(const unsigned int& retries) {
    bool ack_received;
    ConnectHeader c_header;
    c_header.version_major = 1;
    c_header.version_minor = 0;
    std::cout << "Send Connect\n";
    //PrepareDataToSend(c_header, MessageType::CONNECT);

    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds(10);
    start = std::chrono::system_clock::now();
    for(int i = 0; i < retries; ++i) {
        if (ack_received == false) {
            PrepareDataToSend(c_header, MessageType::CONNECT);
            std::cout << "Wait for ack\n";
            len = sizeof(server_addr);

            pollStruct[0].fd = sockfd;
            pollStruct[0].events = POLLIN;
            
            while (true) {
                if (poll(pollStruct, 1, 1000) == 1) {
                    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len);
                    ProtocolHeader* p_header = reinterpret_cast<ProtocolHeader*>(buffer);
                    if (p_header->type != MessageType::ACKNOWLEDGE) {
                        if (p_header->type == MessageType::ERROR) {
                            std::cout << "Got an error\n";
                            break;   
                        }
                        std::cout << "Unexpected Message\n";
                        break;
                    }
                    AcknowledgeHeader* a_header = reinterpret_cast<AcknowledgeHeader*>(buffer + sizeof(ProtocolHeader));
                    client_id = a_header->client_id;
                    ack_received = true;
                    std::cout << "Ack received, client_id = " << client_id << "\n";
                    sleep(3);
                    break;
                } else {
                    end = std::chrono::system_clock::now();
                    if (end - start >= elapsed_seconds) {
                        std::cout << "Stop waiting\n";
                        break;
                    }
                    sleep(1);
                    std::cout << "No response message\n";
                }

            }
        } else {
            break;
        }
    }
}

bool Client::WaitForAcknowledge() {
    bool result = false;
    len = sizeof(server_addr);

    pollStruct[0].fd = sockfd;
    pollStruct[0].events = POLLIN;
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds(10);
    start = std::chrono::system_clock::now();
    while (true) {
        if (poll(pollStruct, 1, 1000) == 1) {
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len);
            ProtocolHeader* p_header = reinterpret_cast<ProtocolHeader*>(buffer);
            if (p_header->type != MessageType::ACKNOWLEDGE) {
                if (p_header->type == MessageType::ERROR) {
                    std::cout << "Got an error\n";
                    break;   
                }
                std::cout << "Unexpected Message\n";
                break;
            }
            AcknowledgeHeader* a_header = reinterpret_cast<AcknowledgeHeader*>(buffer + sizeof(ProtocolHeader));
            client_id = a_header->client_id;
            result = true;
            std::cout << "Ack received, client_id = " << client_id << "\n";
            sleep(3);
            break;
        } else {
            end = std::chrono::system_clock::now();
            if (end - start >= elapsed_seconds) {
                std::cout << "Stop waiting\n";
                break;
            }
            sleep(1);
            std::cout << "No response message\n";
        }

    }

    return result;
}

bool Client::Request(const unsigned int& retries) {
    bool result = false;
    RequestHeader r_header;
    r_header.client_id = client_id;
    r_header.value = 12345678;

    for(int i = 0; i < retries; ++i) {
        if (result == false) {
            PrepareDataToSend(r_header, MessageType::REQUEST);

            result = WaitForResponse();
        } else {
            break;
        }
        
    }

    return result;
}

bool Client::WaitForResponse() {
    bool result = false;
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds(10);
    unsigned int packet_data_size = BUFFER_SIZE - sizeof(ProtocolHeader);
    len = sizeof(server_addr);

    ProtocolHeader* p_header;
    start = std::chrono::system_clock::now();
    unsigned int offset = 0;
    while (true) {
        pollStruct[0].fd = sockfd;
        pollStruct[0].events = POLLIN;
        //std::cout << "Array size: " << arr.size() << "\n";
        if (poll(pollStruct, 1, 1000) == 1) {
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len);
            p_header = reinterpret_cast<ProtocolHeader*>(buffer);
            if (p_header->type != MessageType::RESPONSE) {
                if (p_header->type == MessageType::ERROR) {
                    std::cout << "Error\n";
                    break;
                }
            }
            
            ++counter;
            //std::cout << "packets total: " << rheader->packets_total << "\n";
            if (counter == 1) {
                arr.resize(p_header->packets_total * packet_data_size);
                packets_received.resize(p_header->packets_total + 1);
                elapsed_seconds = std::chrono::duration<double>(1);
            }

            offset = packet_data_size * (p_header->packet_number - 1);
            memcpy(arr.data() + (offset/sizeof(double)), buffer, n);
            packets_received[p_header->packet_number] = true;
            std::cout << "On " << offset << " Received bytes" << n << " Packet number" << p_header->packet_number << std::endl;
            //std::cout << "Server response: " << arr[10] << std::endl;
            //memcpy(arr.data() + offset, buffer, n);
            //if (p_header->packets_total == p_header->packet_number) {
            //    break;
            //}
            result = true;
            start = std::chrono::system_clock::now();
        } else {
            end = std::chrono::system_clock::now();
            if (end - start >= elapsed_seconds) {
                break;
            }
        }
    }
    return result;
}

template<class T>
bool Client::PrepareDataToSend(const T& header, const MessageType& type) {
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

/*bool Client::SendConnect() {
    ProtocolHeader header;
    header.packet_number = packet_num;
    header.packets_total = 1;
    header.type = MessageType::CONNECT;
    header.data_size = sizeof(ConnectHeader);

    ConnectHeader c_header;
    c_header.version_major = 1;
    c_header.version_minor = 0;

    char buffer[sizeof(ProtocolHeader) + sizeof(ConnectHeader)];
    memcpy(buffer, &header, sizeof(ProtocolHeader));
    memcpy(buffer + sizeof(ProtocolHeader), &c_header, sizeof(ConnectHeader));
    SendMessage(buffer, sizeof(ProtocolHeader) + sizeof(ConnectHeader));
    return true;
}

bool Client::SendRequest(const int& value) {
    ProtocolHeader header;
    header.packet_number = packet_num;
    header.packets_total = 1;
    header.type = MessageType::REQUEST;
    header.data_size = sizeof(RequestHeader);

    RequestHeader r_header;
    r_header.client_id = client_id;
    r_header.value = value;

    char buffer[sizeof(ProtocolHeader) + sizeof(RequestHeader)];
    memcpy(buffer, &header, sizeof(ProtocolHeader));
    memcpy(buffer + sizeof(ProtocolHeader), &r_header, sizeof(RequestHeader));
    SendMessage(buffer, sizeof(ProtocolHeader) + sizeof(RequestHeader));
    return true;
}

bool Client::SendAcknowledge() {
    return true;
}*/

bool Client::PrepareMissingPackets(const std::vector<unsigned short>& missed_packets, const unsigned int id) {
    ProtocolHeader p_header;
    p_header.packet_number = packet_num;
    p_header.packets_total = 1;
    p_header.type = MessageType::MISSED_PACKETS;
    p_header.data_size = (missed_packets.size()) * sizeof(unsigned short) + sizeof(MissedPacketsHeader);
    const unsigned int buffer_size = (missed_packets.size()) * sizeof(unsigned short) + sizeof(MissedPacketsHeader) + sizeof(ProtocolHeader); 

    MissedPacketsHeader m_header;
    m_header.client_id = id;
    m_header.total_packets_missed = missed_packets.size();
    std::cout << "Before sending missing client_id = " << id << "\n";

    char* buffer = new char[buffer_size];
    memcpy(buffer, &p_header, sizeof(ProtocolHeader));
    memcpy(buffer + sizeof(ProtocolHeader), &m_header, sizeof(MissedPacketsHeader));
    memcpy(buffer + sizeof(ProtocolHeader) + sizeof(MissedPacketsHeader), missed_packets.data(), (missed_packets.size()) * sizeof(unsigned short));
    MissedPacketsHeader* m_header2 = reinterpret_cast<MissedPacketsHeader*>(buffer + sizeof(ProtocolHeader));
    std::cout << "client_id from MissedPacketsHeader: " << m_header2->client_id << "\n";
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
            std::cerr << "Error sending message\n";
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

int main() {
    Client client;
    /*#ifdef _WIN32
    WSADATA wsa;
    SOCKET sockfd;
    #else
    int sockfd;
    #endif

    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    #ifdef _WIN32
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return -1;
    }

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Error creating socket\n";
        return -1;
    }
    #else
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Error creating socket\n";
        return -1;
    }
    #endif

    memset(&server_addr, 0, sizeof(server_addr));

    // Server information
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Change to server IP address
    server_addr.sin_port = htons(PORT);
    std::vector<double> arr(BUFFER_SIZE / sizeof(double));*/
    //ProtocolHeader header;

    /*while (true) {
        std::cout << "Enter message: ";
        //std::cin.getline(buffer, BUFFER_SIZE);
        int packet_num = 1;
        int offset = 0;
        int counter = 0;
        int transaction_size = 0;
        bool end_of_translation = false;
        bool last_packet;
        unsigned int total_packets = 0;
        ProtocolHeader* rheader;
        int client_id = 0;
        bool ack_received = false;
        struct pollfd pollStruct[1];

        std::vector<bool> packets_received;

        //Connect to server
        /*header.packet_number = packet_num;
        header.packets_total = 1;
        header.type = MessageType::CONNECT;
        header.data_size = sizeof(ConnectHeader);

        ConnectHeader c_header;
        c_header.version_major = 1;
        c_header.version_minor = 0;

        char* c_buffer = new char[sizeof(ProtocolHeader) + sizeof(ConnectHeader)];
        memcpy(c_buffer, &header, sizeof(ProtocolHeader));
        memcpy(c_buffer + sizeof(ProtocolHeader), &c_header, sizeof(ConnectHeader));
        std::cout << "Connect to server\n";
        if (sendto(sockfd, c_buffer, sizeof(ProtocolHeader) + sizeof(ConnectHeader), 0, (struct sockaddr *)&server_addr,
            #ifdef _WIN32
            sizeof(server_addr)) == SOCKET_ERROR
            #else
            sizeof(server_addr)) < 0
            #endif
            ) {
                std::cerr << "Error sending message\n";
            #ifdef _WIN32
            closesocket(sockfd);
            WSACleanup();
            #else
            close(sockfd);
            #endif
            return -1;
        }
        ++packet_num;

        //Send connection request
        ConnectHeader c_header;
        c_header.version_major = 1;
        c_header.version_minor = 0;
        PrepareDataToSend(c_header, MessageType::CONNECT);

        //wait for response
        unsigned int len = sizeof(server_addr);

        pollStruct[0].fd = sockfd;
        pollStruct[0].events = POLLIN;
        bool timeout = false;
        std::chrono::time_point<std::chrono::system_clock> start, end;
        std::chrono::duration<double> elapsed_seconds(3);
        start = std::chrono::system_clock::now();
        while (!timeout) {
            if (poll(pollStruct, 1, 1000) == 1) {
                int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len);
                AcknowledgeHeader* a_header = reinterpret_cast<AcknowledgeHeader*>(buffer + sizeof(ProtocolHeader));
                client_id = a_header->client_id;
                ack_received = true;
                std::cout << "Ack received, client_id = " << client_id << "\n";
                sleep(3);
                break;
            } else {
                end = std::chrono::system_clock::now();
                if (end - start >= elapsed_seconds) {
                    std::cout << "Stop waiting\n";
                    return 0;
                    break;
                }
                sleep(1);
                std::cout << "No response message\n";
            }

        }

        // Send message to server
        RequestHeader r_header;
        r_header.client_id = client_id;
        r_header.value = 12345678;
        PrepareDataToSend(r_header, MessageType::REQUEST);

        /*header.packet_number = packet_num;
        header.packets_total = 1;
        header.type = MessageType::REQUEST;
        header.data_size = strlen(buffer);
        
        char* mbuffer = new char[sizeof(ProtocolHeader) + strlen(buffer)];
        memcpy(mbuffer, &header, sizeof(ProtocolHeader));
        memcpy(mbuffer + sizeof(ProtocolHeader), buffer, strlen(buffer));
        //header.data = ;
        sleep(3);
        std::cout << "Send message, size = " << sizeof(ProtocolHeader) + strlen(buffer) << "\n";
        if (sendto(sockfd, mbuffer, sizeof(ProtocolHeader) + strlen(buffer), 0, (struct sockaddr *)&server_addr,
            #ifdef _WIN32
            sizeof(server_addr)) == SOCKET_ERROR
            #else
            sizeof(server_addr)) < 0
            #endif
            ) {
                std::cerr << "Error sending message\n";
            #ifdef _WIN32
            closesocket(sockfd);
            WSACleanup();
            #else
            close(sockfd);
            #endif
            return -1;
        }
        std::cout << "Message sent\n";
        delete mbuffer;

        // Receive response from server
        
        std::cout << "waiting for message" << std::endl;
        /*while(true) { 
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len);
            memcpy(arr.data(), buffer, n);
            std::cout << "Server response: " << arr[20] << std::endl;
        }
        while (true) {
            pollStruct[0].fd = sockfd;
            pollStruct[0].events = POLLIN;
            //pollStruct[0].revents = 0;
            ack_received = false;
            //std::cout << "Array size: " << arr.size() << "\n";
            if (poll(pollStruct, 1, 5) == 1) {
                int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len);
                rheader = reinterpret_cast<ProtocolHeader*>(buffer);
                switch (rheader->type) {
                    case MessageType::ERROR: {
                        break;
                    }
                    case MessageType::REQUEST: {
                        break;
                    }
                    case MessageType::ACKNOWLEDGE: {
                        AcknowledgeHeader* a_header = reinterpret_cast<AcknowledgeHeader*>(buffer + sizeof(ProtocolHeader));
                        client_id = a_header->client_id;
                        ack_received = true;
                        std::cout << "Ack received, client_id = " << client_id << "\n";
                        break;
                    }
                    case MessageType::RESPONSE: {
                        //ProcessResponse();
                        break;
                    }
                    case MessageType::MISSED_PACKETS: {
                        break;
                    } default: {
                        break;
                    }
                }
                if (ack_received == true) {
                    continue;
                }
                ++counter;
                //std::cout << "packets total: " << rheader->packets_total << "\n";
                if (counter == 1) {
                    arr.resize(rheader->packets_total * BUFFER_SIZE);
                    packets_received.resize(rheader->packets_total + 1);
                }
                offset = BUFFER_SIZE * (rheader->packet_number - 1);
                memcpy(arr.data() + (offset/sizeof(double)), buffer, n);
                packets_received[rheader->packet_number] = true;
                std::cout << "On " << offset << " Received bytes" << n << " Packet number" << rheader->packet_number << std::endl;
                //std::cout << "Server response: " << arr[10] << std::endl;
                //memcpy(arr.data() + offset, buffer, n);
                if (rheader->packets_total == rheader->packet_number) {
                    break;
                }
            }
            end = std::chrono::system_clock::now();
            if (end - start == elapsed_seconds) {
                break;
            }
        }
        std::vector<unsigned short> missed_packets;
        for(short i = 1; i < packets_received.size(); ++i) {
            if(packets_received[i] == false) {
                missed_packets.push_back(i);
            }
        }
        if (missed_packets.size() != 0) {
            ProtocolHeader p_header;
            p_header.packet_number = 1;
            p_header.packets_total = 1;
            p_header.type = MessageType::MISSED_PACKETS;
            p_header.data_size = missed_packets.size() * sizeof(unsigned short) + sizeof(MissedPacketsHeader);
            MissedPacketsHeader m_header;
            m_header.total_packets_missed = missed_packets.size();
            m_header.client_id = client_id;

            mbuffer = new char[sizeof(ProtocolHeader) + p_header.data_size];
            memcpy(mbuffer, &p_header, sizeof(ProtocolHeader));
            memcpy(mbuffer + sizeof(ProtocolHeader), &m_header, sizeof(MissedPacketsHeader));
            memcpy(mbuffer + sizeof(ProtocolHeader) + sizeof(MissedPacketsHeader), missed_packets.data(), missed_packets.size() * sizeof(unsigned short));

            if (sendto(sockfd, mbuffer, sizeof(ProtocolHeader) + strlen(buffer), 0, (struct sockaddr *)&server_addr,
                #ifdef _WIN32
                sizeof(server_addr)) == SOCKET_ERROR
                #else
                sizeof(server_addr)) < 0
                #endif
                ) {
                    std::cerr << "Error sending message\n";
                #ifdef _WIN32
                closesocket(sockfd);
                WSACleanup();
                #else
                close(sockfd);
                #endif
                return -1;
            }
                std::cout << "Message sent\n";
                delete mbuffer;
        } else {
            ProtocolHeader p_header;
            p_header.packet_number = 1;
            p_header.packets_total = 1;
            p_header.type = MessageType::ACKNOWLEDGE;
            p_header.data_size = sizeof(AcknowledgeHeader);
            AcknowledgeHeader a_header;
            a_header.client_id = client_id;

            mbuffer = new char[sizeof(ProtocolHeader) + p_header.data_size];
            memcpy(mbuffer, &p_header, sizeof(ProtocolHeader));
            memcpy(mbuffer + sizeof(ProtocolHeader), &a_header, sizeof(AcknowledgeHeader));

            if (sendto(sockfd, mbuffer, sizeof(ProtocolHeader) + strlen(buffer), 0, (struct sockaddr *)&server_addr,
                #ifdef _WIN32
                sizeof(server_addr)) == SOCKET_ERROR
                #else
                sizeof(server_addr)) < 0
                #endif
                ) {
                    std::cerr << "Error sending message\n";
                #ifdef _WIN32
                closesocket(sockfd);
                WSACleanup();
                #else
                close(sockfd);
                #endif
                return -1;
            }
                std::cout << "Message sent\n";
                delete mbuffer;
        }

        std::cout << "packets received " << counter << std::endl;
    }

    #ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
    #else
    close(sockfd);
    #endif*/

    return 0;
}

    /*std::cout << "Enter message: ";
    char buffer2[10] = "123456789";
    std::cout << "send message" << "\n";

    // Send message to server
    if (sendto(sockfd, buffer2, strlen(buffer), 0, (struct sockaddr *)&server_addr,
        #ifdef _WIN32
        sizeof(server_addr)) == SOCKET_ERROR
        #else
        sizeof(server_addr)) < 0
        #endif
        ) {
        std::cerr << "Error sending message\n";
        #ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
        #else
        close(sockfd);
        #endif
        return -1;
    }

        // Receive response from server
    double arr[1000000];
    unsigned int len = sizeof(server_addr);
    int offset = 0;
    int counter = 0;
    while (true) {
            int n = recvfrom(sockfd, buffer, 10240, 0, (struct sockaddr *)&server_addr, &len);
        ++counter;
        std::cout << "On " << offset << " Received bytes" << n << " Received total packets" << counter << std::endl;
        //memcpy(arr + offset, buffer, n);
        //offset += n;
        /*for(int i = 0; i < 1024 / sizeof(double); ++i ) {
            v.push_back(*(reinterpret_cast<double*>(&buffer[offset])));
            offset += sizeof(double);
        }
        //buffer[n] = '\0';
        //std::cout << v.size() << std::endl;
        if (n < 10240) {
            break;
        }
    }
    std::cout << "packets received " << counter << std::endl;*/

    /*for(int i = 100; i < 105; ++i) {
        std::cout << arr[i] << std::endl;
    }

    #ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
    #else
    close(sockfd);
    #endif

    return 0;
}*/