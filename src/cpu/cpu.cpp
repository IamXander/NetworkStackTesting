#include "cpu/cpu.h"

#include <fstream>
#include <iostream>
#include <numeric>
#include <unistd.h>
#include <vector>
#include <cassert>
#include <chrono>
#include <thread>
 
std::vector<uint64_t> get_cpu_times() {
    std::ifstream proc_stat("/proc/stat");
    while (proc_stat.get() != ' ');
    std::vector<uint64_t> times;
    for (uint64_t time; proc_stat >> time; times.push_back(time));
    return times;
}
 
void get_cpu_times(uint64_t &idle_time, uint64_t &total_time) {
    const std::vector<uint64_t> cpu_times = get_cpu_times();
    assert(cpu_times.size() >= 4);
    idle_time = cpu_times[3];
    total_time = std::accumulate(cpu_times.begin(), cpu_times.end(), 0);
}

void poll_cpu_usage(std::array<core_usage_t, CORE_COUNT>& cores, uint64_t milliseconds) {
    uint64_t prev_idle_time, prev_total_time;
    get_cpu_times(prev_idle_time, prev_total_time);
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    uint64_t new_idle_time, new_total_time;
    get_cpu_times(new_idle_time, new_total_time);

    std::cout << "Idle percent of time: " << double(new_idle_time - prev_idle_time) / double(new_total_time - prev_total_time) << std::endl;
}