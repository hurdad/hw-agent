#pragma once

#include <cstdio>
#include <cstdint>
#include <vector>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class PowerSensor {
 public:
  struct RawFields {
    std::uint64_t throttled_cores{0};
    std::uint64_t total_cores{0};
    float throttle_ratio{0.0F};
  };

  PowerSensor();
  ~PowerSensor();

  PowerSensor(const PowerSensor&) = delete;
  PowerSensor& operator=(const PowerSensor&) = delete;
  PowerSensor(PowerSensor&&) = delete;
  PowerSensor& operator=(PowerSensor&&) = delete;

  void sample(model::signal_frame& frame) noexcept;
  const RawFields& raw() const noexcept;

 private:
  struct ThermalThrottleSource {
    std::FILE* core_throttle_count_file{nullptr};
    std::FILE* package_throttle_count_file{nullptr};

    std::uint64_t prev_core_throttle_count{0};
    std::uint64_t prev_package_throttle_count{0};

    bool has_prev_counts{false};
  };

  static std::uint64_t read_u64_file(std::FILE* file) noexcept;

  std::vector<ThermalThrottleSource> cores_{};
  RawFields raw_{};
};

}  // namespace hw_agent::sensors
