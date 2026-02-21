#include "sensors/power.hpp"

#include <cctype>
#include <filesystem>

namespace hw_agent::sensors {

namespace {
constexpr const char* kCpuPath = "/sys/devices/system/cpu";
}

PowerSensor::PowerSensor() {
  try {
    for (const auto& entry : std::filesystem::directory_iterator(kCpuPath)) {
      if (!entry.is_directory()) {
        continue;
      }

      const std::string name = entry.path().filename().string();
      if (name.rfind("cpu", 0) != 0 || name.size() <= 3) {
        continue;
      }

      bool is_cpu_dir = true;
      for (std::size_t index = 3; index < name.size(); ++index) {
        if (!std::isdigit(static_cast<unsigned char>(name[index]))) {
          is_cpu_dir = false;
          break;
        }
      }
      if (!is_cpu_dir) {
        continue;
      }

      const std::string base_path = entry.path().string() + "/thermal_throttle";

      ThermalThrottleSource source{};
      source.core_throttle_count_file = std::fopen((base_path + "/core_throttle_count").c_str(), "r");
      source.package_throttle_count_file = std::fopen((base_path + "/package_throttle_count").c_str(), "r");

      if (source.core_throttle_count_file == nullptr && source.package_throttle_count_file == nullptr) {
        continue;
      }

      cores_.push_back(source);
    }
  } catch (const std::filesystem::filesystem_error&) {
    cores_.clear();
  }
}

PowerSensor::~PowerSensor() {
  for (ThermalThrottleSource& core : cores_) {
    if (core.core_throttle_count_file != nullptr) {
      std::fclose(core.core_throttle_count_file);
      core.core_throttle_count_file = nullptr;
    }
    if (core.package_throttle_count_file != nullptr) {
      std::fclose(core.package_throttle_count_file);
      core.package_throttle_count_file = nullptr;
    }
  }
}

void PowerSensor::sample(model::signal_frame& frame) noexcept {
  raw_ = {};
  raw_.total_cores = cores_.size();

  for (ThermalThrottleSource& core : cores_) {
    const std::uint64_t core_throttle_count = read_u64_file(core.core_throttle_count_file);
    const std::uint64_t package_throttle_count = read_u64_file(core.package_throttle_count_file);

    if (!core.has_prev_counts) {
      core.prev_core_throttle_count = core_throttle_count;
      core.prev_package_throttle_count = package_throttle_count;
      core.has_prev_counts = true;
      continue;
    }

    if (core_throttle_count > core.prev_core_throttle_count ||
        package_throttle_count > core.prev_package_throttle_count) {
      ++raw_.throttled_cores;
    }

    core.prev_core_throttle_count = core_throttle_count;
    core.prev_package_throttle_count = package_throttle_count;
  }

  if (raw_.total_cores != 0) {
    raw_.throttle_ratio = static_cast<float>(raw_.throttled_cores) / static_cast<float>(raw_.total_cores);
  }

  frame.power = raw_.throttle_ratio;
}

const PowerSensor::RawFields& PowerSensor::raw() const noexcept { return raw_; }

std::uint64_t PowerSensor::read_u64_file(std::FILE* file) noexcept {
  if (file == nullptr) {
    return 0;
  }

  if (std::fseek(file, 0L, SEEK_SET) != 0) {
    return 0;
  }

  unsigned long long value = 0;
  if (std::fscanf(file, "%llu", &value) != 1) {
    std::clearerr(file);
    return 0;
  }

  return value;
}

}  // namespace hw_agent::sensors
