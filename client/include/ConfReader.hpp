#ifndef CONF_READER_HPP
#define CONF_READER_HPP

#include <fstream>
#include "json.hpp"

struct ServerConfig {
    ServerConfig() {}
    ServerConfig(const int& port, const std::string& ip, const double& val);
    int server_port;
    std::string server_ip;
    double value;
};

class ConfReader {
public:
    ConfReader(const std::string& path);
    ServerConfig ReadServerConfig();
private:
    std::string path_;
};

#endif // CONF_READER_HPP