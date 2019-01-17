// #include "driver.h"
#include "cpu/cpu.h"

#include <thread>
#include <atomic>

void cpu_poll_loop(std::atomic<bool>& poll) {
    std::array<core_usage_t, CORE_COUNT> cores;
    while (poll) {
        poll_cpu_usage(cores, 1000);
    }
}

int main() {
    std::atomic<bool> poll = true;
    std::thread poll_thread(cpu_poll_loop, std::ref(poll));
    poll_thread.join();
}