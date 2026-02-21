#pragma once

#include "model/signal_frame.hpp"

namespace hw_agent::derived {

class SchedulerPressure {
 public:
  void sample(model::signal_frame& frame) noexcept;

 private:
  float irq_baseline_{1.0F};
  bool has_irq_baseline_{false};
  float ema_{0.0F};
  bool has_ema_{false};
};

}  // namespace hw_agent::derived
