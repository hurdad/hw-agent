#pragma once

#include <cstdio>
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
  ~ThermalSensor();

  ThermalSensor(const ThermalSensor&) = delete;
  ThermalSensor& operator=(const ThermalSensor&) = delete;
  ThermalSensor(ThermalSensor&&) = delete;
  ThermalSensor& operator=(ThermalSensor&&) = delete;

  bool sample(model::signal_frame& frame) noexcept;
  void set_throttle_temp_c(float throttle_temp_c) noexcept;
  const RawFields& raw() const noexcept;

 private:
  struct ZoneSource {
    std::string name{};
    std::string temp_path{};
    std::FILE* file{nullptr};
  };

  static bool read_temp_c(std::FILE* file, float& temp_c) noexcept;

  std::vector<ZoneSource> zones_{};
  RawFields raw_{};
};

}  // namespace hw_agent::sensors
