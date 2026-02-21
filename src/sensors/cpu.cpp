#include "sensors/cpu.hpp"

#include <cerrno>
#include <cstdlib>

namespace hw_agent::sensors {

CpuSensor::CpuSensor() : file_(std::fopen("/proc/stat", "r")) {}

CpuSensor::~CpuSensor() {
  if (file_ != nullptr) {
    std::fclose(file_);
    file_ = nullptr;
  }
}

void CpuSensor::sample(model::signal_frame& frame) noexcept {
  if (file_ == nullptr) {
    frame.cpu = 0.0F;
    return;
  }

  if (std::fseek(file_, 0L, SEEK_SET) != 0) {
    frame.cpu = 0.0F;
    return;
  }

  char buffer[kReadBufferSize]{};
  if (std::fgets(buffer, static_cast<int>(sizeof(buffer)), file_) == nullptr) {
    frame.cpu = 0.0F;
    return;
  }

  const char* cursor = buffer;
  if (cursor[0] != 'c' || cursor[1] != 'p' || cursor[2] != 'u' || cursor[3] != ' ') {
    frame.cpu = 0.0F;
    return;
  }
  cursor += 4;

  std::uint64_t values[10]{};
  for (std::size_t i = 0; i < 10; ++i) {
    while (*cursor == ' ') {
      ++cursor;
    }
    if (*cursor == '\0' || *cursor == '\n') {
      break;
    }

    char* end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(cursor, &end, 10);
    if (errno != 0 || end == cursor) {
      frame.cpu = 0.0F;
      return;
    }
    values[i] = parsed;
    cursor = end;
  }

  const std::uint64_t idle = values[3] + values[4];
  const std::uint64_t non_idle = values[0] + values[1] + values[2] + values[5] + values[6] + values[7];
  const std::uint64_t total = idle + non_idle;

  if (!has_prev_) {
    has_prev_ = true;
    prev_total_ = total;
    prev_idle_ = idle;
    frame.cpu = 0.0F;
    return;
  }

  const std::uint64_t total_delta = total - prev_total_;
  const std::uint64_t idle_delta = idle - prev_idle_;

  prev_total_ = total;
  prev_idle_ = idle;

  if (total_delta == 0) {
    frame.cpu = 0.0F;
    return;
  }

  const float busy_delta = static_cast<float>(total_delta - idle_delta);
  frame.cpu = (busy_delta / static_cast<float>(total_delta)) * 100.0F;
}

}  // namespace hw_agent::sensors
