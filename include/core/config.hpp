#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace hw_agent::core {

struct RedisConfig {
  std::string host{"127.0.0.1"};
  std::uint16_t port{6379};
  bool enabled{false};
};

struct AgentConfig {
  std::chrono::milliseconds tick_interval{100};
  float thermal_throttle_temp_c{85.0F};
  bool publish_health{true};
  bool stdout_debug{true};
  RedisConfig redis{};
  std::unordered_map<std::string, bool> sensor_enabled{};
};

AgentConfig load_agent_config(const std::string& path);

}  // namespace hw_agent::core
