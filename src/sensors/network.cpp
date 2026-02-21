#include "sensors/network.hpp"

#include <filesystem>

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

      InterfaceSource source{};
      source.rx_packets_file = std::fopen((base + "rx_packets").c_str(), "r");
      source.tx_packets_file = std::fopen((base + "tx_packets").c_str(), "r");
      source.rx_dropped_file = std::fopen((base + "rx_dropped").c_str(), "r");
      source.tx_dropped_file = std::fopen((base + "tx_dropped").c_str(), "r");

      interfaces_.push_back(source);
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

void NetworkSensor::sample(model::signal_frame& frame) noexcept {
  raw_ = {};

  for (const InterfaceSource& iface : interfaces_) {
    raw_.rx_packets += read_u64_file(iface.rx_packets_file);
    raw_.tx_packets += read_u64_file(iface.tx_packets_file);
    raw_.rx_dropped += read_u64_file(iface.rx_dropped_file);
    raw_.tx_dropped += read_u64_file(iface.tx_dropped_file);
  }

  const std::uint64_t total_packets = raw_.rx_packets + raw_.tx_packets;
  const std::uint64_t total_drops = raw_.rx_dropped + raw_.tx_dropped;

  if (!has_prev_) {
    prev_total_packets_ = total_packets;
    prev_total_drops_ = total_drops;
    has_prev_ = true;
    raw_.packet_drop_rate = 0.0F;
  } else {
    const std::uint64_t delta_packets = total_packets - prev_total_packets_;
    const std::uint64_t delta_drops = total_drops - prev_total_drops_;
    prev_total_packets_ = total_packets;
    prev_total_drops_ = total_drops;

    if (delta_packets == 0) {
      raw_.packet_drop_rate = 0.0F;
    } else {
      raw_.packet_drop_rate = static_cast<float>(delta_drops) / static_cast<float>(delta_packets);
    }
  }

  frame.network = raw_.packet_drop_rate;
}

const NetworkSensor::RawFields& NetworkSensor::raw() const noexcept { return raw_; }

std::uint64_t NetworkSensor::read_u64_file(std::FILE* file) noexcept {
  if (file == nullptr) {
    return 0;
  }

  if (std::fseek(file, 0L, SEEK_SET) != 0) {
    return 0;
  }

  unsigned long long value = 0;
  if (std::fscanf(file, "%llu", &value) != 1) {
    std::clearerr(file);
    return 0;
  }

  return value;
}

}  // namespace hw_agent::sensors
