#include "driver.h"

#include <chrono>
#include <iostream>
#include <string>

int main(int argv, char** argc) {
    if (argv != 5) {
        std::cout << "Usage: ./client <ip> <port> <packets> <packet_size>" << std::endl;
        return 1;
    }
    std::string ip(argc[1]);
    uint64_t port = atoi(argc[2]);
    uint64_t packets = atoll(argc[3]);
    uint64_t packet_size = atoll(argc[4]);
    auto start = std::chrono::system_clock::now();
    send_data(ip, port, packets, packet_size);
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<uint64_t, std::nano> diff = end - start;
    result_t r;
    r.nanos = diff.count();
    std::cout << "Nanos: " << r.nanos << std::endl;
    double seconds = (r.nanos / 1e9);
    std::cout << "Seconds: " << seconds << std::endl;
    double BPS = (double(packets) * double(packet_size)) / seconds;
    std::cout << "Bytes per second: ";
    if (BPS >= 1000000000) {
        std::cout << (BPS / 1000000000) << "GBps" << std::endl;
    } else if (BPS >= 1000000) {
        std::cout << (BPS / 1000000) << "MBps" << std::endl;
    } else {
        std::cout << BPS << "Bps" << std::endl;
    }
}