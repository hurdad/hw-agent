#pragma once

#include <cstdio>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class CpuFreqSensor {
 public:
  CpuFreqSensor();
  ~CpuFreqSensor();

  CpuFreqSensor(const CpuFreqSensor&) = delete;
  CpuFreqSensor& operator=(const CpuFreqSensor&) = delete;

  bool sample(model::signal_frame& frame) noexcept;

 private:
  static constexpr std::size_t kReadBufferSize = 32768;

  std::FILE* file_{nullptr};
  float ema_mhz_{0.0F};
  bool has_ema_{false};
};

}  // namespace hw_agent::sensors
