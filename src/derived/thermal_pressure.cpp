#include "derived/thermal_pressure.hpp"

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

void ThermalPressure::sample(model::signal_frame& frame) noexcept {
  const float headroom_pressure = clamp01((30.0F - frame.thermal) / 30.0F);
  const float power_norm = clamp01(frame.power);
  const float cpu_norm = clamp01(frame.cpu / 100.0F);

  const float raw_score = (0.70F * headroom_pressure) + (0.20F * power_norm) + (0.10F * cpu_norm);

  if (!has_ema_) {
    ema_ = raw_score;
    has_ema_ = true;
  } else {
    constexpr float alpha = 0.20F;
    ema_ = ((1.0F - alpha) * ema_) + (alpha * raw_score);
  }

  frame.thermal_pressure = clamp01(ema_);
}

}  // namespace hw_agent::derived
