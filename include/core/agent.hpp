#pragma once

#include <chrono>
#include <cstddef>

#include "core/sampler.hpp"

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
  explicit Agent(std::chrono::milliseconds tick_interval = std::chrono::milliseconds{100});

  AgentStats run_for_ticks(std::size_t total_ticks);

 private:
  void collect_sensors(AgentStats& stats);
  void compute_derived(AgentStats& stats);
  void compute_risk(AgentStats& stats);
  void publish_sinks(AgentStats& stats);

  std::chrono::milliseconds tick_interval_;
  Sampler sampler_{};
};

}  // namespace hw_agent::core
