#include "derived/power_pressure.hpp"

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

void PowerPressure::sample(model::signal_frame& frame) noexcept {
  const float raw_throttle = clamp01(frame.power);
  const float cpu_norm = clamp01(frame.cpu / 100.0F);
  const float thermal_norm = clamp01(frame.thermal_pressure);

  const float raw_score = (0.75F * raw_throttle) + (0.15F * cpu_norm) + (0.10F * thermal_norm);

  if (!has_ema_) {
    ema_ = raw_score;
    has_ema_ = true;
  } else {
    constexpr float alpha = 0.25F;
    ema_ = ((1.0F - alpha) * ema_) + (alpha * raw_score);
  }

  frame.power_pressure = clamp01(ema_);
}

}  // namespace hw_agent::derived
