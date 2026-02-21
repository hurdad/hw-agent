#include "sensors/cpufreq.hpp"

#include <cerrno>
#include <cstdlib>
#include <glob.h>
#include <utility>

namespace hw_agent::sensors {

CpuFreqSensor::CpuFreqSensor() : owns_files_(true) {
  glob_t matches{};
  constexpr const char* pattern = "/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq";

  if (::glob(pattern, 0, nullptr, &matches) == 0) {
    files_.reserve(matches.gl_pathc);
    for (std::size_t i = 0; i < matches.gl_pathc; ++i) {
      if (std::FILE* file = std::fopen(matches.gl_pathv[i], "r"); file != nullptr) {
        files_.push_back(file);
      }
    }
  }

  ::globfree(&matches);
}

CpuFreqSensor::CpuFreqSensor(std::vector<std::FILE*> files, const bool owns_files)
    : files_(std::move(files)), owns_files_(owns_files) {}

CpuFreqSensor::~CpuFreqSensor() {
  if (!owns_files_) {
    return;
  }

  for (std::FILE* file : files_) {
    if (file != nullptr) {
      std::fclose(file);
    }
  }
  files_.clear();
}

bool CpuFreqSensor::sample(model::signal_frame& frame) noexcept {
  if (files_.empty()) {
    frame.cpufreq = 0.0F;
    return false;
  }

  char value_buffer[64]{};
  double total_mhz = 0.0;
  std::size_t count = 0;

  for (std::FILE* file : files_) {
    if (file == nullptr) {
      continue;
    }

    if (std::fseek(file, 0L, SEEK_SET) != 0) {
      continue;
    }

    if (std::fgets(value_buffer, static_cast<int>(sizeof(value_buffer)), file) == nullptr) {
      continue;
    }

    char* end = nullptr;
    errno = 0;
    const unsigned long long khz = std::strtoull(value_buffer, &end, 10);
    if (errno == 0 && end != value_buffer) {
      total_mhz += static_cast<double>(khz) / 1000.0;
      ++count;
    }
  }

  if (count == 0) {
    frame.cpufreq = 0.0F;
    return false;
  }

  const float current_average_mhz = static_cast<float>(total_mhz / static_cast<double>(count));
  constexpr float alpha = 0.25F;

  if (!has_ema_) {
    has_ema_ = true;
    ema_mhz_ = current_average_mhz;
  } else {
    ema_mhz_ = ((1.0F - alpha) * ema_mhz_) + (alpha * current_average_mhz);
  }

  frame.cpufreq = ema_mhz_;
  return true;
}

}  // namespace hw_agent::sensors
