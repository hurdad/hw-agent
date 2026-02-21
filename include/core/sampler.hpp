#pragma once

#include <cstdint>

namespace hw_agent::core {

class Sampler {
 public:
  Sampler() = default;

  [[nodiscard]] std::uint64_t tick() const noexcept;

  [[nodiscard]] bool should_sample_every(std::uint64_t every_n_ticks) const noexcept;

  void advance() noexcept;

 private:
  std::uint64_t tick_count_{0};
};

}  // namespace hw_agent::core
