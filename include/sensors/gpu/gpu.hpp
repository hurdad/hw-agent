#pragma once

#include <memory>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors::gpu {

using SignalFrame = model::signal_frame;

class GpuSensor {
 public:
  virtual bool available() const = 0;
  virtual bool collect(SignalFrame& frame) = 0;
  virtual ~GpuSensor() = default;
};

std::unique_ptr<GpuSensor> make_nvml_sensor();
std::unique_ptr<GpuSensor> make_none_sensor();

}  // namespace hw_agent::sensors::gpu
