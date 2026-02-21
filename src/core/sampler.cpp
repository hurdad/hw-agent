#include "core/sampler.hpp"

namespace hw_agent::core {

std::uint64_t Sampler::tick() const noexcept { return tick_count_; }

bool Sampler::should_sample_every(const std::uint64_t every_n_ticks) const noexcept {
  if (every_n_ticks == 0) {
    return false;
  }
  return (tick_count_ % every_n_ticks) == 0;
}

void Sampler::advance() noexcept { ++tick_count_; }

}  // namespace hw_agent::core
