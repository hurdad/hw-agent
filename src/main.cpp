#include <iostream>

#include "core/agent.hpp"

int main() {
  constexpr std::size_t kTestTicks = 5;

  hw_agent::core::Agent agent{};
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
