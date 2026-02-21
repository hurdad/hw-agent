#pragma once

#include "model/signal_frame.hpp"

namespace hw_agent::derived {

class ThermalPressure {
 public:
  explicit ThermalPressure(float warning_window_c = 30.0F) noexcept;
  void sample(model::signal_frame& frame) noexcept;

 private:
  float warning_window_c_{30.0F};
  float ema_{0.0F};
  bool has_ema_{false};
};

}  // namespace hw_agent::derived
