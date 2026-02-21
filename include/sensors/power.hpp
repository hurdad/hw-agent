#pragma once

#include <cstdint>
#include <string>
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
  ~PowerSensor() = default;

  PowerSensor(const PowerSensor&) = delete;
  PowerSensor& operator=(const PowerSensor&) = delete;

  void sample(model::signal_frame& frame) noexcept;
  const RawFields& raw() const noexcept;

 private:
  static std::uint64_t read_u64_file(const std::string& path) noexcept;

  std::vector<std::string> policy_paths_{};
  RawFields raw_{};
};

}  // namespace hw_agent::sensors
