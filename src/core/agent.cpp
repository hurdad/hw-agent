#include "core/agent.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "core/timestamp.hpp"

namespace hw_agent::core {
namespace {

bool is_sensor_enabled(const AgentConfig& config, const std::string& name) {
  const auto it = config.sensor_enabled.find(name);
  if (it == config.sensor_enabled.end()) {
    return true;
  }
  return it->second;
}

std::vector<std::string> enabled_redis_metrics(const AgentConfig& config) {
  std::vector<std::string> metrics = {
      "derived:scheduler_pressure", "derived:memory_pressure", "derived:io_pressure",   "derived:thermal_pressure",
      "derived:power_pressure",    "derived:latency_jitter",  "risk:realtime_risk",     "risk:saturation_risk",
      "risk:state",
  };

  if (is_sensor_enabled(config, "psi")) {
    metrics.push_back("raw:psi");
    metrics.push_back("raw:psi_memory");
    metrics.push_back("raw:psi_io");
  }
  if (is_sensor_enabled(config, "cpu")) {
    metrics.push_back("raw:cpu");
  }
  if (is_sensor_enabled(config, "interrupts")) {
    metrics.push_back("raw:irq");
  }
  if (is_sensor_enabled(config, "softirqs")) {
    metrics.push_back("raw:softirqs");
  }
  if (is_sensor_enabled(config, "memory")) {
    metrics.push_back("raw:memory");
  }
  if (is_sensor_enabled(config, "thermal")) {
    metrics.push_back("raw:thermal");
  }
  if (is_sensor_enabled(config, "cpufreq")) {
    metrics.push_back("raw:cpufreq");
  }
  if (is_sensor_enabled(config, "cpu_throttle")) {
    metrics.push_back("raw:cpu_throttle_ratio");
  }
  if (is_sensor_enabled(config, "disk")) {
    metrics.push_back("raw:disk");
  }
  if (is_sensor_enabled(config, "network")) {
    metrics.push_back("raw:network");
  }

  const bool gpu_enabled = is_sensor_enabled(config, "gpu");
  const bool tegrastats_enabled = is_sensor_enabled(config, "tegrastats");
  if (gpu_enabled) {
    metrics.push_back("raw:nvml_gpu_util");
    metrics.push_back("raw:gpu_mem_util");
    metrics.push_back("raw:nvml_gpu_temp");
    metrics.push_back("raw:nvml_gpu_power_ratio");
  }
  if (tegrastats_enabled) {
    metrics.push_back("raw:tegra_gpu_util");
    metrics.push_back("raw:tegra_gpu_temp");
    metrics.push_back("raw:tegra_gpu_power_mw");
  }
  if (tegrastats_enabled) {
    metrics.push_back("raw:tegra_emc_util");
  }

  if (config.publish_health) {
    metrics.push_back("agent:heartbeat");
    metrics.push_back("agent:loop_jitter");
    metrics.push_back("agent:compute_time");
    metrics.push_back("agent:redis_latency");
    metrics.push_back("agent:redis_errors");
    metrics.push_back("agent:sensor_failures");
    metrics.push_back("agent:missed_cycles");
  }

  return metrics;
}

}  // namespace

Agent::Agent(AgentConfig config)
    : tick_interval_(config.tick_interval),
      publish_health_(config.publish_health),
      publish_stdout_(config.stdout_debug),
      thermal_sensor_(config.thermal_throttle_temp_c),
      thermal_pressure_(config.thermal_pressure_warning_window_c) {
  if (config.redis.enabled) {
    sinks::RedisTsOptions options{};
    options.host = config.redis.host;
    options.port = config.redis.port;
    options.unix_socket = config.redis.unix_socket;
    options.publish_health = config.publish_health;
    options.enabled_metrics = enabled_redis_metrics(config);
    redis_sink_ = std::make_unique<sinks::RedisTsSink>(options);

    if (redis_sink_->check_connectivity()) {
      if (!options.unix_socket.empty()) {
        std::cerr << "[agent] redis connectivity confirmed at unix://" << options.unix_socket << '\n';
      } else {
        std::cerr << "[agent] redis connectivity confirmed at " << options.host << ':' << options.port << '\n';
      }
    } else {
      if (!options.unix_socket.empty()) {
        std::cerr << "[agent] redis connectivity check failed at unix://" << options.unix_socket << '\n';
      } else {
        std::cerr << "[agent] redis connectivity check failed at " << options.host << ':' << options.port << '\n';
      }
    }
  }

  gpu_sensor_ = sensors::gpu::make_nvml_sensor(config.gpu_device_index);
  if (gpu_sensor_ != nullptr && gpu_sensor_->available()) {
    std::cerr << "[agent] detected NVML GPU sensor\n";
  } else {
    std::cerr << "[agent] NVML GPU sensor unavailable; falling back to none sensor\n";
    gpu_sensor_ = sensors::gpu::make_none_sensor();
  }

  std::cerr << "[agent] tegrastats " << (tegrastats_sensor_.enabled() ? "detected" : "not detected") << '\n';

  register_sensors(config);
}

AgentStats Agent::run_for_ticks(const std::size_t total_ticks) {
  AgentStats stats{};

  if (first_tick_) {
    next_wakeup_ = std::chrono::steady_clock::now();
    first_tick_ = false;
  }

  for (std::size_t i = 0; total_ticks == 0 || i < total_ticks; ++i) {
    const auto cycle_start = std::chrono::steady_clock::now();

    collect_sensors(stats);
    compute_derived(stats);
    compute_risk(stats);
    publish_sinks(stats);

    const auto cycle_end = std::chrono::steady_clock::now();
    const auto actual_period_ms = previous_cycle_start_.has_value()
                                      ? std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(cycle_start - *previous_cycle_start_).count()
                                      : 0.0F;
    const auto compute_ms = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(cycle_end - cycle_start).count();
    update_agent_health(actual_period_ms, compute_ms);
    previous_cycle_start_ = cycle_start;

    ++stats.ticks_executed;
    sampler_.advance();

    next_wakeup_ += tick_interval_;
    std::this_thread::sleep_until(next_wakeup_);
  }

  return stats;
}

void Agent::register_sensors(const AgentConfig& config) {
  sensor_registry_.push_back({"psi", 1, sensor_enabled(config, "psi"), [this](model::signal_frame& frame) { return psi_sensor_.sample(frame); }});
  sensor_registry_.push_back({"cpu", 2, sensor_enabled(config, "cpu"), [this](model::signal_frame& frame) { return cpu_sensor_.sample(frame); }});
  sensor_registry_.push_back({"interrupts", 3, sensor_enabled(config, "interrupts"), [this](model::signal_frame& frame) { return interrupts_sensor_.sample(frame); }});
  sensor_registry_.push_back({"softirqs", 4, sensor_enabled(config, "softirqs"), [this](model::signal_frame& frame) { return softirqs_sensor_.sample(frame); }});
  sensor_registry_.push_back({"memory", 5, sensor_enabled(config, "memory"), [this](model::signal_frame& frame) { return memory_sensor_.sample(frame); }});
  sensor_registry_.push_back({"disk", 6, sensor_enabled(config, "disk"), [this](model::signal_frame& frame) { return disk_sensor_.sample(frame); }});
  sensor_registry_.push_back({"network", 7, sensor_enabled(config, "network"), [this](model::signal_frame& frame) { return network_sensor_.sample(frame); }});
  sensor_registry_.push_back({"tegrastats", 8, sensor_enabled(config, "tegrastats"), [this](model::signal_frame& frame) { return tegrastats_sensor_.sample(frame); }});
  sensor_registry_.push_back({"thermal", 9, sensor_enabled(config, "thermal"), [this](model::signal_frame& frame) { return thermal_sensor_.sample(frame); }});
  sensor_registry_.push_back({"cpu_throttle", 10, sensor_enabled(config, "cpu_throttle"), [this](model::signal_frame& frame) { return cpu_throttle_sensor_.sample(frame); }});
  sensor_registry_.push_back({"cpufreq", 11, sensor_enabled(config, "cpufreq"), [this](model::signal_frame& frame) { return cpufreq_sensor_.sample(frame); }});
  sensor_registry_.push_back({"gpu", 12, sensor_enabled(config, "gpu"), [this](model::signal_frame& frame) {
    return gpu_sensor_ != nullptr ? gpu_sensor_->collect(frame) : false;
  }});
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
  frame_.monotonic_ns = monotonic_timestamp_now_ns();
  frame_.agent.sensor_failures = 0;

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
  frame_.agent.redis_errors = 0;

  if (publish_stdout_) {
    stdout_sink_.publish(frame_);
  }

  if (redis_sink_ != nullptr) {
    const bool ok = redis_sink_->publish(frame_);
    if (!ok) {
      ++frame_.agent.redis_errors;
      if (redis_was_ok_) {
        std::cerr << "[redis] publish failed\n";
        redis_was_ok_ = false;
      }
    } else if (!redis_was_ok_) {
      std::cerr << "[redis] publish recovered\n";
      redis_was_ok_ = true;
    }
  }
}

void Agent::update_agent_health(const float actual_period_ms, const float compute_time_ms) {
  if (!publish_health_) {
    return;
  }

  const auto tick_ms = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(tick_interval_).count();

  frame_.agent.loop_jitter_ms = std::fabs(actual_period_ms - tick_ms);
  frame_.agent.compute_time_ms = compute_time_ms;
  frame_.agent.heartbeat_ms = unix_timestamp_now_ns() / 1'000'000ULL;
  if (compute_time_ms > tick_ms) {
    ++frame_.agent.missed_cycles;
  }
}

}  // namespace hw_agent::core
