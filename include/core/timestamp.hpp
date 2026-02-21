#pragma once

#include <chrono>
#include <cstdint>

namespace hw_agent::core {

inline std::uint64_t monotonic_timestamp_now_ns() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

inline std::uint64_t unix_timestamp_now_ns() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

}  // namespace hw_agent::core
