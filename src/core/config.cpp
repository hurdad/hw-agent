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
    if (hz > 0) {
      config.tick_interval = std::chrono::milliseconds(1000 / hz);
    }
    return;
  }

  if (key == "thermal_throttle_temp_c") {
    config.thermal_throttle_temp_c = std::stof(value);
    return;
  }

  if (key == "agent.publish_health") {
    config.publish_health = parse_bool(value);
    return;
  }

  if (key == "redis.address") {
    const auto split = value.find(':');
    config.redis.enabled = !value.empty();
    if (split == std::string::npos) {
      config.redis.host = value;
      return;
    }

    config.redis.host = value.substr(0, split);
    config.redis.port = static_cast<std::uint16_t>(std::stoi(value.substr(split + 1)));
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
