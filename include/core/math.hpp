#pragma once

#include <algorithm>

namespace hw_agent::core {

inline constexpr float clamp01(const float value) noexcept {
  return std::clamp(value, 0.0F, 1.0F);
}

}  // namespace hw_agent::core
