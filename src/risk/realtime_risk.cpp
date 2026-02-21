#include "risk/realtime_risk.hpp"
#include "core/math.hpp"

namespace hw_agent::risk {


void RealtimeRisk::sample(model::signal_frame& frame) noexcept {
  const float latency_norm = core::clamp01(frame.latency_jitter);
  const float scheduler_norm = core::clamp01(frame.scheduler_pressure);
  const float thermal_norm = core::clamp01(frame.thermal_pressure);
  const float io_norm = core::clamp01(frame.io_pressure);
  const float raw_score =
      (0.55F * latency_norm) + (0.25F * scheduler_norm) + (0.15F * thermal_norm) + (0.05F * io_norm);

  if (!has_ema_) {
    ema_ = raw_score;
    has_ema_ = true;
  } else {
    constexpr float alpha = 0.35F;
    ema_ = ((1.0F - alpha) * ema_) + (alpha * raw_score);
  }

  frame.realtime_risk = core::clamp01(ema_);
}

}  // namespace hw_agent::risk
