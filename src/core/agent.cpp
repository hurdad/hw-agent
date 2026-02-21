#include "core/agent.hpp"

#include <thread>

namespace hw_agent::core {

Agent::Agent(const std::chrono::milliseconds tick_interval) : tick_interval_(tick_interval) {}

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

void Agent::collect_sensors(AgentStats& stats) {
  // Base sensors sample every tick.
  ++stats.sensor_cycles;

  // Example multi-rate sensor hook (10 Hz when base tick is 100 ms).
  (void)sampler_.should_sample_every(1);
}

void Agent::compute_derived(AgentStats& stats) {
  ++stats.derived_cycles;

  // Example derived metric that runs every 5 ticks.
  (void)sampler_.should_sample_every(5);
}

void Agent::compute_risk(AgentStats& stats) { ++stats.risk_cycles; }

void Agent::publish_sinks(AgentStats& stats) { ++stats.sink_cycles; }

}  // namespace hw_agent::core
