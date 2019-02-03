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
    if (argv != 2) {
        std::cout << "Usage: ./server <port>" << std::endl;
        return 1;
    }
    uint64_t port = atoi(argc[1]);
    std::atomic<bool> poll(true);
    std::thread poll_thread(cpu_poll_loop, std::ref(poll));
    recv_data(port);
    poll = false;
    poll_thread.join();
}