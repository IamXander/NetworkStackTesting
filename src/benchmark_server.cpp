#include "driver.h"
#include "cpu/cpu.h"

#include <thread>
#include <atomic>
#include <iostream>

void cpu_poll_loop(std::atomic<bool>& poll) {
    std::array<core_usage_t, CORE_COUNT> cores;
    while (poll) {
        poll_cpu_usage(cores, 1000);
    }
}

int main(int argv, char** argc) {
    if (argv != 4) {
        std::cout << "Usage: ./server <port> <packets> <packet_size>" << std::endl;
        return 1;
    }
    uint64_t port = atoi(argc[1]);
    uint64_t packets = atoll(argc[2]);
    uint64_t packet_size = atoll(argc[3]);
    std::atomic<bool> poll(true);
    std::thread poll_thread(cpu_poll_loop, std::ref(poll));
    recv_data(port, packets, packet_size);
    poll = false;
    poll_thread.join();
}