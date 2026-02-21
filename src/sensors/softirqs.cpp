#include "sensors/softirqs.hpp"

#include <cerrno>
#include <cstdlib>

namespace hw_agent::sensors {

SoftirqsSensor::SoftirqsSensor() : file_(std::fopen("/proc/softirqs", "r")) {}

SoftirqsSensor::~SoftirqsSensor() {
  if (file_ != nullptr) {
    std::fclose(file_);
    file_ = nullptr;
  }
}

void SoftirqsSensor::sample(model::signal_frame& frame) noexcept {
  if (file_ == nullptr) {
    frame.softirqs = 0.0F;
    return;
  }

  if (std::fseek(file_, 0L, SEEK_SET) != 0) {
    frame.softirqs = 0.0F;
    return;
  }

  char buffer[kReadBufferSize]{};
  const std::size_t bytes_read = std::fread(buffer, 1, sizeof(buffer) - 1, file_);
  if (bytes_read == 0U) {
    frame.softirqs = 0.0F;
    return;
  }
  buffer[bytes_read] = '\0';

  std::uint64_t total_softirqs = 0;
  const char* cursor = buffer;
  while (*cursor != '\0') {
    if (*cursor >= '0' && *cursor <= '9') {
      char* end = nullptr;
      errno = 0;
      const unsigned long long parsed = std::strtoull(cursor, &end, 10);
      if (errno == 0 && end != cursor) {
        total_softirqs += parsed;
        cursor = end;
        continue;
      }
    }
    ++cursor;
  }

  if (!has_prev_) {
    has_prev_ = true;
    prev_total_ = total_softirqs;
    baseline_delta_ = 0.0F;
    frame.softirqs = 0.0F;
    return;
  }

  const std::uint64_t delta = total_softirqs - prev_total_;
  prev_total_ = total_softirqs;

  const float delta_f = static_cast<float>(delta);
  if (baseline_delta_ <= 0.0F) {
    baseline_delta_ = delta_f;
    frame.softirqs = 0.0F;
    return;
  }

  constexpr float alpha = 0.2F;
  baseline_delta_ = (1.0F - alpha) * baseline_delta_ + alpha * delta_f;

  if (baseline_delta_ <= 0.0F) {
    frame.softirqs = 0.0F;
    return;
  }

  const float burst_ratio = delta_f / baseline_delta_;
  const float score = (burst_ratio - 1.5F) / 2.5F;
  frame.softirqs = score < 0.0F ? 0.0F : (score > 1.0F ? 1.0F : score);
}

}  // namespace hw_agent::sensors
