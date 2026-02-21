#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/sampler.hpp"
#include "derived/io_pressure.hpp"
#include "derived/latency_jitter.hpp"
#include "derived/memory_pressure.hpp"
#include "derived/power_pressure.hpp"
#include "derived/scheduler_pressure.hpp"
#include "derived/thermal_pressure.hpp"
#include "model/signal_frame.hpp"
#include "risk/realtime_risk.hpp"
#include "risk/saturation_risk.hpp"
#include "risk/system_state.hpp"
#include "sensors/cpu.hpp"
#include "sensors/cpufreq.hpp"
#include "sensors/gpu/gpu.hpp"
#include "sensors/disk.hpp"
#include "sensors/interrupts.hpp"
#include "sensors/memory.hpp"
#include "sensors/network.hpp"
#include "sensors/power.hpp"
#include "sensors/psi.hpp"
#include "sensors/softirqs.hpp"
#include "sensors/tegrastats.hpp"
#include "sensors/thermal.hpp"
#include "sinks/redis_ts.hpp"
#include "sinks/stdout_debug.hpp"

namespace hw_agent::core {

struct AgentStats {
  std::size_t ticks_executed{0};
  std::size_t sensor_cycles{0};
  std::size_t derived_cycles{0};
  std::size_t risk_cycles{0};
  std::size_t sink_cycles{0};
};

class Agent {
 public:
  explicit Agent(AgentConfig config = {});

  AgentStats run_for_ticks(std::size_t total_ticks);

 private:
  struct SensorRegistration {
    std::string name;
    std::uint64_t every_ticks;
    bool enabled;
    std::function<bool(model::signal_frame&)> sample;
  };

  void register_sensors(const AgentConfig& config);
  [[nodiscard]] bool sensor_enabled(const AgentConfig& config, const std::string& name) const;
  void collect_sensors(AgentStats& stats);
  void compute_derived(AgentStats& stats);
  void compute_risk(AgentStats& stats);
  void publish_sinks(AgentStats& stats);
  void update_agent_health(float actual_period_ms, float compute_time_ms);

  std::chrono::milliseconds tick_interval_{};
  std::chrono::steady_clock::time_point next_wakeup_{};
  bool first_tick_{true};
  std::optional<std::chrono::steady_clock::time_point> previous_cycle_start_{};
  Sampler sampler_{};
  model::signal_frame frame_{};
  bool publish_health_{true};
  bool publish_stdout_{true};
  std::vector<SensorRegistration> sensor_registry_{};

  sensors::PsiSensor psi_sensor_{};
  sensors::CpuSensor cpu_sensor_{};
  sensors::InterruptsSensor interrupts_sensor_{};
  sensors::SoftirqsSensor softirqs_sensor_{};
  sensors::MemorySensor memory_sensor_{};
  sensors::DiskSensor disk_sensor_{};
  sensors::NetworkSensor network_sensor_{};
  sensors::TegraStatsSensor tegrastats_sensor_{};
  sensors::ThermalSensor thermal_sensor_{};
  sensors::PowerSensor power_sensor_{};
  sensors::CpuFreqSensor cpufreq_sensor_{};
  std::unique_ptr<sensors::gpu::GpuSensor> gpu_sensor_{};

  derived::SchedulerPressure scheduler_pressure_{};
  derived::MemoryPressure memory_pressure_{};
  derived::IoPressure io_pressure_{};
  derived::ThermalPressure thermal_pressure_{};
  derived::PowerPressure power_pressure_{};
  derived::LatencyJitter latency_jitter_{};

  risk::RealtimeRisk realtime_risk_{};
  risk::SaturationRisk saturation_risk_{};
  risk::SystemState system_state_{};

  sinks::StdoutDebugSink stdout_sink_{};
  std::unique_ptr<sinks::RedisTsSink> redis_sink_{};
  bool redis_was_ok_{true};
};

}  // namespace hw_agent::core
