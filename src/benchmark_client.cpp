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
    double seconds = (r.nanos / 1e9);
	uint64_t bytesSent = packets * packet_size;
	std::cout << "Total data sent: ";
	if (bytesSent >= 1000000000) {
        std::cout << (bytesSent / 1000000000) << "GB" << std::endl;
    } else if (bytesSent >= 1000000) {
        std::cout << (bytesSent / 1000000) << "MB" << std::endl;
    } else if (bytesSent >= 1000) {
		std::cout << (bytesSent / 1000) << "KB" << std::endl;
	} else {
        std::cout << bytesSent << "B" << std::endl;
    }
    std::cout << "Total duration: " << seconds << " seconds" << std::endl;
    double BPS = double(bytesSent) * 8 / seconds;
    std::cout << "Bits per second: ";
    if (BPS >= 1000000000) {
        std::cout << (BPS / 1000000000) << "Gbps" << std::endl;
    } else if (BPS >= 1000000) {
        std::cout << (BPS / 1000000) << "Mbps" << std::endl;
    } else if (BPS >= 1000) {
		std::cout << (BPS / 1000) << "Kbps" << std::endl;
	} else {
        std::cout << BPS << "bps" << std::endl;
    }
}