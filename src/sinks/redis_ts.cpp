#include "sinks/redis_ts.hpp"

#include "core/timestamp.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <hiredis/hiredis.h>

namespace hw_agent::sinks {
namespace {

constexpr std::size_t kMetricCountBase = 28;
constexpr std::size_t kMetricCountHealth = 7;
constexpr std::size_t kMaxMetricCount = kMetricCountBase + kMetricCountHealth;
constexpr std::size_t kMaxCommandArgCount = 1 + (kMaxMetricCount * 3);

double sanitize_value(const float value) {
  return std::isfinite(value) ? static_cast<double>(value) : 0.0;
}

void add_metric_args(std::vector<std::string>& args, const std::string& key_prefix,
                     const std::uint64_t timestamp_ms, const char* suffix, const double value) {
  args.emplace_back(key_prefix + ":" + suffix);
  args.emplace_back(std::to_string(timestamp_ms));
  args.emplace_back(std::to_string(value));
}

const std::vector<std::string>& default_metric_suffixes() {
  static const std::vector<std::string> kMetricSuffixes = {
      "raw:psi",
      "raw:psi_memory",
      "raw:psi_io",
      "raw:cpu",
      "raw:irq",
      "raw:softirqs",
      "raw:memory",
      "raw:thermal",
      "raw:cpufreq",
      "raw:cpu_throttle_ratio",
      "raw:disk",
      "raw:network",
      "raw:nvml_gpu_util",
      "raw:tegra_emc_util",
      "raw:nvml_gpu_temp",
      "raw:nvml_gpu_power_ratio",
      "raw:tegra_gpu_util",
      "raw:tegra_gpu_temp",
      "raw:tegra_gpu_power_mw",
      "derived:scheduler_pressure",
      "derived:memory_pressure",
      "derived:io_pressure",
      "derived:thermal_pressure",
      "derived:power_pressure",
      "derived:latency_jitter",
      "risk:realtime_risk",
      "risk:saturation_risk",
      "risk:state",
      "agent:heartbeat",
      "agent:loop_jitter",
      "agent:compute_time",
      "agent:redis_latency",
      "agent:redis_errors",
      "agent:sensor_failures",
      "agent:missed_cycles",
  };
  return kMetricSuffixes;
}

}  // namespace

RedisTsSink::RedisTsSink(RedisTsOptions options) : options_(std::move(options)) {
  enabled_metrics_ = options_.enabled_metrics.empty() ? default_metric_suffixes() : options_.enabled_metrics;
  enabled_metric_set_ = std::unordered_set<std::string>(enabled_metrics_.begin(), enabled_metrics_.end());
  reserve_command_buffers();
}

RedisTsSink::~RedisTsSink() = default;

RedisTsSink::RedisTsSink(RedisTsSink&&) noexcept = default;
RedisTsSink& RedisTsSink::operator=(RedisTsSink&&) noexcept = default;

bool RedisTsSink::check_connectivity() {
  return ensure_connected();
}

void RedisTsSink::ContextDeleter::operator()(redisContext* context) const {
  if (context != nullptr) {
    redisFree(context);
  }
}

bool RedisTsSink::ensure_connected() {
  if (!timeseries_available_) {
    return false;
  }

  if (context_ != nullptr && context_->err == REDIS_OK) {
    return true;
  }
  return reconnect();
}

bool RedisTsSink::reconnect() {
  context_.reset();

  timeval timeout{};
  timeout.tv_sec = static_cast<time_t>(options_.connect_timeout_ms / 1000);
  timeout.tv_usec = static_cast<suseconds_t>((options_.connect_timeout_ms % 1000) * 1000);

  redisContext* raw = nullptr;
  if (!options_.unix_socket.empty()) {
    raw = redisConnectUnixWithTimeout(options_.unix_socket.c_str(), timeout);
  } else {
    raw = redisConnectWithTimeout(options_.host.c_str(), static_cast<int>(options_.port), timeout);
  }
  if (raw == nullptr || raw->err != REDIS_OK) {
    if (raw != nullptr) {
      std::cerr << "[redis] connect failed: " << raw->errstr << '\n';
      redisFree(raw);
    } else {
      std::cerr << "[redis] connect failed: out of memory\n";
    }
    return false;
  }

  context_.reset(raw);
  if (!authenticate()) {
    context_.reset();
    return false;
  }
  if (!select_db()) {
    context_.reset();
    return false;
  }
  if (!ensure_schema()) {
    context_.reset();
    return false;
  }

  return true;
}

bool RedisTsSink::authenticate() {
  if (options_.password.empty()) {
    return true;
  }

  redisReply* reply = static_cast<redisReply*>(redisCommand(context_.get(), "AUTH %s", options_.password.c_str()));
  if (reply == nullptr) {
    return false;
  }
  const bool ok = reply->type != REDIS_REPLY_ERROR;
  if (!ok) {
    std::cerr << "[redis] AUTH rejected\n";
  }
  freeReplyObject(reply);
  return ok;
}

bool RedisTsSink::select_db() {
  if (options_.db == 0) {
    return true;
  }

  redisReply* reply = static_cast<redisReply*>(redisCommand(context_.get(), "SELECT %d", options_.db));
  if (reply == nullptr) {
    return false;
  }
  const bool ok = reply->type != REDIS_REPLY_ERROR;
  freeReplyObject(reply);
  return ok;
}

bool RedisTsSink::ensure_schema() {
  if (schema_ready_) {
    return true;
  }

  for (const auto& suffix : enabled_metrics_) {
    const std::string key = options_.key_prefix + ":" + suffix;
    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_.get(), "TS.CREATE %s DUPLICATE_POLICY LAST", key.c_str()));
    if (reply == nullptr) {
      return false;
    }

    const bool already_exists =
        reply->type == REDIS_REPLY_ERROR && reply->str != nullptr && strstr(reply->str, "already exists") != nullptr;
    const bool unknown_command =
        reply->type == REDIS_REPLY_ERROR && reply->str != nullptr && strstr(reply->str, "unknown command") != nullptr;
    const bool ok = reply->type != REDIS_REPLY_ERROR || already_exists;
    const std::string reply_message = reply->str != nullptr ? reply->str : "unknown";
    freeReplyObject(reply);

    if (unknown_command) {
      std::cerr << "[redis] RedisTimeSeries module not available (TS.CREATE unknown command)\n";
      timeseries_available_ = false;
      return false;
    }
    if (!ok) {
      std::cerr << "[redis] schema error on TS.CREATE " << key << ": " << reply_message << '\n';
      return false;
    }
  }

  schema_ready_ = true;
  return true;
}

bool RedisTsSink::publish(model::signal_frame& frame) {
  if (!ensure_connected()) {
    return false;
  }

  if (publish_impl(frame)) {
    return true;
  }

  if (!reconnect()) {
    return false;
  }
  return publish_impl(frame);
}

bool RedisTsSink::publish_impl(model::signal_frame& frame) {
  const std::uint64_t timestamp_ms = core::unix_timestamp_now_ns() / 1'000'000ULL;

  command_args_.clear();
  command_argv_.clear();
  command_argv_len_.clear();
  command_args_.emplace_back("TS.MADD");

  const auto append_metric = [&](const char* suffix, const double value) {
    if (enabled_metric_set_.find(suffix) == enabled_metric_set_.end()) {
      return;
    }
    add_metric_args(command_args_, options_.key_prefix, timestamp_ms, suffix, value);
  };

  append_metric("raw:psi", sanitize_value(frame.psi));
  append_metric("raw:psi_memory", sanitize_value(frame.psi_memory));
  append_metric("raw:psi_io", sanitize_value(frame.psi_io));
  append_metric("raw:cpu", sanitize_value(frame.cpu));
  append_metric("raw:irq", sanitize_value(frame.irq));
  append_metric("raw:softirqs", sanitize_value(frame.softirqs));
  append_metric("raw:memory", sanitize_value(frame.memory));
  append_metric("raw:thermal", sanitize_value(frame.thermal));
  append_metric("raw:cpufreq", sanitize_value(frame.cpufreq));
  append_metric("raw:cpu_throttle_ratio", sanitize_value(frame.cpu_throttle_ratio));
  append_metric("raw:disk", sanitize_value(frame.disk));
  append_metric("raw:network", sanitize_value(frame.network));
  append_metric("raw:nvml_gpu_util", sanitize_value(frame.nvml_gpu_util));
  append_metric("raw:tegra_emc_util", sanitize_value(frame.tegra_emc_util));
  append_metric("raw:nvml_gpu_temp", sanitize_value(frame.nvml_gpu_temp));
  append_metric("raw:nvml_gpu_power_ratio", sanitize_value(frame.nvml_gpu_power_ratio));
  append_metric("raw:tegra_gpu_util", sanitize_value(frame.tegra_gpu_util));
  append_metric("raw:tegra_gpu_temp", sanitize_value(frame.tegra_gpu_temp));
  append_metric("raw:tegra_gpu_power_mw", sanitize_value(frame.tegra_gpu_power_mw));
  append_metric("derived:scheduler_pressure", sanitize_value(frame.scheduler_pressure));
  append_metric("derived:memory_pressure", sanitize_value(frame.memory_pressure));
  append_metric("derived:io_pressure", sanitize_value(frame.io_pressure));
  append_metric("derived:thermal_pressure", sanitize_value(frame.thermal_pressure));
  append_metric("derived:power_pressure", sanitize_value(frame.power_pressure));
  append_metric("derived:latency_jitter", sanitize_value(frame.latency_jitter));
  append_metric("risk:realtime_risk", sanitize_value(frame.realtime_risk));
  append_metric("risk:saturation_risk", sanitize_value(frame.saturation_risk));
  append_metric("risk:state", static_cast<double>(static_cast<std::uint8_t>(frame.state)));

  if (options_.publish_health) {
    append_metric("agent:heartbeat", static_cast<double>(frame.agent.heartbeat_ms));
    append_metric("agent:loop_jitter", sanitize_value(frame.agent.loop_jitter_ms));
    append_metric("agent:compute_time", sanitize_value(frame.agent.compute_time_ms));
    append_metric("agent:redis_latency", sanitize_value(frame.agent.redis_latency_ms));
    append_metric("agent:redis_errors", static_cast<double>(frame.agent.redis_errors));
    append_metric("agent:sensor_failures", static_cast<double>(frame.agent.sensor_failures));
    append_metric("agent:missed_cycles", static_cast<double>(frame.agent.missed_cycles));
  }

  for (const auto& arg : command_args_) {
    command_argv_.push_back(arg.c_str());
    command_argv_len_.push_back(arg.size());
  }

  const auto publish_start = std::chrono::steady_clock::now();
  redisReply* reply = static_cast<redisReply*>(
      redisCommandArgv(context_.get(), static_cast<int>(command_argv_.size()), command_argv_.data(),
                       command_argv_len_.data()));
  const auto publish_end = std::chrono::steady_clock::now();
  frame.agent.redis_latency_ms =
      std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(publish_end - publish_start).count();
  if (reply == nullptr) {
    return false;
  }

  const bool ok = reply->type != REDIS_REPLY_ERROR;
  freeReplyObject(reply);
  return ok;
}

void RedisTsSink::reserve_command_buffers() {
  command_args_.reserve(kMaxCommandArgCount);
  command_argv_.reserve(kMaxCommandArgCount);
  command_argv_len_.reserve(kMaxCommandArgCount);
}

}  // namespace hw_agent::sinks
