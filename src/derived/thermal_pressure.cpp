#include "derived/thermal_pressure.hpp"
#include "core/math.hpp"

namespace hw_agent::derived {

ThermalPressure::ThermalPressure(const float warning_window_c) noexcept : warning_window_c_(warning_window_c > 0.0F ? warning_window_c : 30.0F) {}

void ThermalPressure::sample(model::signal_frame& frame) noexcept {
  const float headroom_pressure = core::clamp01((warning_window_c_ - frame.thermal) / warning_window_c_);
  const float throttle_norm = core::clamp01(frame.cpu_throttle_ratio);
  const float cpu_norm = core::clamp01(frame.cpu / 100.0F);

  const float raw_score = (0.70F * headroom_pressure) + (0.20F * throttle_norm) + (0.10F * cpu_norm);

  if (!has_ema_) {
    ema_ = raw_score;
    has_ema_ = true;
  } else {
    constexpr float alpha = 0.20F;
    ema_ = ((1.0F - alpha) * ema_) + (alpha * raw_score);
  }

  frame.thermal_pressure = core::clamp01(ema_);
}

}  // namespace hw_agent::derived
