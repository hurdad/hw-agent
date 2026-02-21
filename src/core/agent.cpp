#include "core/agent.hpp"

#include <cmath>
#include <thread>

#include "core/timestamp.hpp"

namespace hw_agent::core {

Agent::Agent(AgentConfig config)
    : tick_interval_(config.tick_interval), publish_health_(config.publish_health), thermal_sensor_(config.thermal_throttle_temp_c) {
  if (config.redis.enabled) {
    sinks::RedisTsOptions options{};
    options.host = config.redis.host;
    options.port = config.redis.port;
    options.publish_health = config.publish_health;
    redis_sink_ = std::make_unique<sinks::RedisTsSink>(options);
  }

  register_sensors(config);
}

AgentStats Agent::run_for_ticks(const std::size_t total_ticks) {
  AgentStats stats{};

  auto next_wakeup = std::chrono::steady_clock::now();
  auto previous_cycle_start = next_wakeup;
  for (std::size_t i = 0; i < total_ticks; ++i) {
    const auto cycle_start = std::chrono::steady_clock::now();

    collect_sensors(stats);
    compute_derived(stats);
    compute_risk(stats);
    publish_sinks(stats);

    const auto cycle_end = std::chrono::steady_clock::now();
    const auto actual_period_ms = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(cycle_start - previous_cycle_start).count();
    const auto compute_ms = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(cycle_end - cycle_start).count();
    update_agent_health(actual_period_ms, compute_ms);
    previous_cycle_start = cycle_start;

    ++stats.ticks_executed;
    sampler_.advance();

    next_wakeup += tick_interval_;
    std::this_thread::sleep_until(next_wakeup);
  }

  return stats;
}

void Agent::register_sensors(const AgentConfig& config) {
  sensor_registry_.push_back({"psi", 1, sensor_enabled(config, "psi"), [this](model::signal_frame& frame) { psi_sensor_.sample(frame); return true; }});
  sensor_registry_.push_back({"cpu", 2, sensor_enabled(config, "cpu"), [this](model::signal_frame& frame) { cpu_sensor_.sample(frame); return true; }});
  sensor_registry_.push_back({"interrupts", 3, sensor_enabled(config, "interrupts"), [this](model::signal_frame& frame) { interrupts_sensor_.sample(frame); return true; }});
  sensor_registry_.push_back({"softirqs", 4, sensor_enabled(config, "softirqs"), [this](model::signal_frame& frame) { softirqs_sensor_.sample(frame); return true; }});
  sensor_registry_.push_back({"memory", 5, sensor_enabled(config, "memory"), [this](model::signal_frame& frame) { memory_sensor_.sample(frame); return true; }});
  sensor_registry_.push_back({"disk", 6, sensor_enabled(config, "disk"), [this](model::signal_frame& frame) { disk_sensor_.sample(frame); return true; }});
  sensor_registry_.push_back({"network", 7, sensor_enabled(config, "network"), [this](model::signal_frame& frame) { network_sensor_.sample(frame); return true; }});
  sensor_registry_.push_back({"tegrastats", 8, sensor_enabled(config, "tegrastats"), [this](model::signal_frame& frame) { tegrastats_sensor_.sample(frame); return true; }});
  sensor_registry_.push_back({"thermal", 9, sensor_enabled(config, "thermal"), [this](model::signal_frame& frame) { thermal_sensor_.sample(frame); return true; }});
  sensor_registry_.push_back({"power", 10, sensor_enabled(config, "power"), [this](model::signal_frame& frame) { power_sensor_.sample(frame); return true; }});
  sensor_registry_.push_back({"cpufreq", 11, sensor_enabled(config, "cpufreq"), [this](model::signal_frame& frame) { cpufreq_sensor_.sample(frame); return true; }});
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
      if (!sensor.sample(frame_)) {
        ++frame_.agent.sensor_failures;
      }
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

void Agent::update_agent_health(const float actual_period_ms, const float compute_time_ms) {
  if (!publish_health_) {
    return;
  }

  const auto tick_ms = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(tick_interval_).count();

  frame_.agent.loop_jitter_ms = std::fabs(actual_period_ms - tick_ms);
  frame_.agent.compute_time_ms = compute_time_ms;
  frame_.agent.heartbeat_ms = timestamp_now_ns() / 1'000'000ULL;
  if (compute_time_ms > tick_ms) {
    ++frame_.agent.missed_cycles;
  }
}

}  // namespace hw_agent::core
