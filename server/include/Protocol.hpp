#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

constexpr uint32_t PROTOCOL_VERSION_MAJOR = 1;
constexpr uint32_t PROTOCOL_VERSION_MINOR = 0; 

enum class MessageType : uint8_t {
    ERROR_CODE = 0,
    REQUEST = 1,
    ACKNOWLEDGE = 2,
    RESPONSE = 3,
    MISSED_PACKETS = 4,
    CONNECT = 5
};

enum class ErrorCode : uint8_t {
    INVALID_VERSION = 0,
    INVALID_VALUE = 1,
    INVALID_HEADER = 2
};

struct ErrorHeader {
    ErrorCode error;
    uint8_t version_major;
    uint8_t version_minor;
};

struct ConnectHeader {
    uint8_t version_major;
    uint8_t version_minor;
};

struct ProtocolHeader {
    uint16_t packet_number;
    uint16_t packets_total;
    uint16_t data_size;
    MessageType type;
};

struct RequestHeader {
    uint8_t client_id;
    double value;
};

struct AcknowledgeHeader {
    uint8_t client_id;
    uint16_t received_packet_number;
};

struct ResponseHeader {
    char* data;
};

struct MissedPacketsHeader {
    uint8_t client_id;
    uint16_t total_packets_missed;
};

#endif // PROTOCOL_HPP