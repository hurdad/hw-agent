#include "derived/power_pressure.hpp"
#include "core/math.hpp"

namespace hw_agent::derived {


void PowerPressure::sample(model::signal_frame& frame) noexcept {
  const float raw_throttle = core::clamp01(frame.cpu_throttle_ratio);
  const float cpu_norm = core::clamp01(frame.cpu / 100.0F);
  const float thermal_norm = core::clamp01(frame.thermal_pressure);

  const float raw_score = (0.75F * raw_throttle) + (0.15F * cpu_norm) + (0.10F * thermal_norm);

  if (!has_ema_) {
    ema_ = raw_score;
    has_ema_ = true;
  } else {
    constexpr float alpha = 0.25F;
    ema_ = ((1.0F - alpha) * ema_) + (alpha * raw_score);
  }

  frame.power_pressure = core::clamp01(ema_);
}

}  // namespace hw_agent::derived
