#include "ConfReader.hpp"
#include <iostream>

using json = nlohmann::json_abi_v3_11_3::json;

ServerConfig::ServerConfig(const int& port) : server_port(port) {}

ProtocolConfig::ProtocolConfig(const unsigned int& val_amount)
    : values_amount(val_amount) {}

ConfReader::ConfReader(const std::string& path = "./") : path_(path) {}

ServerConfig ConfReader::ReadServerConfig() {
    std::ifstream f(path_ + "serverconf.json");
    json data = json::parse(f);
    std::cout << "Port: " << data["port"] << "\n";
    ServerConfig conf(data["port"]);
    return conf;
}

ProtocolConfig ConfReader::ReadProtocolConfig() {
    std::ifstream f(path_ + "protocolconf.json");
    json data = json::parse(f);
    ProtocolConfig conf(data["value_amount"]);
    return conf;
}