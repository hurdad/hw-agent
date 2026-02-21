#pragma once

#include <string>
#include <vector>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class ThermalSensor {
 public:
  struct RawFields {
    std::string hottest_zone{};
    float hottest_temp_c{0.0F};
    float throttle_temp_c{85.0F};
    float headroom_c{0.0F};
  };

  explicit ThermalSensor(float throttle_temp_c = 85.0F);
  ~ThermalSensor() = default;

  ThermalSensor(const ThermalSensor&) = delete;
  ThermalSensor& operator=(const ThermalSensor&) = delete;

  void sample(model::signal_frame& frame) noexcept;
  void set_throttle_temp_c(float throttle_temp_c) noexcept;
  const RawFields& raw() const noexcept;

 private:
  static float read_temp_c(const std::string& path) noexcept;

  std::vector<std::string> zone_temp_paths_{};
  std::vector<std::string> zone_names_{};
  RawFields raw_{};
};

}  // namespace hw_agent::sensors
