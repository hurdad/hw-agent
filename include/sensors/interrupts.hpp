#pragma once

#include <cstdint>
#include <cstdio>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class InterruptsSensor {
 public:
  InterruptsSensor();
  ~InterruptsSensor();

  InterruptsSensor(const InterruptsSensor&) = delete;
  InterruptsSensor& operator=(const InterruptsSensor&) = delete;

  void sample(model::signal_frame& frame) noexcept;

 private:
  static constexpr std::size_t kReadBufferSize = 256;

  std::FILE* file_{nullptr};
  std::uint64_t prev_total_{0};
  std::uint64_t prev_timestamp_ns_{0};
  bool has_prev_{false};
};

}  // namespace hw_agent::sensors
