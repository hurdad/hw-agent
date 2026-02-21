#pragma once

#include <cstdio>
#include <vector>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class CpuFreqSensor {
 public:
  CpuFreqSensor();
  explicit CpuFreqSensor(std::vector<std::FILE*> files, bool owns_files = false);
  ~CpuFreqSensor();

  CpuFreqSensor(const CpuFreqSensor&) = delete;
  CpuFreqSensor& operator=(const CpuFreqSensor&) = delete;

  bool sample(model::signal_frame& frame) noexcept;

 private:
  std::vector<std::FILE*> files_{};
  bool owns_files_{true};
  float ema_mhz_{0.0F};
  bool has_ema_{false};
};

}  // namespace hw_agent::sensors
