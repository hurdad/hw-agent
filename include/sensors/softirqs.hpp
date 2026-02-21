#pragma once

#include <cstdint>
#include <cstdio>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class SoftirqsSensor {
 public:
  SoftirqsSensor();
  explicit SoftirqsSensor(std::FILE* file, bool owns_file = false);
  ~SoftirqsSensor();

  SoftirqsSensor(const SoftirqsSensor&) = delete;
  SoftirqsSensor& operator=(const SoftirqsSensor&) = delete;

  bool sample(model::signal_frame& frame) noexcept;

 private:
  static constexpr std::size_t kReadBufferSize = 8192;

  std::FILE* file_{nullptr};
  bool owns_file_{true};
  std::uint64_t prev_total_{0};
  float baseline_delta_{0.0F};
  bool has_prev_{false};
};

}  // namespace hw_agent::sensors
