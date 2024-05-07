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

#include "Client.hpp"

int main() {
    Client client;

    return 0;
}