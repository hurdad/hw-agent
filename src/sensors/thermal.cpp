#include "sensors/thermal.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <utility>

namespace hw_agent::sensors {

namespace {
constexpr const char* kSysClassThermal = "/sys/class/thermal";
}

ThermalSensor::ThermalSensor(const float throttle_temp_c) : owns_files_(true) {
  raw_.throttle_temp_c = throttle_temp_c;
  discover_zones(kSysClassThermal);
}

ThermalSensor::ThermalSensor(const float throttle_temp_c, std::string thermal_root) : owns_files_(true) {
  raw_.throttle_temp_c = throttle_temp_c;
  discover_zones(thermal_root);
}

ThermalSensor::ThermalSensor(const float throttle_temp_c, std::vector<ZoneSource> zones, const bool owns_files)
    : zones_(std::move(zones)), owns_files_(owns_files) {
  raw_.throttle_temp_c = throttle_temp_c;
}

void ThermalSensor::discover_zones(const std::string& thermal_root) {
  try {
    if (!std::filesystem::exists(thermal_root)) {
      return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(thermal_root)) {
      if (!entry.is_directory()) {
        continue;
      }

      const std::string name = entry.path().filename().string();
      if (name.rfind("thermal_zone", 0) != 0) {
        continue;
      }

      const std::filesystem::path zone_path = entry.path();

      std::ifstream type_file(zone_path / "type");
      std::string zone_name = name;
      if (type_file.is_open()) {
        std::getline(type_file, zone_name);
      }

      ZoneSource source{};
      source.name = zone_name;
      source.temp_path = (zone_path / "temp").string();

      auto file_closer = [](std::FILE* file) {
        if (file != nullptr) {
          std::fclose(file);
        }
      };
      using file_ptr = std::unique_ptr<std::FILE, decltype(file_closer)>;

      file_ptr file(std::fopen(source.temp_path.c_str(), "r"), file_closer);
      source.file = file.get();

      zones_.push_back(source);
      file.release();
    }
  } catch (const std::filesystem::filesystem_error&) {
    zones_.clear();
  }
}

ThermalSensor::~ThermalSensor() {
  if (!owns_files_) {
    return;
  }

  for (ZoneSource& zone : zones_) {
    if (zone.file != nullptr) {
      std::fclose(zone.file);
      zone.file = nullptr;
    }
  }
}

bool ThermalSensor::sample(model::signal_frame& frame) noexcept {
  raw_.hottest_zone.clear();
  raw_.hottest_temp_c = 0.0F;

  float max_temp_c = -std::numeric_limits<float>::infinity();
  std::size_t max_index = 0;

  for (std::size_t i = 0; i < zones_.size(); ++i) {
    ZoneSource& zone = zones_[i];
    bool opened_in_sample = false;
    if (zone.file == nullptr) {
      zone.file = std::fopen(zone.temp_path.c_str(), "r");
      if (zone.file == nullptr) {
        continue;
      }
      opened_in_sample = true;
    }

    float zone_temp_c = 0.0F;
    if (!read_temp_c(zone.file, zone_temp_c)) {
      if (opened_in_sample && !owns_files_) {
        std::fclose(zone.file);
        zone.file = nullptr;
      }
      continue;
    }

    if (opened_in_sample && !owns_files_) {
      std::fclose(zone.file);
      zone.file = nullptr;
    }

    if (zone_temp_c > max_temp_c) {
      max_temp_c = zone_temp_c;
      max_index = i;
    }
  }

  if (std::isfinite(max_temp_c)) {
    raw_.hottest_temp_c = max_temp_c;
    raw_.hottest_zone = zones_[max_index].name;
    raw_.headroom_c = raw_.throttle_temp_c - raw_.hottest_temp_c;
  } else {
    raw_.headroom_c = 0.0F;
  }
  frame.thermal = raw_.headroom_c;
  return std::isfinite(max_temp_c);
}

void ThermalSensor::set_throttle_temp_c(const float throttle_temp_c) noexcept {
  raw_.throttle_temp_c = throttle_temp_c;
}

const ThermalSensor::RawFields& ThermalSensor::raw() const noexcept { return raw_; }

bool ThermalSensor::read_temp_c(std::FILE* file, float& temp_c) noexcept {
  if (file == nullptr) {
    temp_c = -std::numeric_limits<float>::infinity();
    return false;
  }

  if (std::fseek(file, 0L, SEEK_SET) != 0) {
    temp_c = -std::numeric_limits<float>::infinity();
    return false;
  }

  long long raw_temp = 0;
  if (std::fscanf(file, "%lld", &raw_temp) != 1) {
    std::clearerr(file);
    temp_c = -std::numeric_limits<float>::infinity();
    return false;
  }

  temp_c = static_cast<float>(raw_temp) / 1000.0F;
  return true;
}

}  // namespace hw_agent::sensors
