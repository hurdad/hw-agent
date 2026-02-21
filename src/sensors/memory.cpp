#include "sensors/memory.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace hw_agent::sensors {

MemorySensor::MemorySensor() : meminfo_(std::fopen("/proc/meminfo", "r")), vmstat_(std::fopen("/proc/vmstat", "r")) {}

MemorySensor::MemorySensor(std::FILE* meminfo, std::FILE* vmstat, const bool owns_files)
    : meminfo_(meminfo), vmstat_(vmstat), owns_files_(owns_files) {}

MemorySensor::~MemorySensor() {
  if (owns_files_ && meminfo_ != nullptr) {
    std::fclose(meminfo_);
    meminfo_ = nullptr;
  }
  if (owns_files_ && vmstat_ != nullptr) {
    std::fclose(vmstat_);
    vmstat_ = nullptr;
  }
}

bool MemorySensor::sample(model::signal_frame& frame) noexcept {
  const bool meminfo_ok = parse_meminfo();
  const bool vmstat_ok = parse_vmstat();

  if (!meminfo_ok || !vmstat_ok) {
    frame.memory = 0.0F;
    return false;
  }

  const std::uint64_t dirty_and_writeback = raw_.dirty_kb + raw_.writeback_kb;
  raw_.dirty_writeback_pressure = static_cast<float>(dirty_and_writeback);

  if (!has_prev_) {
    prev_pgsteal_total_ = raw_.pgsteal_total;
    has_prev_ = true;
    raw_.reclaim_activity = 0.0F;
  } else {
    if (raw_.pgsteal_total >= prev_pgsteal_total_) {
      raw_.reclaim_activity = static_cast<float>(raw_.pgsteal_total - prev_pgsteal_total_);
    } else {
      raw_.reclaim_activity = 0.0F;
    }
    prev_pgsteal_total_ = raw_.pgsteal_total;
  }

  frame.memory = raw_.dirty_writeback_pressure;
  return true;
}

const MemorySensor::RawFields& MemorySensor::raw() const noexcept { return raw_; }

bool MemorySensor::parse_meminfo() noexcept {
  if (meminfo_ == nullptr) {
    return false;
  }

  if (std::fseek(meminfo_, 0L, SEEK_SET) != 0) {
    return false;
  }

  raw_.mem_total_kb = 0;
  raw_.mem_available_kb = 0;
  raw_.dirty_kb = 0;
  raw_.writeback_kb = 0;

  char buffer[kReadBufferSize]{};
  while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), meminfo_) != nullptr) {
    char key[64]{};
    unsigned long long value = 0;
    if (std::sscanf(buffer, "%63[^:]: %llu kB", key, &value) != 2) {
      continue;
    }

    if (std::strcmp(key, "MemTotal") == 0) {
      raw_.mem_total_kb = value;
    } else if (std::strcmp(key, "MemAvailable") == 0) {
      raw_.mem_available_kb = value;
    } else if (std::strcmp(key, "Dirty") == 0) {
      raw_.dirty_kb = value;
    } else if (std::strcmp(key, "Writeback") == 0) {
      raw_.writeback_kb = value;
    }
  }

  if (std::ferror(meminfo_) != 0) {
    std::clearerr(meminfo_);
    return false;
  }

  return raw_.mem_total_kb != 0;
}

bool MemorySensor::parse_vmstat() noexcept {
  if (vmstat_ == nullptr) {
    return false;
  }

  if (std::fseek(vmstat_, 0L, SEEK_SET) != 0) {
    return false;
  }

  std::uint64_t pgscan_kswapd = 0;
  std::uint64_t pgscan_direct = 0;
  std::uint64_t pgsteal_kswapd = 0;
  std::uint64_t pgsteal_direct = 0;

  char buffer[kReadBufferSize]{};
  while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), vmstat_) != nullptr) {
    char key[64]{};
    unsigned long long value = 0;
    if (std::sscanf(buffer, "%63s %llu", key, &value) != 2) {
      continue;
    }

    if (std::strcmp(key, "pgscan_kswapd") == 0) {
      pgscan_kswapd = value;
    } else if (std::strcmp(key, "pgscan_direct") == 0) {
      pgscan_direct = value;
    } else if (std::strcmp(key, "pgsteal_kswapd") == 0) {
      pgsteal_kswapd = value;
    } else if (std::strcmp(key, "pgsteal_direct") == 0) {
      pgsteal_direct = value;
    }
  }

  if (std::ferror(vmstat_) != 0) {
    std::clearerr(vmstat_);
    return false;
  }

  raw_.pgscan_total = pgscan_kswapd + pgscan_direct;
  raw_.pgsteal_total = pgsteal_kswapd + pgsteal_direct;
  return true;
}

}  // namespace hw_agent::sensors
