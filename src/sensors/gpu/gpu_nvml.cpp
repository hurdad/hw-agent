#include "sensors/gpu/gpu.hpp"

#include <cstdint>
#include <dlfcn.h>
#include <memory>

#if defined(HW_AGENT_HAVE_NVML)
#include <nvml.h>
#else
using nvmlReturn_t = int;
using nvmlDevice_t = struct nvmlDevice_st*;

struct nvmlUtilization_t {
  unsigned int gpu;
  unsigned int memory;
};

struct nvmlMemory_t {
  unsigned long long total;
  unsigned long long free;
  unsigned long long used;
};

constexpr nvmlReturn_t NVML_SUCCESS = 0;
constexpr unsigned int NVML_TEMPERATURE_GPU = 0;
constexpr unsigned int NVML_CLOCK_GRAPHICS = 0;
constexpr unsigned long long nvmlClocksThrottleReasonNone = 0x0000000000000000ULL;
#endif

namespace hw_agent::sensors::gpu {
namespace {

class NvmlGpuSensor final : public GpuSensor {
 public:
  explicit NvmlGpuSensor(unsigned int device_index) noexcept : device_index_(device_index) { init(); }

  ~NvmlGpuSensor() override {
    if (initialized_ && fn_shutdown_ != nullptr) {
      (void)fn_shutdown_();
      initialized_ = false;
    }

    if (library_ != nullptr) {
      dlclose(library_);
      library_ = nullptr;
    }
  }

  bool available() const override { return available_; }

  bool collect(SignalFrame& frame) override {
    if (!available_) {
      set_defaults(frame);
      return false;
    }

    nvmlUtilization_t util{};
    nvmlMemory_t memory{};
    unsigned int temp_c = 0;
    unsigned int graphics_clock_mhz = 0;
    unsigned int power_mw = 0;
    unsigned long long throttle_reasons = 0;

    if (fn_device_get_utilization_rates_(device_, &util) != NVML_SUCCESS ||
        fn_device_get_memory_info_(device_, &memory) != NVML_SUCCESS ||
        fn_device_get_temperature_(device_, NVML_TEMPERATURE_GPU, &temp_c) != NVML_SUCCESS ||
        fn_device_get_clock_info_(device_, NVML_CLOCK_GRAPHICS, &graphics_clock_mhz) != NVML_SUCCESS ||
        fn_device_get_power_usage_(device_, &power_mw) != NVML_SUCCESS ||
        fn_device_get_current_clocks_throttle_reasons_(device_, &throttle_reasons) != NVML_SUCCESS) {
      set_defaults(frame);
      return false;
    }

    frame.gpu_util = static_cast<float>(util.gpu);
    frame.nvml_gpu_util = static_cast<float>(util.gpu);
    frame.gpu_mem_util = static_cast<float>(util.memory);
    frame.emc_util = 0.0F;
    frame.gpu_mem_free = static_cast<float>(memory.free / (1024ULL * 1024ULL));
    frame.gpu_temp = static_cast<float>(temp_c);
    frame.nvml_gpu_temp = static_cast<float>(temp_c);
    frame.gpu_clock_ratio = max_graphics_clock_mhz_ > 0U
                                ? static_cast<float>(graphics_clock_mhz) / static_cast<float>(max_graphics_clock_mhz_)
                                : 0.0F;
    frame.gpu_power_ratio = power_limit_mw_ > 0U ? static_cast<float>(power_mw) / static_cast<float>(power_limit_mw_)
                                                 : 0.0F;
    frame.nvml_gpu_power_ratio = frame.gpu_power_ratio;
    frame.gpu_throttle = throttle_reasons == nvmlClocksThrottleReasonNone ? 0.0F : 1.0F;

    return true;
  }

 private:
  using FnNvmlInit = nvmlReturn_t (*)();
  using FnNvmlShutdown = nvmlReturn_t (*)();
  using FnNvmlDeviceGetHandleByIndex = nvmlReturn_t (*)(unsigned int, nvmlDevice_t*);
  using FnNvmlDeviceGetUtilizationRates = nvmlReturn_t (*)(nvmlDevice_t, nvmlUtilization_t*);
  using FnNvmlDeviceGetMemoryInfo = nvmlReturn_t (*)(nvmlDevice_t, nvmlMemory_t*);
  using FnNvmlDeviceGetTemperature = nvmlReturn_t (*)(nvmlDevice_t, unsigned int, unsigned int*);
  using FnNvmlDeviceGetClockInfo = nvmlReturn_t (*)(nvmlDevice_t, unsigned int, unsigned int*);
  using FnNvmlDeviceGetMaxClockInfo = nvmlReturn_t (*)(nvmlDevice_t, unsigned int, unsigned int*);
  using FnNvmlDeviceGetPowerUsage = nvmlReturn_t (*)(nvmlDevice_t, unsigned int*);
  using FnNvmlDeviceGetEnforcedPowerLimit = nvmlReturn_t (*)(nvmlDevice_t, unsigned int*);
  using FnNvmlDeviceGetCurrentClocksThrottleReasons = nvmlReturn_t (*)(nvmlDevice_t, unsigned long long*);

  template <typename FnType>
  bool resolve(FnType& fn, const char* symbol) noexcept {
    fn = reinterpret_cast<FnType>(dlsym(library_, symbol));
    return fn != nullptr;
  }

  void init() noexcept {
    library_ = dlopen("libnvidia-ml.so.1", RTLD_NOW);
    if (library_ == nullptr) {
      return;
    }

    if (!resolve(fn_init_, "nvmlInit_v2") && !resolve(fn_init_, "nvmlInit")) {
      return;
    }

    if (!resolve(fn_shutdown_, "nvmlShutdown")) {
      return;
    }

    if (!resolve(fn_device_get_handle_by_index_, "nvmlDeviceGetHandleByIndex_v2") &&
        !resolve(fn_device_get_handle_by_index_, "nvmlDeviceGetHandleByIndex")) {
      return;
    }

    if (!resolve(fn_device_get_utilization_rates_, "nvmlDeviceGetUtilizationRates") ||
        !resolve(fn_device_get_memory_info_, "nvmlDeviceGetMemoryInfo") ||
        !resolve(fn_device_get_temperature_, "nvmlDeviceGetTemperature") ||
        !resolve(fn_device_get_clock_info_, "nvmlDeviceGetClockInfo") ||
        !resolve(fn_device_get_max_clock_info_, "nvmlDeviceGetMaxClockInfo") ||
        !resolve(fn_device_get_power_usage_, "nvmlDeviceGetPowerUsage") ||
        !resolve(fn_device_get_enforced_power_limit_, "nvmlDeviceGetEnforcedPowerLimit") ||
        !resolve(fn_device_get_current_clocks_throttle_reasons_, "nvmlDeviceGetCurrentClocksThrottleReasons")) {
      return;
    }

    if (fn_init_() != NVML_SUCCESS) {
      return;
    }
    initialized_ = true;

    if (fn_device_get_handle_by_index_(device_index_, &device_) != NVML_SUCCESS) {
      return;
    }

    (void)fn_device_get_max_clock_info_(device_, NVML_CLOCK_GRAPHICS, &max_graphics_clock_mhz_);
    (void)fn_device_get_enforced_power_limit_(device_, &power_limit_mw_);
    available_ = true;
  }

  static void set_defaults(SignalFrame& frame) noexcept {
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
  }

  void* library_{nullptr};
  bool available_{false};
  bool initialized_{false};
  unsigned int device_index_{0};
  nvmlDevice_t device_{nullptr};
  unsigned int max_graphics_clock_mhz_{0};
  unsigned int power_limit_mw_{0};

  FnNvmlInit fn_init_{nullptr};
  FnNvmlShutdown fn_shutdown_{nullptr};
  FnNvmlDeviceGetHandleByIndex fn_device_get_handle_by_index_{nullptr};
  FnNvmlDeviceGetUtilizationRates fn_device_get_utilization_rates_{nullptr};
  FnNvmlDeviceGetMemoryInfo fn_device_get_memory_info_{nullptr};
  FnNvmlDeviceGetTemperature fn_device_get_temperature_{nullptr};
  FnNvmlDeviceGetClockInfo fn_device_get_clock_info_{nullptr};
  FnNvmlDeviceGetMaxClockInfo fn_device_get_max_clock_info_{nullptr};
  FnNvmlDeviceGetPowerUsage fn_device_get_power_usage_{nullptr};
  FnNvmlDeviceGetEnforcedPowerLimit fn_device_get_enforced_power_limit_{nullptr};
  FnNvmlDeviceGetCurrentClocksThrottleReasons fn_device_get_current_clocks_throttle_reasons_{nullptr};
};

}  // namespace

std::unique_ptr<GpuSensor> make_nvml_sensor(unsigned int device_index) {
  return std::make_unique<NvmlGpuSensor>(device_index);
}

}  // namespace hw_agent::sensors::gpu
