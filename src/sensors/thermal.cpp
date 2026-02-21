#include "sensors/thermal.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

namespace hw_agent::sensors {

namespace {
constexpr const char* kSysClassThermal = "/sys/class/thermal";
}

ThermalSensor::ThermalSensor(const float throttle_temp_c) {
  raw_.throttle_temp_c = throttle_temp_c;

  for (const auto& entry : std::filesystem::directory_iterator(kSysClassThermal)) {
    if (!entry.is_directory()) {
      continue;
    }

    const std::string name = entry.path().filename().string();
    if (name.rfind("thermal_zone", 0) != 0) {
      continue;
    }

    const std::filesystem::path zone_path = entry.path();
    zone_temp_paths_.push_back((zone_path / "temp").string());

    std::ifstream type_file(zone_path / "type");
    std::string zone_name = name;
    if (type_file.is_open()) {
      std::getline(type_file, zone_name);
    }
    zone_names_.push_back(zone_name);
  }
}

void ThermalSensor::sample(model::signal_frame& frame) noexcept {
  raw_.hottest_zone.clear();
  raw_.hottest_temp_c = 0.0F;

  float max_temp_c = -std::numeric_limits<float>::infinity();
  std::size_t max_index = 0;

  for (std::size_t i = 0; i < zone_temp_paths_.size(); ++i) {
    const float zone_temp_c = read_temp_c(zone_temp_paths_[i]);
    if (zone_temp_c > max_temp_c) {
      max_temp_c = zone_temp_c;
      max_index = i;
    }
  }

  if (std::isfinite(max_temp_c)) {
    raw_.hottest_temp_c = max_temp_c;
    raw_.hottest_zone = zone_names_[max_index];
  }

  raw_.headroom_c = raw_.throttle_temp_c - raw_.hottest_temp_c;
  frame.thermal = raw_.headroom_c;
}

void ThermalSensor::set_throttle_temp_c(const float throttle_temp_c) noexcept {
  raw_.throttle_temp_c = throttle_temp_c;
}

const ThermalSensor::RawFields& ThermalSensor::raw() const noexcept { return raw_; }

float ThermalSensor::read_temp_c(const std::string& path) noexcept {
  std::ifstream input(path);
  if (!input.is_open()) {
    return -std::numeric_limits<float>::infinity();
  }

  long long raw_temp = 0;
  input >> raw_temp;
  if (!input.good() && !input.eof()) {
    return -std::numeric_limits<float>::infinity();
  }

  return static_cast<float>(raw_temp) / 1000.0F;
}

}  // namespace hw_agent::sensors
