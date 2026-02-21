#include <csignal>
#include <iostream>
#include <string>

#include "core/agent.hpp"
#include "core/config.hpp"

namespace {

volatile std::sig_atomic_t g_shutdown_requested = 0;

void handle_shutdown_signal(int /*signal*/) {
  g_shutdown_requested = 1;
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, handle_shutdown_signal);
  std::signal(SIGTERM, handle_shutdown_signal);

  const std::string config_path = argc > 1 ? argv[1] : "configs/agent.yaml";

  hw_agent::core::AgentConfig config{};
  try {
    config = hw_agent::core::load_agent_config(config_path);
  } catch (const std::exception& ex) {
    std::cerr << "config error: " << ex.what() << '\n';
    return 1;
  }

  hw_agent::core::Agent agent{config};
  while (g_shutdown_requested == 0) {
    agent.run_for_ticks(1);
  }

  std::cerr << "[agent] shutdown signal received; exiting cleanly\n";

  return 0;
}
