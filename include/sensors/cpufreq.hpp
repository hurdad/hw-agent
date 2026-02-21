#pragma once

#include <cstdio>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class CpuFreqSensor {
 public:
  CpuFreqSensor();
  explicit CpuFreqSensor(std::FILE* file, bool owns_file = false);
  ~CpuFreqSensor();

  CpuFreqSensor(const CpuFreqSensor&) = delete;
  CpuFreqSensor& operator=(const CpuFreqSensor&) = delete;

  void sample(model::signal_frame& frame) noexcept;

 private:
  static constexpr std::size_t kReadBufferSize = 32768;

  std::FILE* file_{nullptr};
  bool owns_file_{true};
  float ema_mhz_{0.0F};
  bool has_ema_{false};
};

}  // namespace hw_agent::sensors
