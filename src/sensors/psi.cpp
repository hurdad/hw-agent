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

bool PsiSensor::sample(model::signal_frame& frame) noexcept {
  float cpu_avg10 = 0.0F;
  float memory_avg10 = 0.0F;
  float io_avg10 = 0.0F;
  const bool cpu_ok = read_avg10(sources_[0], cpu_avg10);
  const bool memory_ok = read_avg10(sources_[1], memory_avg10);
  const bool io_ok = read_avg10(sources_[2], io_avg10);

  frame.psi = cpu_avg10;
  frame.psi_memory = memory_avg10;
  frame.psi_io = io_avg10;
  return cpu_ok && memory_ok && io_ok;
}

bool PsiSensor::read_avg10(Source& source, float& value) noexcept {
  if (source.file == nullptr) {
    value = 0.0F;
    return false;
  }

  if (std::fseek(source.file, 0L, SEEK_SET) != 0) {
    value = 0.0F;
    return false;
  }

  char buffer[kReadBufferSize]{};
  const std::size_t bytes_read = std::fread(buffer, 1, sizeof(buffer) - 1, source.file);
  if (bytes_read == 0U) {
    if (std::ferror(source.file) != 0) {
      std::clearerr(source.file);
    }
    value = 0.0F;
    return false;
  }

  value = parse_avg10(buffer, bytes_read);
  return true;
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
