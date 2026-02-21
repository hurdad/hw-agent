#include "risk/realtime_risk.hpp"

namespace hw_agent::risk {

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

void RealtimeRisk::sample(model::signal_frame& frame) noexcept {
  const float latency_norm = clamp01(frame.latency_jitter);
  const float scheduler_norm = clamp01(frame.scheduler_pressure);
  const float thermal_norm = clamp01(frame.thermal_pressure);
  const float io_norm = clamp01(frame.io_pressure);
  const float raw_score =
      (0.55F * latency_norm) + (0.25F * scheduler_norm) + (0.15F * thermal_norm) + (0.05F * io_norm);

  if (!has_ema_) {
    ema_ = raw_score;
    has_ema_ = true;
  } else {
    constexpr float alpha = 0.35F;
    ema_ = ((1.0F - alpha) * ema_) + (alpha * raw_score);
  }

  frame.realtime_risk = clamp01(ema_);
}

}  // namespace hw_agent::risk
