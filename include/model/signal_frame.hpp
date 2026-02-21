#pragma once

#include <cstdint>
#include <type_traits>

namespace hw_agent::model {

enum class system_state : std::uint8_t {
    STABLE = 0,
    DEGRADED = 1,
    UNSTABLE = 2,
    CRITICAL = 3,
};

// ABI frame shared across modules.
// POD layout: one timestamp + tightly packed float signals.
struct signal_frame {
    struct AgentHealth {
        std::uint64_t heartbeat_ms;
        float loop_jitter_ms;
        float compute_time_ms;
        float redis_latency_ms;
        std::uint32_t sensor_failures;
        std::uint32_t missed_cycles;
    };

    std::uint64_t timestamp;

    // Raw signals.
    float psi;
    float cpu;
    float irq;
    float memory;
    float thermal;
    float power;
    float disk;
    float network;
    float gpu_util;
    float gpu_mem_util;
    float gpu_mem_free;
    float gpu_temp;
    float gpu_clock_ratio;
    float gpu_power_ratio;
    float gpu_throttle;

    // Derived signals.
    float scheduler_pressure;
    float memory_pressure;
    float io_pressure;
    float thermal_pressure;
    float power_pressure;
    float latency_jitter;

    // Risk signals.
    float realtime_risk;
    float saturation_risk;

    system_state state;

    AgentHealth agent;
};

static_assert(std::is_standard_layout_v<signal_frame>, "signal_frame must be standard layout");
static_assert(std::is_trivial_v<signal_frame>, "signal_frame must be trivial");

} // namespace hw_agent::model
