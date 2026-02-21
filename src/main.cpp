#include <iostream>
#include <string>

#include "core/agent.hpp"
#include "core/config.hpp"

int main(int argc, char** argv) {
  const std::string config_path = argc > 1 ? argv[1] : "configs/agent.yaml";

  hw_agent::core::AgentConfig config{};
  try {
    config = hw_agent::core::load_agent_config(config_path);
  } catch (const std::exception& ex) {
    std::cerr << "config error: " << ex.what() << '\n';
    return 1;
  }

  hw_agent::core::Agent agent{config};
  agent.run_for_ticks(0);

  return 0;
}
