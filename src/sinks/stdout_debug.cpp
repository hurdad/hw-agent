#include "sinks/stdout_debug.hpp"

#include <cstdio>

namespace hw_agent::sinks {

void StdoutDebugSink::publish(const model::signal_frame& frame) const {
  std::printf("[psi] cpu.avg10=%.2f memory.avg10=%.2f io.avg10=%.2f\n", frame.cpu, frame.memory,
              frame.disk);
}

}  // namespace hw_agent::sinks
