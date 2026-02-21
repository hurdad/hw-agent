#include "risk/saturation_risk.hpp"
#include "core/math.hpp"

namespace hw_agent::risk {


void SaturationRisk::sample(model::signal_frame& frame) noexcept {
  const float scheduler_norm = core::clamp01(frame.scheduler_pressure);
  const float memory_norm = core::clamp01(frame.memory_pressure);
  const float io_norm = core::clamp01(frame.io_pressure);
  const float power_norm = core::clamp01(frame.power_pressure);
  const float thermal_norm = core::clamp01(frame.thermal_pressure);

  const float raw_score = (0.30F * scheduler_norm) + (0.25F * memory_norm) + (0.20F * io_norm) +
                          (0.15F * power_norm) + (0.10F * thermal_norm);

  if (!has_ema_) {
    ema_ = raw_score;
    has_ema_ = true;
  } else {
    constexpr float alpha = 0.18F;
    ema_ = ((1.0F - alpha) * ema_) + (alpha * raw_score);
  }

  frame.saturation_risk = core::clamp01(ema_);
}

}  // namespace hw_agent::risk
