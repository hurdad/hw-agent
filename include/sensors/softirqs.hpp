#pragma once

#include <cstdint>
#include <cstdio>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class SoftirqsSensor {
 public:
  SoftirqsSensor();
  ~SoftirqsSensor();

  SoftirqsSensor(const SoftirqsSensor&) = delete;
  SoftirqsSensor& operator=(const SoftirqsSensor&) = delete;

  void sample(model::signal_frame& frame) noexcept;

 private:
  static constexpr std::size_t kReadBufferSize = 8192;

  std::FILE* file_{nullptr};
  std::uint64_t prev_total_{0};
  float baseline_delta_{0.0F};
  bool has_prev_{false};
};

}  // namespace hw_agent::sensors
