#include "core/agent.hpp"

#include <thread>

#include "core/timestamp.hpp"

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
  ++stats.sensor_cycles;
  frame_.timestamp = timestamp_now_ns();
  psi_sensor_.sample(frame_);
  cpu_sensor_.sample(frame_);
  interrupts_sensor_.sample(frame_);
  softirqs_sensor_.sample(frame_);
  cpufreq_sensor_.sample(frame_);

  (void)sampler_.should_sample_every(1);
}

void Agent::compute_derived(AgentStats& stats) {
  ++stats.derived_cycles;
  (void)sampler_.should_sample_every(5);
}

void Agent::compute_risk(AgentStats& stats) { ++stats.risk_cycles; }

void Agent::publish_sinks(AgentStats& stats) {
  ++stats.sink_cycles;
  stdout_sink_.publish(frame_);
}

}  // namespace hw_agent::core
