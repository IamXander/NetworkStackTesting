#include "driver.h"

#include <chrono>
#include <iostream>

int main() {
    auto start = std::chrono::system_clock::now();
    send_data("127.0.0.1", 8080, 10000000, 1);
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<uint64_t, std::nano> diff = end - start;
    result_t r;
    r.nanos = diff.count();
    std::cout << "Nanos: " << r.nanos << std::endl;
    double seconds = (r.nanos / 1e9);
    std::cout << "Seconds: " << seconds << std::endl;
    std::cout << "Bytes per second: " << (double(10000000) * 10000) / seconds << std::endl;
}