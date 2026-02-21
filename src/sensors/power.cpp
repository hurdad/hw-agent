#include "sensors/power.hpp"

#include <cctype>
#include <filesystem>
#include <memory>
#include <utility>

namespace hw_agent::sensors {

namespace {
constexpr const char* kCpuPath = "/sys/devices/system/cpu";
}

PowerSensor::PowerSensor() : owns_files_(true) {
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

      auto file_closer = [](std::FILE* file) {
        if (file != nullptr) {
          std::fclose(file);
        }
      };
      using file_ptr = std::unique_ptr<std::FILE, decltype(file_closer)>;

      file_ptr core_throttle_count_file(
          std::fopen((base_path + "/core_throttle_count").c_str(), "r"), file_closer);
      file_ptr package_throttle_count_file(
          std::fopen((base_path + "/package_throttle_count").c_str(), "r"), file_closer);

      ThermalThrottleSource source{};
      source.core_throttle_count_file = core_throttle_count_file.get();
      source.package_throttle_count_file = package_throttle_count_file.get();

      if (source.core_throttle_count_file == nullptr && source.package_throttle_count_file == nullptr) {
        continue;
      }

      cores_.push_back(source);
      core_throttle_count_file.release();
      package_throttle_count_file.release();
    }
  } catch (const std::filesystem::filesystem_error&) {
    cores_.clear();
  }
}

PowerSensor::PowerSensor(std::vector<ThermalThrottleSource> cores, const bool owns_files)
    : cores_(std::move(cores)), owns_files_(owns_files) {}

PowerSensor::~PowerSensor() {
  if (!owns_files_) {
    return;
  }

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

bool PowerSensor::sample(model::signal_frame& frame) noexcept {
  raw_ = {};
  raw_.total_cores = cores_.size();

  bool all_reads_ok = true;
  for (ThermalThrottleSource& core : cores_) {
    std::uint64_t core_throttle_count = 0;
    std::uint64_t package_throttle_count = 0;
    all_reads_ok = read_u64_file(core.core_throttle_count_file, core_throttle_count) && all_reads_ok;
    all_reads_ok = read_u64_file(core.package_throttle_count_file, package_throttle_count) && all_reads_ok;

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
  return all_reads_ok;
}

const PowerSensor::RawFields& PowerSensor::raw() const noexcept { return raw_; }

bool PowerSensor::read_u64_file(std::FILE* file, std::uint64_t& value) noexcept {
  if (file == nullptr) {
    value = 0;
    return false;
  }

  if (std::fseek(file, 0L, SEEK_SET) != 0) {
    value = 0;
    return false;
  }

  unsigned long long parsed = 0;
  if (std::fscanf(file, "%llu", &parsed) != 1) {
    std::clearerr(file);
    value = 0;
    return false;
  }

  value = static_cast<std::uint64_t>(parsed);
  return true;
}

}  // namespace hw_agent::sensors
