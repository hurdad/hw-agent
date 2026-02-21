#include "sensors/psi.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace hw_agent::sensors {

PsiSensor::PsiSensor()
    : sources_{{
          {"/proc/pressure/cpu", std::fopen("/proc/pressure/cpu", "r")},
          {"/proc/pressure/memory", std::fopen("/proc/pressure/memory", "r")},
          {"/proc/pressure/io", std::fopen("/proc/pressure/io", "r")},
      }} {}

PsiSensor::~PsiSensor() {
  for (auto& source : sources_) {
    if (source.file != nullptr) {
      std::fclose(source.file);
      source.file = nullptr;
    }
  }
}

void PsiSensor::sample(model::signal_frame& frame) noexcept {
  const float cpu_avg10 = read_avg10(sources_[0]);
  const float memory_avg10 = read_avg10(sources_[1]);
  const float io_avg10 = read_avg10(sources_[2]);

  frame.psi = cpu_avg10;
  frame.cpu = cpu_avg10;
  frame.memory = memory_avg10;
  frame.disk = io_avg10;
}

float PsiSensor::read_avg10(Source& source) noexcept {
  if (source.file == nullptr) {
    return 0.0F;
  }

  if (std::fseek(source.file, 0L, SEEK_SET) != 0) {
    return 0.0F;
  }

  char buffer[kReadBufferSize]{};
  const std::size_t bytes_read = std::fread(buffer, 1, sizeof(buffer) - 1, source.file);
  if (bytes_read == 0U) {
    if (std::ferror(source.file) != 0) {
      std::clearerr(source.file);
    }
    return 0.0F;
  }

  return parse_avg10(buffer, bytes_read);
}

float PsiSensor::parse_avg10(const char* data, const std::size_t size) noexcept {
  constexpr char needle[] = "avg10=";
  constexpr std::size_t needle_size = sizeof(needle) - 1;

  for (std::size_t i = 0; (i + needle_size) <= size; ++i) {
    if (std::memcmp(data + i, needle, needle_size) != 0) {
      continue;
    }

    const char* value_begin = data + i + needle_size;
    char* value_end = nullptr;
    errno = 0;
    const float parsed = std::strtof(value_begin, &value_end);
    if (errno != 0 || value_end == value_begin) {
      return 0.0F;
    }
    return parsed;
  }

  return 0.0F;
}

}  // namespace hw_agent::sensors
