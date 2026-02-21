#include "derived/memory_pressure.hpp"

#include <cmath>

namespace hw_agent::derived {

namespace {
float clamp01(const float value) {
  if (value < 0.0F) {
    return 0.0F;
  }
  if (value > 1.0F) {
    return 1.0F;
  }
  return value;
}
}  // namespace

void MemoryPressure::sample(model::signal_frame& frame) noexcept {
  const float dirty_writeback_norm = 1.0F - std::exp(-frame.memory / 262144.0F);
  const float psi_norm = clamp01(frame.psi / 20.0F);

  const float raw_score = (0.70F * clamp01(dirty_writeback_norm)) + (0.30F * psi_norm);

  if (!has_ema_) {
    ema_ = raw_score;
    has_ema_ = true;
  } else {
    constexpr float alpha = 0.25F;
    ema_ = ((1.0F - alpha) * ema_) + (alpha * raw_score);
  }

  frame.memory_pressure = clamp01(ema_);
}

}  // namespace hw_agent::derived
