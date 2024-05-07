#include "ConfReader.hpp"
#include <iostream>

using json = nlohmann::json_abi_v3_11_3::json;

ServerConfig::ServerConfig(const int& port, const std::string& ip, const double& val)
    : server_port(port)
    , server_ip(ip)
    , value(val) {}

ConfReader::ConfReader(const std::string& path = "./") : path_(path) {}

ServerConfig ConfReader::ReadServerConfig() {
    std::ifstream f(path_ + "clientconf.json");
    json data = json::parse(f);
    std::cout << "Port: " << data["port"] << "\n";
    ServerConfig conf(data["port"], data["ip"], data["value"]);
    return conf;
}