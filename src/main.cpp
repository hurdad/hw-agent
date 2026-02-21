#include <csignal>
#include <iostream>
#include <sstream>
#include <string>

#include "core/agent.hpp"
#include "core/config.hpp"

namespace {

volatile std::sig_atomic_t g_shutdown_requested = 0;

void handle_shutdown_signal(int /*signal*/) {
  g_shutdown_requested = 1;
}

}  // namespace

std::string format_config_settings(const hw_agent::core::AgentConfig& config, const std::string& config_path) {
  std::ostringstream output;
  output << "[agent] loaded config from " << config_path
         << " | tick_interval_ms=" << config.tick_interval.count()
         << " | thermal_throttle_temp_c=" << config.thermal_throttle_temp_c
         << " | thermal_pressure_warning_window_c=" << config.thermal_pressure_warning_window_c
         << " | publish_health=" << (config.publish_health ? "true" : "false")
         << " | stdout_debug=" << (config.stdout_debug ? "true" : "false")
         << " | redis_enabled=" << (config.redis.enabled ? "true" : "false")
         << " | redis_address=";

  if (!config.redis.unix_socket.empty()) {
    output << "unix://" << config.redis.unix_socket;
  } else {
    output << config.redis.host << ':' << config.redis.port;
  }
  return output.str();
}

int main(int argc, char** argv) {
  std::signal(SIGINT, handle_shutdown_signal);
  std::signal(SIGTERM, handle_shutdown_signal);

  const std::string config_path = argc > 1 ? argv[1] : "configs/agent.all.debug.yaml";

  hw_agent::core::AgentConfig config{};
  try {
    config = hw_agent::core::load_agent_config(config_path);
  } catch (const std::exception& ex) {
    std::cerr << "config error: " << ex.what() << '\n';
    return 1;
  }

  std::cerr << format_config_settings(config, config_path) << '\n';

  hw_agent::core::Agent agent{config};
  while (g_shutdown_requested == 0) {
    agent.run_for_ticks(1);
  }

  std::cerr << "[agent] shutdown signal received; exiting cleanly\n";

  return 0;
}
