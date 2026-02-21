#include "sensors/cpufreq.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace hw_agent::sensors {

CpuFreqSensor::CpuFreqSensor() : file_(std::fopen("/proc/cpuinfo", "r")) {}

CpuFreqSensor::CpuFreqSensor(std::FILE* file, const bool owns_file) : file_(file), owns_file_(owns_file) {}

CpuFreqSensor::~CpuFreqSensor() {
  if (owns_file_ && file_ != nullptr) {
    std::fclose(file_);
    file_ = nullptr;
  }
}

void CpuFreqSensor::sample(model::signal_frame& frame) noexcept {
  if (file_ == nullptr) {
    frame.cpufreq = 0.0F;
    return;
  }

  if (std::fseek(file_, 0L, SEEK_SET) != 0) {
    frame.cpufreq = 0.0F;
    return;
  }

  constexpr char needle[] = "cpu MHz";
  constexpr std::size_t needle_size = sizeof(needle) - 1;

  char line_buffer[kReadBufferSize]{};
  double total_mhz = 0.0;
  std::size_t count = 0;

  while (std::fgets(line_buffer, static_cast<int>(sizeof(line_buffer)), file_) != nullptr) {
    if (std::memcmp(line_buffer, needle, needle_size) == 0) {
      const char* colon = std::strchr(line_buffer, ':');
      if (colon != nullptr) {
        char* end = nullptr;
        errno = 0;
        const double mhz = std::strtod(colon + 1, &end);
        if (errno == 0 && end != colon + 1) {
          total_mhz += mhz;
          ++count;
        }
      }
    }
  }

  if (count == 0) {
    frame.cpufreq = 0.0F;
    return;
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
}

}  // namespace hw_agent::sensors
