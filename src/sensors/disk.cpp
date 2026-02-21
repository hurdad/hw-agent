#include "sensors/disk.hpp"

#include <cctype>
#include <cstring>

namespace hw_agent::sensors {

namespace {

bool is_partition_device(const char* name) noexcept {
  const std::size_t len = std::strlen(name);
  std::size_t digit_start = len;

  while (digit_start > 0 && std::isdigit(static_cast<unsigned char>(name[digit_start - 1])) != 0) {
    --digit_start;
  }

  if (digit_start == len || digit_start == 0) {
    return false;
  }

  const char marker = name[digit_start - 1];
  if (marker == 'p' && digit_start > 1 &&
      std::isdigit(static_cast<unsigned char>(name[digit_start - 2])) != 0) {
    return true;
  }

  return std::isalpha(static_cast<unsigned char>(marker)) != 0;
}

}  // namespace

DiskSensor::DiskSensor() : diskstats_(std::fopen("/proc/diskstats", "r")) {}

DiskSensor::~DiskSensor() {
  if (diskstats_ != nullptr) {
    std::fclose(diskstats_);
    diskstats_ = nullptr;
  }
}

void DiskSensor::sample(model::signal_frame& frame) noexcept {
  if (diskstats_ == nullptr) {
    frame.disk = 0.0F;
    return;
  }

  if (std::fseek(diskstats_, 0L, SEEK_SET) != 0) {
    frame.disk = 0.0F;
    return;
  }

  raw_ = {};

  char buffer[kReadBufferSize]{};
  while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), diskstats_) != nullptr) {
    unsigned int major = 0;
    unsigned int minor = 0;
    char name[64]{};
    unsigned long long reads_completed = 0;
    unsigned long long reads_merged = 0;
    unsigned long long sectors_read = 0;
    unsigned long long ms_reading = 0;
    unsigned long long writes_completed = 0;
    unsigned long long writes_merged = 0;
    unsigned long long sectors_written = 0;
    unsigned long long ms_writing = 0;
    unsigned long long io_in_progress = 0;
    unsigned long long io_ms = 0;
    unsigned long long weighted_io_ms = 0;

    const int parsed = std::sscanf(
        buffer,
        "%u %u %63s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        &major,
        &minor,
        name,
        &reads_completed,
        &reads_merged,
        &sectors_read,
        &ms_reading,
        &writes_completed,
        &writes_merged,
        &sectors_written,
        &ms_writing,
        &io_in_progress,
        &io_ms,
        &weighted_io_ms);

    if (parsed < 14) {
      continue;
    }

    if (std::strncmp(name, "loop", 4) == 0 || std::strncmp(name, "ram", 3) == 0) {
      continue;
    }

    if (is_partition_device(name)) {
      continue;
    }

    raw_.reads_completed += reads_completed;
    raw_.writes_completed += writes_completed;
    raw_.io_in_progress += io_in_progress;
    raw_.io_ms += io_ms;
    raw_.weighted_io_ms += weighted_io_ms;
  }

  if (std::ferror(diskstats_) != 0) {
    std::clearerr(diskstats_);
    frame.disk = 0.0F;
    return;
  }

  const std::uint64_t completed = raw_.reads_completed + raw_.writes_completed;
  if (!has_prev_) {
    prev_completed_ = completed;
    prev_weighted_io_ms_ = raw_.weighted_io_ms;
    has_prev_ = true;
    raw_.disk_wait_estimation_ms = 0.0F;
  } else {
    const std::uint64_t delta_completed = completed - prev_completed_;
    const std::uint64_t delta_weighted_ms = raw_.weighted_io_ms - prev_weighted_io_ms_;
    prev_completed_ = completed;
    prev_weighted_io_ms_ = raw_.weighted_io_ms;

    if (delta_completed == 0) {
      raw_.disk_wait_estimation_ms = 0.0F;
    } else {
      raw_.disk_wait_estimation_ms = static_cast<float>(delta_weighted_ms) /
                                     static_cast<float>(delta_completed);
    }
  }

  frame.disk = raw_.disk_wait_estimation_ms;
}

const DiskSensor::RawFields& DiskSensor::raw() const noexcept { return raw_; }

}  // namespace hw_agent::sensors
