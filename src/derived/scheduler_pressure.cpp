#include "derived/scheduler_pressure.hpp"

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

void SchedulerPressure::sample(model::signal_frame& frame) noexcept {
  const float cpu_norm = clamp01(frame.cpu / 100.0F);
  const float psi_norm = clamp01(frame.psi / 10.0F);
  const float softirq_norm = clamp01(frame.softirqs);

  if (!has_irq_baseline_) {
    irq_baseline_ = frame.irq > 1.0F ? frame.irq : 1.0F;
    has_irq_baseline_ = true;
  } else {
    irq_baseline_ = (0.9F * irq_baseline_) + (0.1F * frame.irq);
    if (irq_baseline_ < 1.0F) {
      irq_baseline_ = 1.0F;
    }
  }

  const float irq_ratio = frame.irq / ((2.0F * irq_baseline_) + 1.0F);
  const float irq_norm = clamp01((irq_ratio - 0.5F) / 1.5F);

  const float raw_score = (0.40F * cpu_norm) + (0.35F * psi_norm) + (0.15F * irq_norm) + (0.10F * softirq_norm);
  if (!has_ema_) {
    ema_ = raw_score;
    has_ema_ = true;
  } else {
    constexpr float alpha = 0.30F;
    ema_ = ((1.0F - alpha) * ema_) + (alpha * raw_score);
  }

  frame.scheduler_pressure = clamp01(ema_);
}

}  // namespace hw_agent::derived
