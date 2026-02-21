#include "core/agent.hpp"

#include <thread>

#include "core/timestamp.hpp"

namespace hw_agent::core {

Agent::Agent(AgentConfig config)
    : tick_interval_(config.tick_interval), thermal_sensor_(config.thermal_throttle_temp_c) {
  if (config.redis.enabled) {
    sinks::RedisTsOptions options{};
    options.host = config.redis.host;
    options.port = config.redis.port;
    redis_sink_ = std::make_unique<sinks::RedisTsSink>(options);
  }

  register_sensors(config);
}

AgentStats Agent::run_for_ticks(const std::size_t total_ticks) {
  AgentStats stats{};

  auto next_wakeup = std::chrono::steady_clock::now();
  for (std::size_t i = 0; i < total_ticks; ++i) {
    collect_sensors(stats);
    compute_derived(stats);
    compute_risk(stats);
    publish_sinks(stats);

    ++stats.ticks_executed;
    sampler_.advance();

    next_wakeup += tick_interval_;
    std::this_thread::sleep_until(next_wakeup);
  }

  return stats;
}

void Agent::register_sensors(const AgentConfig& config) {
  sensor_registry_.push_back({"psi", 1, sensor_enabled(config, "psi"), [this](model::signal_frame& frame) { psi_sensor_.sample(frame); }});
  sensor_registry_.push_back({"cpu", 2, sensor_enabled(config, "cpu"), [this](model::signal_frame& frame) { cpu_sensor_.sample(frame); }});
  sensor_registry_.push_back({"interrupts", 3, sensor_enabled(config, "interrupts"), [this](model::signal_frame& frame) { interrupts_sensor_.sample(frame); }});
  sensor_registry_.push_back({"softirqs", 4, sensor_enabled(config, "softirqs"), [this](model::signal_frame& frame) { softirqs_sensor_.sample(frame); }});
  sensor_registry_.push_back({"memory", 5, sensor_enabled(config, "memory"), [this](model::signal_frame& frame) { memory_sensor_.sample(frame); }});
  sensor_registry_.push_back({"disk", 6, sensor_enabled(config, "disk"), [this](model::signal_frame& frame) { disk_sensor_.sample(frame); }});
  sensor_registry_.push_back({"network", 7, sensor_enabled(config, "network"), [this](model::signal_frame& frame) { network_sensor_.sample(frame); }});
  sensor_registry_.push_back({"tegrastats", 8, sensor_enabled(config, "tegrastats"), [this](model::signal_frame& frame) { tegrastats_sensor_.sample(frame); }});
  sensor_registry_.push_back({"thermal", 9, sensor_enabled(config, "thermal"), [this](model::signal_frame& frame) { thermal_sensor_.sample(frame); }});
  sensor_registry_.push_back({"power", 10, sensor_enabled(config, "power"), [this](model::signal_frame& frame) { power_sensor_.sample(frame); }});
  sensor_registry_.push_back({"cpufreq", 11, sensor_enabled(config, "cpufreq"), [this](model::signal_frame& frame) { cpufreq_sensor_.sample(frame); }});
}

bool Agent::sensor_enabled(const AgentConfig& config, const std::string& name) const {
  const auto it = config.sensor_enabled.find(name);
  if (it == config.sensor_enabled.end()) {
    return true;
  }
  return it->second;
}

void Agent::collect_sensors(AgentStats& stats) {
  ++stats.sensor_cycles;
  frame_.timestamp = timestamp_now_ns();

  for (auto& sensor : sensor_registry_) {
    if (!sensor.enabled) {
      continue;
    }

    if (sampler_.should_sample_every(sensor.every_ticks)) {
      sensor.sample(frame_);
    }
  }
}

void Agent::compute_derived(AgentStats& stats) {
  ++stats.derived_cycles;
  scheduler_pressure_.sample(frame_);
  memory_pressure_.sample(frame_);
  io_pressure_.sample(frame_);
  thermal_pressure_.sample(frame_);
  power_pressure_.sample(frame_);
  latency_jitter_.sample(frame_);
}

void Agent::compute_risk(AgentStats& stats) {
  ++stats.risk_cycles;
  realtime_risk_.sample(frame_);
  saturation_risk_.sample(frame_);
  system_state_.sample(frame_);
}

void Agent::publish_sinks(AgentStats& stats) {
  ++stats.sink_cycles;
  stdout_sink_.publish(frame_);
  if (redis_sink_ != nullptr) {
    redis_sink_->publish(frame_);
  }
}

}  // namespace hw_agent::core
