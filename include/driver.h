#ifndef DRIVER_H
#define DRIVER_H

#include <cstdint>
#include <string>

struct result_t {
    bool success;
    uint64_t data_sent;
    uint64_t packets_sent;
    uint64_t nanos;
    uint64_t bps;
};

int send_data(std::string ip, uint16_t port, uint64_t packets, uint64_t packet_size);
result_t recv_data(uint16_t port, uint64_t packets, uint64_t packet_size);

#endif