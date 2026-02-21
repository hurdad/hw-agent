#include "risk/system_state.hpp"

namespace hw_agent::risk {

namespace {
float max_risk(const model::signal_frame& frame) {
  return frame.realtime_risk > frame.saturation_risk ? frame.realtime_risk : frame.saturation_risk;
}
}  // namespace

void SystemState::sample(model::signal_frame& frame) noexcept {
  const float risk = max_risk(frame);

  switch (state_) {
    case model::system_state::STABLE:
      if (risk >= 0.45F) {
        state_ = model::system_state::DEGRADED;
      }
      break;

    case model::system_state::DEGRADED:
      if (risk >= 0.70F) {
        state_ = model::system_state::UNSTABLE;
      } else if (risk <= 0.30F) {
        state_ = model::system_state::STABLE;
      }
      break;

    case model::system_state::UNSTABLE:
      if (risk >= 0.88F) {
        state_ = model::system_state::CRITICAL;
      } else if (risk <= 0.55F) {
        state_ = model::system_state::DEGRADED;
      }
      break;

    case model::system_state::CRITICAL:
      if (risk <= 0.78F) {
        state_ = model::system_state::UNSTABLE;
      }
      break;
  }

  frame.state = state_;
}

}  // namespace hw_agent::risk
