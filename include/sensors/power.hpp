#pragma once

#include <cstdio>
#include <cstdint>
#include <vector>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class PowerSensor {
 public:
  struct RawFields {
    std::uint64_t throttled_policies{0};
    std::uint64_t total_policies{0};
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
  struct PolicySource {
    std::FILE* cur_freq_file{nullptr};
    std::FILE* max_freq_file{nullptr};
  };

  static std::uint64_t read_u64_file(std::FILE* file) noexcept;

  std::vector<PolicySource> policies_{};
  RawFields raw_{};
};

}  // namespace hw_agent::sensors
