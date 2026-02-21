#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "model/signal_frame.hpp"

namespace hw_agent::derived {

class LatencyJitter {
 public:
  void sample(model::signal_frame& frame) noexcept;

 private:
  static constexpr std::size_t kWindow = 8;

  std::array<std::uint64_t, kWindow> intervals_ns_{};
  std::size_t count_{0};
  std::size_t next_{0};
  std::uint64_t prev_timestamp_{0};
  bool has_prev_timestamp_{false};

  float ema_{0.0F};
  bool has_ema_{false};
};

}  // namespace hw_agent::derived
