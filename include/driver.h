#ifndef DRIVER_H
#define DRIVER_H

#include <cstdint>

struct result_t {
    bool success;
    uint64_t data_sent;
    uint64_t packets_sent;
    uint64_t nanos;
    uint64_t bps;
};

result_t send_data(); 

#endif