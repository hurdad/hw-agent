#include "sensors/network.hpp"

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace hw_agent::sensors {

namespace {
constexpr const char* kSysClassNet = "/sys/class/net";
}

NetworkSensor::NetworkSensor() {
  for (const auto& entry : std::filesystem::directory_iterator(kSysClassNet)) {
    if (!entry.is_directory()) {
      continue;
    }

    const std::string iface = entry.path().filename().string();
    if (iface == "lo") {
      continue;
    }
    interfaces_.push_back(iface);
  }
}

void NetworkSensor::sample(model::signal_frame& frame) noexcept {
  raw_ = {};

  for (const std::string& iface : interfaces_) {
    const std::string base = std::string{kSysClassNet} + "/" + iface + "/statistics/";
    raw_.rx_packets += read_u64_file(base + "rx_packets");
    raw_.tx_packets += read_u64_file(base + "tx_packets");
    raw_.rx_dropped += read_u64_file(base + "rx_dropped");
    raw_.tx_dropped += read_u64_file(base + "tx_dropped");
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

std::uint64_t NetworkSensor::read_u64_file(const std::string& path) noexcept {
  std::ifstream input(path);
  if (!input.is_open()) {
    return 0;
  }

  unsigned long long value = 0;
  input >> value;
  if (!input.good() && !input.eof()) {
    return 0;
  }
  return value;
}

}  // namespace hw_agent::sensors
