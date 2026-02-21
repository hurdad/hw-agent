#pragma once

#include <cstdint>
#include <cstdio>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class CpuSensor {
 public:
  CpuSensor();
  ~CpuSensor();

  CpuSensor(const CpuSensor&) = delete;
  CpuSensor& operator=(const CpuSensor&) = delete;

  bool sample(model::signal_frame& frame) noexcept;

 private:
  static constexpr std::size_t kReadBufferSize = 512;

  std::FILE* file_{nullptr};
  std::uint64_t prev_total_{0};
  std::uint64_t prev_idle_{0};
  bool has_prev_{false};
};

}  // namespace hw_agent::sensors
