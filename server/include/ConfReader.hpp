#ifndef CONF_READER_HPP
#define CONF_READER_HPP

#include <fstream>
#include "json.hpp"

struct ServerConfig {
    ServerConfig() {}
    ServerConfig(const int& port);
    int server_port;
};

struct ProtocolConfig {
    ProtocolConfig() {}
    ProtocolConfig(const int& ver_maj, const int& ver_min, const unsigned int& val_amount);
    int version_major;
    int version_minor;
    unsigned int values_amount;
};

class ConfReader {
public:
    ConfReader(const std::string& path);
    ServerConfig ReadServerConfig();
    ProtocolConfig ReadProtocolConfig();
private:
    std::string path_;
};

#endif // CONF_READER_HPP