#pragma once

#include <cstdint>
#include <cstdio>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class DiskSensor {
 public:
  struct RawFields {
    std::uint64_t reads_completed{0};
    std::uint64_t writes_completed{0};
    std::uint64_t io_in_progress{0};
    std::uint64_t io_ms{0};
    std::uint64_t weighted_io_ms{0};
    float disk_wait_estimation_ms{0.0F};
  };

  DiskSensor();
  explicit DiskSensor(std::FILE* diskstats, bool owns_file = false);
  ~DiskSensor();

  DiskSensor(const DiskSensor&) = delete;
  DiskSensor& operator=(const DiskSensor&) = delete;

  void sample(model::signal_frame& frame) noexcept;
  const RawFields& raw() const noexcept;

 private:
  static constexpr std::size_t kReadBufferSize = 512;

  std::FILE* diskstats_{nullptr};
  bool owns_file_{true};
  RawFields raw_{};
  std::uint64_t prev_completed_{0};
  std::uint64_t prev_weighted_io_ms_{0};
  bool has_prev_{false};
};

}  // namespace hw_agent::sensors
