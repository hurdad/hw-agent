#include "sensors/network.hpp"

#include <filesystem>
#include <memory>

namespace hw_agent::sensors {

namespace {
constexpr const char* kSysClassNet = "/sys/class/net";
}

NetworkSensor::NetworkSensor() {
  try {
    if (!std::filesystem::exists(kSysClassNet)) {
      return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(kSysClassNet)) {
      if (!entry.is_directory()) {
        continue;
      }

      const std::string iface = entry.path().filename().string();
      if (iface == "lo") {
        continue;
      }

      const std::string base = std::string{kSysClassNet} + "/" + iface + "/statistics/";

      auto file_closer = [](std::FILE* file) {
        if (file != nullptr) {
          std::fclose(file);
        }
      };
      using file_ptr = std::unique_ptr<std::FILE, decltype(file_closer)>;

      file_ptr rx_packets_file(std::fopen((base + "rx_packets").c_str(), "r"), file_closer);
      file_ptr tx_packets_file(std::fopen((base + "tx_packets").c_str(), "r"), file_closer);
      file_ptr rx_dropped_file(std::fopen((base + "rx_dropped").c_str(), "r"), file_closer);
      file_ptr tx_dropped_file(std::fopen((base + "tx_dropped").c_str(), "r"), file_closer);

      InterfaceSource source{};
      source.rx_packets_file = rx_packets_file.get();
      source.tx_packets_file = tx_packets_file.get();
      source.rx_dropped_file = rx_dropped_file.get();
      source.tx_dropped_file = tx_dropped_file.get();

      interfaces_.push_back(source);

      rx_packets_file.release();
      tx_packets_file.release();
      rx_dropped_file.release();
      tx_dropped_file.release();
    }
  } catch (const std::filesystem::filesystem_error&) {
    interfaces_.clear();
  }
}

NetworkSensor::~NetworkSensor() {
  for (InterfaceSource& iface : interfaces_) {
    if (iface.rx_packets_file != nullptr) {
      std::fclose(iface.rx_packets_file);
      iface.rx_packets_file = nullptr;
    }
    if (iface.tx_packets_file != nullptr) {
      std::fclose(iface.tx_packets_file);
      iface.tx_packets_file = nullptr;
    }
    if (iface.rx_dropped_file != nullptr) {
      std::fclose(iface.rx_dropped_file);
      iface.rx_dropped_file = nullptr;
    }
    if (iface.tx_dropped_file != nullptr) {
      std::fclose(iface.tx_dropped_file);
      iface.tx_dropped_file = nullptr;
    }
  }
}

bool NetworkSensor::sample(model::signal_frame& frame) noexcept {
  raw_ = {};

  bool all_reads_ok = true;
  for (const InterfaceSource& iface : interfaces_) {
    std::uint64_t value = 0;
    all_reads_ok = read_u64_file(iface.rx_packets_file, value) && all_reads_ok;
    raw_.rx_packets += value;
    all_reads_ok = read_u64_file(iface.tx_packets_file, value) && all_reads_ok;
    raw_.tx_packets += value;
    all_reads_ok = read_u64_file(iface.rx_dropped_file, value) && all_reads_ok;
    raw_.rx_dropped += value;
    all_reads_ok = read_u64_file(iface.tx_dropped_file, value) && all_reads_ok;
    raw_.tx_dropped += value;
  }

  const std::uint64_t total_packets = raw_.rx_packets + raw_.tx_packets;
  const std::uint64_t total_drops = raw_.rx_dropped + raw_.tx_dropped;

  if (!has_prev_) {
    prev_total_packets_ = total_packets;
    prev_total_drops_ = total_drops;
    has_prev_ = true;
    raw_.packet_drop_rate = 0.0F;
  } else {
    const std::uint64_t delta_packets =
        total_packets >= prev_total_packets_ ? (total_packets - prev_total_packets_) : 0;
    const std::uint64_t delta_drops = total_drops >= prev_total_drops_ ? (total_drops - prev_total_drops_) : 0;
    prev_total_packets_ = total_packets;
    prev_total_drops_ = total_drops;

    if (delta_packets == 0) {
      raw_.packet_drop_rate = 0.0F;
    } else {
      raw_.packet_drop_rate = static_cast<float>(delta_drops) / static_cast<float>(delta_packets);
    }
  }

  frame.network = raw_.packet_drop_rate;
  return all_reads_ok;
}

const NetworkSensor::RawFields& NetworkSensor::raw() const noexcept { return raw_; }

bool NetworkSensor::read_u64_file(std::FILE* file, std::uint64_t& value) noexcept {
  if (file == nullptr) {
    value = 0;
    return false;
  }

  if (std::fseek(file, 0L, SEEK_SET) != 0) {
    value = 0;
    return false;
  }

  unsigned long long parsed = 0;
  if (std::fscanf(file, "%llu", &parsed) != 1) {
    std::clearerr(file);
    value = 0;
    return false;
  }

  value = static_cast<std::uint64_t>(parsed);
  return true;
}

}  // namespace hw_agent::sensors
