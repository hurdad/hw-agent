#pragma once

#include "model/signal_frame.hpp"

namespace hw_agent::risk {

class SystemState {
 public:
  void sample(model::signal_frame& frame) noexcept;

 private:
  model::system_state state_{model::system_state::STABLE};
};

}  // namespace hw_agent::risk
