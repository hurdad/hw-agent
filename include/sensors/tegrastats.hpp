#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class TegraStatsSensor {
 public:
  struct RawFields {
    float gpu_util_pct{0.0F};
    // EMC_FREQ percentage from tegrastats ([0,100]).
    float emc_util_pct{0.0F};
    float total_rail_power_mw{0.0F};
    std::unordered_map<std::string, float> rail_power_mw{};
    std::unordered_map<std::string, float> temperatures_c{};
  };

  explicit TegraStatsSensor(std::uint32_t interval_ms = 1000U) noexcept;
  ~TegraStatsSensor();

  TegraStatsSensor(const TegraStatsSensor&) = delete;
  TegraStatsSensor& operator=(const TegraStatsSensor&) = delete;

  bool sample(model::signal_frame& frame) noexcept;

  [[nodiscard]] bool enabled() const noexcept;
  [[nodiscard]] const RawFields& raw() const noexcept;

 private:
  bool launch(std::uint32_t interval_ms) noexcept;
  void disable() noexcept;
  bool parse_line(const std::string& line);

  int read_fd_{-1};
  int child_pid_{-1};
  bool enabled_{false};
  std::string read_buffer_{};
  RawFields raw_{};
};

}  // namespace hw_agent::sensors
