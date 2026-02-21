#include <iostream>
#include <string>

#include "core/agent.hpp"
#include "core/config.hpp"

int main(int argc, char** argv) {
  constexpr std::size_t kTestTicks = 5;
  const std::string config_path = argc > 1 ? argv[1] : "configs/agent.yaml";

  hw_agent::core::AgentConfig config{};
  try {
    config = hw_agent::core::load_agent_config(config_path);
  } catch (const std::exception& ex) {
    std::cerr << "config error: " << ex.what() << '\n';
    return 1;
  }

  hw_agent::core::Agent agent{config};
  const auto stats = agent.run_for_ticks(kTestTicks);

  std::cout << "tick_count=" << stats.ticks_executed << '\n';

  if (stats.ticks_executed != kTestTicks || stats.sensor_cycles != kTestTicks ||
      stats.derived_cycles != kTestTicks || stats.risk_cycles != kTestTicks ||
      stats.sink_cycles != kTestTicks) {
    std::cerr << "runtime loop stage count mismatch\n";
    return 1;
  }

  return 0;
}
