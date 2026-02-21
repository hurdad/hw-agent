#include "sensors/power.hpp"

#include <filesystem>

namespace hw_agent::sensors {

namespace {
constexpr const char* kCpuFreqPath = "/sys/devices/system/cpu/cpufreq";
}

PowerSensor::PowerSensor() {
  for (const auto& entry : std::filesystem::directory_iterator(kCpuFreqPath)) {
    if (!entry.is_directory()) {
      continue;
    }

    const std::string name = entry.path().filename().string();
    if (name.rfind("policy", 0) != 0) {
      continue;
    }

    const std::string base_path = entry.path().string();

    PolicySource source{};
    source.cur_freq_file = std::fopen((base_path + "/scaling_cur_freq").c_str(), "r");
    source.max_freq_file = std::fopen((base_path + "/scaling_max_freq").c_str(), "r");
    policies_.push_back(source);
  }
}

PowerSensor::~PowerSensor() {
  for (PolicySource& policy : policies_) {
    if (policy.cur_freq_file != nullptr) {
      std::fclose(policy.cur_freq_file);
      policy.cur_freq_file = nullptr;
    }
    if (policy.max_freq_file != nullptr) {
      std::fclose(policy.max_freq_file);
      policy.max_freq_file = nullptr;
    }
  }
}

void PowerSensor::sample(model::signal_frame& frame) noexcept {
  raw_ = {};
  raw_.total_policies = policies_.size();

  for (const PolicySource& policy : policies_) {
    const std::uint64_t cur_freq = read_u64_file(policy.cur_freq_file);
    const std::uint64_t max_freq = read_u64_file(policy.max_freq_file);

    if (max_freq == 0) {
      continue;
    }

    if (cur_freq < max_freq) {
      ++raw_.throttled_policies;
    }
  }

  if (raw_.total_policies != 0) {
    raw_.throttle_ratio = static_cast<float>(raw_.throttled_policies) / static_cast<float>(raw_.total_policies);
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
