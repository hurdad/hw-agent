#include "derived/io_pressure.hpp"

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

void IoPressure::sample(model::signal_frame& frame) noexcept {
  const float disk_norm = clamp01(1.0F - std::exp(-frame.disk / 50.0F));
  const float network_norm = clamp01(frame.network);
  const float psi_norm = clamp01(frame.psi / 20.0F);

  const float raw_score = (0.70F * disk_norm) + (0.20F * network_norm) + (0.10F * psi_norm);

  if (!has_ema_) {
    ema_ = raw_score;
    has_ema_ = true;
  } else {
    constexpr float alpha = 0.25F;
    ema_ = ((1.0F - alpha) * ema_) + (alpha * raw_score);
  }

  frame.io_pressure = clamp01(ema_);
}

}  // namespace hw_agent::derived
