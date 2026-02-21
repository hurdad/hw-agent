#include "derived/latency_jitter.hpp"
#include "core/math.hpp"

namespace hw_agent::derived {


void LatencyJitter::sample(model::signal_frame& frame) noexcept {
  float temporal_jitter_norm = 0.0F;

  if (has_prev_timestamp_) {
    const std::uint64_t interval = frame.monotonic_ns - prev_timestamp_;
    intervals_ns_[next_] = interval;
    next_ = (next_ + 1) % kWindow;
    if (count_ < kWindow) {
      ++count_;
    }

    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < count_; ++i) {
      sum += intervals_ns_[i];
    }

    const float mean = static_cast<float>(sum) / static_cast<float>(count_);
    if (mean > 0.0F) {
      float abs_dev_sum = 0.0F;
      for (std::size_t i = 0; i < count_; ++i) {
        const float deviation = static_cast<float>(intervals_ns_[i]) - mean;
        abs_dev_sum += deviation < 0.0F ? -deviation : deviation;
      }
      const float mad = abs_dev_sum / static_cast<float>(count_);
      temporal_jitter_norm = core::clamp01((mad / mean) / 0.25F);
    }
  }

  prev_timestamp_ = frame.monotonic_ns;
  has_prev_timestamp_ = true;

  const float sched_norm = core::clamp01(frame.scheduler_pressure);
  const float io_norm = core::clamp01(frame.io_pressure);
  const float raw_score = (0.60F * temporal_jitter_norm) + (0.25F * sched_norm) + (0.15F * io_norm);

  if (!has_ema_) {
    ema_ = raw_score;
    has_ema_ = true;
  } else {
    constexpr float alpha = 0.30F;
    ema_ = ((1.0F - alpha) * ema_) + (alpha * raw_score);
  }

  frame.latency_jitter = core::clamp01(ema_);
}

}  // namespace hw_agent::derived
