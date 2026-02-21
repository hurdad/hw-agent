#include "sensors/gpu/gpu.hpp"

#include <memory>

namespace hw_agent::sensors::gpu {
namespace {

class NoneGpuSensor final : public GpuSensor {
 public:
  bool available() const override { return false; }

  bool collect(SignalFrame& frame) override {
    frame.gpu_util = 0.0F;
    frame.gpu_mem_util = 0.0F;
    frame.emc_util = 0.0F;
    frame.gpu_mem_free = 0.0F;
    frame.gpu_temp = 0.0F;
    frame.gpu_clock_ratio = 0.0F;
    frame.gpu_power_ratio = 0.0F;
    frame.gpu_throttle = 0.0F;
    frame.nvml_gpu_util = 0.0F;
    frame.nvml_gpu_temp = 0.0F;
    frame.nvml_gpu_power_ratio = 0.0F;
    return false;
  }
};

}  // namespace

std::unique_ptr<GpuSensor> make_none_sensor() { return std::make_unique<NoneGpuSensor>(); }

}  // namespace hw_agent::sensors::gpu
