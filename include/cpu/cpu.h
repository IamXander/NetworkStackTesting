<<<<<<< HEAD
#ifndef INCLUDE_CPU_CPU_H
#define INCLUDE_CPU_CPU_H

#include <cstdint>
#include <array>

#ifndef CORE_COUNT
static_assert(false, "Must define CORE_COUNT");
#endif

struct core_usage_t {
    uint64_t idle_time;
    uint64_t active_time;
    double utilization;
};

//Read /proc/stat and load statistics
void poll_cpu_usage(std::array<core_usage_t, CORE_COUNT>& cores, uint64_t milliseconds);

=======
#ifndef INCLUDE_CPU_CPU_H
#define INCLUDE_CPU_CPU_H

#include <cstdint>
#include <array>

#ifndef CORE_COUNT
static_assert(false, "Must define CORE_COUNT");
#endif

struct core_usage_t {
    uint64_t idle_time;
    uint64_t active_time;
    double utilization;
};

//Read /proc/stat and load statistics
void poll_cpu_usage(std::array<core_usage_t, CORE_COUNT>& cores, uint64_t milliseconds);

>>>>>>> aa0fd0478e28e500b738278cef83bebc5e426b3d
#endif