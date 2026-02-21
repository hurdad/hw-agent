#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class NetworkSensor {
 public:
  struct RawFields {
    std::uint64_t rx_packets{0};
    std::uint64_t tx_packets{0};
    std::uint64_t rx_dropped{0};
    std::uint64_t tx_dropped{0};
    float packet_drop_rate{0.0F};
  };

  NetworkSensor();
  ~NetworkSensor() = default;

  NetworkSensor(const NetworkSensor&) = delete;
  NetworkSensor& operator=(const NetworkSensor&) = delete;

  void sample(model::signal_frame& frame) noexcept;
  const RawFields& raw() const noexcept;

 private:
  static std::uint64_t read_u64_file(const std::string& path) noexcept;

  std::vector<std::string> interfaces_;
  RawFields raw_{};
  std::uint64_t prev_total_packets_{0};
  std::uint64_t prev_total_drops_{0};
  bool has_prev_{false};
};

}  // namespace hw_agent::sensors
