#pragma once

#include <array>
#include <cstdio>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class PsiSensor {
 public:
  PsiSensor();
  ~PsiSensor();

  PsiSensor(const PsiSensor&) = delete;
  PsiSensor& operator=(const PsiSensor&) = delete;

  void sample(model::signal_frame& frame) noexcept;

 private:
  static constexpr std::size_t kReadBufferSize = 256;

  struct Source {
    const char* path;
    std::FILE* file;
  };

  float read_avg10(Source& source) noexcept;
  static float parse_avg10(const char* data, std::size_t size) noexcept;

  std::array<Source, 3> sources_;
};

}  // namespace hw_agent::sensors
