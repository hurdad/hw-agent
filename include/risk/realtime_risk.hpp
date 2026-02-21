#pragma once

#include "model/signal_frame.hpp"

namespace hw_agent::risk {

class RealtimeRisk {
 public:
  void sample(model::signal_frame& frame) noexcept;

 private:
  float ema_{0.0F};
  bool has_ema_{false};
};

}  // namespace hw_agent::risk
