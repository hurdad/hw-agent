#include "sensors/softirqs.hpp"

#include <cstdint>
#include <limits>

namespace hw_agent::sensors {

SoftirqsSensor::SoftirqsSensor() : file_(std::fopen("/proc/softirqs", "r")) {}

SoftirqsSensor::SoftirqsSensor(std::FILE* file, const bool owns_file) : file_(file), owns_file_(owns_file) {}

SoftirqsSensor::~SoftirqsSensor() {
  if (owns_file_ && file_ != nullptr) {
    std::fclose(file_);
    file_ = nullptr;
  }
}

bool SoftirqsSensor::sample(model::signal_frame& frame) noexcept {
  if (file_ == nullptr) {
    frame.softirqs = 0.0F;
    return false;
  }

  if (std::fseek(file_, 0L, SEEK_SET) != 0) {
    frame.softirqs = 0.0F;
    return false;
  }

  char buffer[kReadBufferSize]{};
  bool saw_any_data = false;
  bool parse_values = false;
  bool in_number = false;
  std::uint64_t current_value = 0;

  std::uint64_t total_softirqs = 0;

  while (true) {
    const std::size_t bytes_read = std::fread(buffer, 1, sizeof(buffer), file_);
    if (bytes_read == 0U) {
      break;
    }

    saw_any_data = true;
    for (std::size_t i = 0; i < bytes_read; ++i) {
      const char ch = buffer[i];

      if (ch == '\n') {
        if (in_number) {
          total_softirqs += current_value;
          in_number = false;
          current_value = 0;
        }
        parse_values = false;
        continue;
      }

      if (!parse_values) {
        if (ch == ':') {
          parse_values = true;
        }
        continue;
      }

      if (ch >= '0' && ch <= '9') {
        const std::uint64_t digit = static_cast<std::uint64_t>(ch - '0');
        if (in_number) {
          constexpr std::uint64_t kMaxValue = std::numeric_limits<std::uint64_t>::max();
          constexpr std::uint64_t kMaxBeforeMul10 = kMaxValue / 10;
          constexpr std::uint64_t kMaxLastDigit = kMaxValue % 10;
          if (current_value > kMaxBeforeMul10 || (current_value == kMaxBeforeMul10 && digit > kMaxLastDigit)) {
            current_value = kMaxValue;
          } else {
            current_value = (current_value * 10) + digit;
          }
        } else {
          in_number = true;
          current_value = digit;
        }
        continue;
      }

      if (in_number) {
        total_softirqs += current_value;
        in_number = false;
        current_value = 0;
      }
    }
  }

  if (in_number) {
    total_softirqs += current_value;
  }

  if (!saw_any_data) {
    frame.softirqs = 0.0F;
    return false;
  }

  if (!has_prev_) {
    has_prev_ = true;
    prev_total_ = total_softirqs;
    baseline_delta_ = 0.0F;
    frame.softirqs = 0.0F;
    return true;
  }

  const std::uint64_t delta = total_softirqs >= prev_total_ ? (total_softirqs - prev_total_) : 0;
  prev_total_ = total_softirqs;

  const float delta_f = static_cast<float>(delta);
  if (baseline_delta_ <= 0.0F) {
    baseline_delta_ = delta_f;
    frame.softirqs = 0.0F;
    return true;
  }

  constexpr float alpha = 0.2F;
  baseline_delta_ = (1.0F - alpha) * baseline_delta_ + alpha * delta_f;

  if (baseline_delta_ <= 0.0F) {
    frame.softirqs = 0.0F;
    return true;
  }

  const float burst_ratio = delta_f / baseline_delta_;
  const float score = (burst_ratio - 1.5F) / 2.5F;
  frame.softirqs = score < 0.0F ? 0.0F : (score > 1.0F ? 1.0F : score);
  return true;
}

}  // namespace hw_agent::sensors
