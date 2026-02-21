#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class MemorySensor {
 public:
  struct RawFields {
    std::uint64_t mem_total_kb{0};
    std::uint64_t mem_available_kb{0};
    std::uint64_t dirty_kb{0};
    std::uint64_t writeback_kb{0};
    std::uint64_t pgscan_total{0};
    std::uint64_t pgsteal_total{0};
    float reclaim_activity{0.0F};
    float dirty_writeback_pressure{0.0F};
  };

  MemorySensor();
  ~MemorySensor();

  MemorySensor(const MemorySensor&) = delete;
  MemorySensor& operator=(const MemorySensor&) = delete;

  void sample(model::signal_frame& frame) noexcept;
  const RawFields& raw() const noexcept;

 private:
  static constexpr std::size_t kReadBufferSize = 512;

  bool parse_meminfo() noexcept;
  bool parse_vmstat() noexcept;

  std::FILE* meminfo_{nullptr};
  std::FILE* vmstat_{nullptr};
  RawFields raw_{};
  std::uint64_t prev_pgsteal_total_{0};
  bool has_prev_{false};
};

}  // namespace hw_agent::sensors
