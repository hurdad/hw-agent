#pragma once

#include <array>
#include <cstdio>

#include "model/signal_frame.hpp"

namespace hw_agent::sensors {

class PsiSensor {
 public:
  struct Source {
    const char* path;
    std::FILE* file;
  };

  PsiSensor();
  explicit PsiSensor(std::array<Source, 3> sources, bool owns_files = false);
  ~PsiSensor();

  PsiSensor(const PsiSensor&) = delete;
  PsiSensor& operator=(const PsiSensor&) = delete;

  bool sample(model::signal_frame& frame) noexcept;

 private:
  static constexpr std::size_t kReadBufferSize = 256;

  bool read_avg10(Source& source, float& value) noexcept;
  static float parse_avg10(const char* data, std::size_t size) noexcept;

  std::array<Source, 3> sources_;
  bool owns_files_{true};
};

}  // namespace hw_agent::sensors
