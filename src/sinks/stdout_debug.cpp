#include "sinks/stdout_debug.hpp"

#include <cstdio>

namespace hw_agent::sinks {

void StdoutDebugSink::publish(const model::signal_frame& frame) const {
  std::printf("[raw] cpu.utilization_pct=%.2f memory.dirty_writeback_kb=%.2f disk.wait_ms=%.2f\n",
              frame.cpu, frame.memory, frame.disk);
}

}  // namespace hw_agent::sinks
