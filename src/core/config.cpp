#include "core/config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace hw_agent::core {
namespace {

std::string trim(const std::string& value) {
  const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
  const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
  if (begin >= end) {
    return {};
  }
  return std::string(begin, end);
}

bool parse_bool(const std::string& value) {
  const std::string lower = [&value]() {
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
      out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
  }();

  return lower == "true" || lower == "yes" || lower == "on" || lower == "1";
}

void apply_key_value(AgentConfig& config, const std::string& key, const std::string& value) {
  if (key == "tick_rate_hz") {
    const auto hz = std::stoi(value);
    if (hz <= 0) {
      throw std::runtime_error("tick_rate_hz must be greater than 0");
    }

    if (hz > 1000) {
      throw std::runtime_error("tick_rate_hz must be less than or equal to 1000");
    }

    config.tick_interval = std::chrono::milliseconds(1000 / hz);
    return;
  }

  if (key == "thermal_throttle_temp_c") {
    config.thermal_throttle_temp_c = std::stof(value);
    return;
  }

  if (key == "thermal_pressure_warning_window_c") {
    config.thermal_pressure_warning_window_c = std::stof(value);
    if (config.thermal_pressure_warning_window_c <= 0.0F) {
      throw std::runtime_error("thermal_pressure_warning_window_c must be greater than 0");
    }
    return;
  }

  if (key == "gpu.device_index") {
    const auto parsed_index = std::stoll(value);
    if (parsed_index < 0) {
      throw std::runtime_error("gpu.device_index must be greater than or equal to 0");
    }
    config.gpu_device_index = static_cast<std::uint32_t>(parsed_index);
    return;
  }

  if (key == "agent.publish_health") {
    config.publish_health = parse_bool(value);
    return;
  }

  if (key == "agent.stdout_debug") {
    config.stdout_debug = parse_bool(value);
    return;
  }

  if (key == "redis.address") {
    config.redis.enabled = !value.empty();
    if (value.rfind("unix://", 0) == 0) {
      config.redis.unix_socket = value.substr(std::string("unix://").size());
      config.redis.host.clear();
      config.redis.port = 0;
      return;
    }

    if (!value.empty() && value.front() == '/') {
      config.redis.unix_socket = value;
      config.redis.host.clear();
      config.redis.port = 0;
      return;
    }

    config.redis.unix_socket.clear();
    const auto split = value.find(':');
    if (split == std::string::npos) {
      config.redis.host = value;
      return;
    }

    config.redis.host = value.substr(0, split);
    const auto parsed_port = std::stoi(value.substr(split + 1));
    if (parsed_port <= 0 || parsed_port > 65535) {
      throw std::runtime_error("redis.address port must be in range 1..65535");
    }

    config.redis.port = static_cast<std::uint16_t>(parsed_port);
    return;
  }

  if (key.rfind("sensors.", 0) == 0) {
    const std::string sensor_name = key.substr(std::string("sensors.").size());
    config.sensor_enabled[sensor_name] = parse_bool(value);
  }
}

}  // namespace

AgentConfig load_agent_config(const std::string& path) {
  AgentConfig config{};

  std::ifstream input(path);
  if (!input.is_open()) {
    throw std::runtime_error("unable to open config file: " + path);
  }

  std::vector<std::string> sections;
  std::string line;
  while (std::getline(input, line)) {
    const auto comment_pos = line.find('#');
    if (comment_pos != std::string::npos) {
      line.erase(comment_pos);
    }

    if (trim(line).empty()) {
      continue;
    }

    std::size_t indent_spaces = 0;
    while (indent_spaces < line.size() && line[indent_spaces] == ' ') {
      ++indent_spaces;
    }
    const std::size_t depth = indent_spaces / 2;

    const std::string stripped = trim(line);
    const auto colon_pos = stripped.find(':');
    if (colon_pos == std::string::npos) {
      continue;
    }

    const std::string key = trim(stripped.substr(0, colon_pos));
    const std::string value = trim(stripped.substr(colon_pos + 1));

    if (sections.size() > depth) {
      sections.resize(depth);
    }

    if (value.empty()) {
      if (sections.size() == depth) {
        sections.push_back(key);
      } else {
        sections[depth] = key;
      }
      continue;
    }

    std::ostringstream full_key;
    for (const auto& section : sections) {
      if (!section.empty()) {
        full_key << section << '.';
      }
    }
    full_key << key;

    apply_key_value(config, full_key.str(), value);
  }

  return config;
}

}  // namespace hw_agent::core
