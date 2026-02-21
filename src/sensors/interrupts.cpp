#include "sensors/interrupts.hpp"

#include <cerrno>
#include <cstdlib>

namespace hw_agent::sensors {

InterruptsSensor::InterruptsSensor() : file_(std::fopen("/proc/stat", "r")) {}

InterruptsSensor::~InterruptsSensor() {
  if (file_ != nullptr) {
    std::fclose(file_);
    file_ = nullptr;
  }
}

void InterruptsSensor::sample(model::signal_frame& frame) noexcept {
  if (file_ == nullptr) {
    frame.irq = 0.0F;
    return;
  }

  if (std::fseek(file_, 0L, SEEK_SET) != 0) {
    frame.irq = 0.0F;
    return;
  }

  char buffer[kReadBufferSize]{};
  bool found = false;
  while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), file_) != nullptr) {
    if (buffer[0] == 'i' && buffer[1] == 'n' && buffer[2] == 't' && buffer[3] == 'r' &&
        buffer[4] == ' ') {
      found = true;
      break;
    }
  }

  if (!found) {
    frame.irq = 0.0F;
    return;
  }

  const char* cursor = buffer + 5;
  while (*cursor == ' ') {
    ++cursor;
  }

  char* end = nullptr;
  errno = 0;
  const unsigned long long total_interrupts = std::strtoull(cursor, &end, 10);
  if (errno != 0 || end == cursor) {
    frame.irq = 0.0F;
    return;
  }

  if (!has_prev_) {
    has_prev_ = true;
    prev_total_ = total_interrupts;
    prev_timestamp_ns_ = frame.timestamp;
    frame.irq = 0.0F;
    return;
  }

  const std::uint64_t count_delta = total_interrupts - prev_total_;
  const std::uint64_t time_delta_ns = frame.timestamp - prev_timestamp_ns_;

  prev_total_ = total_interrupts;
  prev_timestamp_ns_ = frame.timestamp;

  if (time_delta_ns == 0) {
    frame.irq = 0.0F;
    return;
  }

  const double seconds = static_cast<double>(time_delta_ns) / 1'000'000'000.0;
  frame.irq = static_cast<float>(static_cast<double>(count_delta) / seconds);
}

}  // namespace hw_agent::sensors
