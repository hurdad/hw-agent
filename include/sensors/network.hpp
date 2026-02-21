#pragma once

#include <cstdio>
#include <cstdint>
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
  ~NetworkSensor();

  NetworkSensor(const NetworkSensor&) = delete;
  NetworkSensor& operator=(const NetworkSensor&) = delete;
  NetworkSensor(NetworkSensor&&) = delete;
  NetworkSensor& operator=(NetworkSensor&&) = delete;

  bool sample(model::signal_frame& frame) noexcept;
  const RawFields& raw() const noexcept;

 private:
  struct InterfaceSource {
    std::FILE* rx_packets_file{nullptr};
    std::FILE* tx_packets_file{nullptr};
    std::FILE* rx_dropped_file{nullptr};
    std::FILE* tx_dropped_file{nullptr};
  };

  static bool read_u64_file(std::FILE* file, std::uint64_t& value) noexcept;

  std::vector<InterfaceSource> interfaces_{};
  RawFields raw_{};
  std::uint64_t prev_total_packets_{0};
  std::uint64_t prev_total_drops_{0};
  bool has_prev_{false};
};

}  // namespace hw_agent::sensors
