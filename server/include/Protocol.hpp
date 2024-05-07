#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

constexpr unsigned int PROTOCOL_VERSION_MAJOR = 1;
constexpr unsigned int PROTOCOL_VERSION_MINOR = 0; 

enum class MessageType {
    ERROR_CODE = 0,
    REQUEST = 1,
    ACKNOWLEDGE = 2,
    RESPONSE = 3,
    MISSED_PACKETS = 4,
    CONNECT = 5
};

enum class ErrorCode {
    INVALID_VERSION = 0,
    INVALID_VALUE = 1,
    INVALID_HEADER = 2
};

struct ErrorHeader {
    ErrorCode error : 8;
    unsigned int version_major : 8;
    unsigned int version_minor : 8;
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
    unsigned int client_id : 8;
    double value;
};

struct AcknowledgeHeader {
    unsigned int client_id : 8;
    unsigned int received_packet_number;
};

struct ResponseHeader {
    char* data;
};

struct MissedPacketsHeader {
    unsigned int client_id : 8;
    unsigned int total_packets_missed : 16;
};

#endif // PROTOCOL_HPP