#pragma once

#include "model/signal_frame.hpp"

namespace hw_agent::sinks {

class StdoutDebugSink {
 public:
  void publish(const model::signal_frame& frame) const;
};

}  // namespace hw_agent::sinks
