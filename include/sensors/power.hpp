#pragma once

#include <cstdio>
#include <cstdint>
#include <vector>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class CpuThrottleSensor {
 public:
  struct RawFields {
    std::uint64_t throttled_cores{0};
    std::uint64_t total_cores{0};
    float cpu_throttle_ratio{0.0F};
  };

  struct ThermalThrottleSource {
    std::FILE* core_throttle_count_file{nullptr};
    std::FILE* package_throttle_count_file{nullptr};

    std::uint64_t prev_core_throttle_count{0};
    std::uint64_t prev_package_throttle_count{0};

    bool has_prev_counts{false};
  };

  CpuThrottleSensor();
  explicit CpuThrottleSensor(std::vector<ThermalThrottleSource> cores, bool owns_files = false);
  ~CpuThrottleSensor();

  CpuThrottleSensor(const CpuThrottleSensor&) = delete;
  CpuThrottleSensor& operator=(const CpuThrottleSensor&) = delete;
  CpuThrottleSensor(CpuThrottleSensor&&) = delete;
  CpuThrottleSensor& operator=(CpuThrottleSensor&&) = delete;

  bool sample(model::signal_frame& frame) noexcept;
  const RawFields& raw() const noexcept;

 private:
  static bool read_u64_file(std::FILE* file, std::uint64_t& value) noexcept;

  std::vector<ThermalThrottleSource> cores_{};
  bool owns_files_{true};
  RawFields raw_{};
};

}  // namespace hw_agent::sensors
