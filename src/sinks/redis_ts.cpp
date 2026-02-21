#include "sinks/redis_ts.hpp"

#include "core/timestamp.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <hiredis/hiredis.h>

namespace hw_agent::sinks {
namespace {

using Metric = std::pair<const char*, double>;

double sanitize_value(const float value) {
  return std::isfinite(value) ? static_cast<double>(value) : 0.0;
}

void add_metric_args(std::vector<std::string>& args, const std::string& key_prefix,
                     const std::uint64_t timestamp_ms, const char* suffix, const double value) {
  args.emplace_back(key_prefix + ":" + suffix);
  args.emplace_back(std::to_string(timestamp_ms));
  args.emplace_back(std::to_string(value));
}

}  // namespace

RedisTsSink::RedisTsSink(RedisTsOptions options) : options_(std::move(options)) {}

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

  redisContext* raw =
      redisConnectWithTimeout(options_.host.c_str(), static_cast<int>(options_.port), timeout);
  if (raw == nullptr || raw->err != REDIS_OK) {
    if (raw != nullptr) {
      redisFree(raw);
    }
    return false;
  }

  context_.reset(raw);
  return authenticate() && select_db();
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

  std::vector<Metric> metrics = {
      {"raw:psi", sanitize_value(frame.psi)},
      {"raw:cpu", sanitize_value(frame.cpu)},
      {"raw:irq", sanitize_value(frame.irq)},
      {"raw:softirqs", sanitize_value(frame.softirqs)},
      {"raw:memory", sanitize_value(frame.memory)},
      {"raw:thermal", sanitize_value(frame.thermal)},
      {"raw:cpufreq", sanitize_value(frame.cpufreq)},
      {"raw:power", sanitize_value(frame.power)},
      {"raw:disk", sanitize_value(frame.disk)},
      {"raw:network", sanitize_value(frame.network)},
      {"raw:gpu_util", sanitize_value(frame.gpu_util)},
      {"raw:gpu_mem_util", sanitize_value(frame.gpu_mem_util)},
      {"raw:gpu_mem_free", sanitize_value(frame.gpu_mem_free)},
      {"raw:gpu_temp", sanitize_value(frame.gpu_temp)},
      {"raw:gpu_clock_ratio", sanitize_value(frame.gpu_clock_ratio)},
      {"raw:gpu_power_ratio", sanitize_value(frame.gpu_power_ratio)},
      {"raw:gpu_throttle", sanitize_value(frame.gpu_throttle)},
      {"derived:scheduler_pressure", sanitize_value(frame.scheduler_pressure)},
      {"derived:memory_pressure", sanitize_value(frame.memory_pressure)},
      {"derived:io_pressure", sanitize_value(frame.io_pressure)},
      {"derived:thermal_pressure", sanitize_value(frame.thermal_pressure)},
      {"derived:power_pressure", sanitize_value(frame.power_pressure)},
      {"derived:latency_jitter", sanitize_value(frame.latency_jitter)},
      {"risk:realtime_risk", sanitize_value(frame.realtime_risk)},
      {"risk:saturation_risk", sanitize_value(frame.saturation_risk)},
      {"risk:state", static_cast<double>(static_cast<std::uint8_t>(frame.state))},
  };

  if (options_.publish_health) {
    metrics.push_back({"agent:heartbeat", static_cast<double>(frame.agent.heartbeat_ms)});
    metrics.push_back({"agent:loop_jitter", sanitize_value(frame.agent.loop_jitter_ms)});
    metrics.push_back({"agent:compute_time", sanitize_value(frame.agent.compute_time_ms)});
    metrics.push_back({"agent:redis_latency", sanitize_value(frame.agent.redis_latency_ms)});
    metrics.push_back({"agent:sensor_failures", static_cast<double>(frame.agent.sensor_failures)});
    metrics.push_back({"agent:missed_cycles", static_cast<double>(frame.agent.missed_cycles)});
  }

  std::vector<std::string> args;
  args.reserve(1 + (metrics.size() * 3));
  args.emplace_back("TS.MADD");
  for (const auto& [suffix, value] : metrics) {
    add_metric_args(args, options_.key_prefix, timestamp_ms, suffix, value);
  }

  std::vector<const char*> argv;
  std::vector<std::size_t> argv_len;
  argv.reserve(args.size());
  argv_len.reserve(args.size());

  for (const auto& arg : args) {
    argv.push_back(arg.c_str());
    argv_len.push_back(arg.size());
  }

  const auto publish_start = std::chrono::steady_clock::now();
  redisReply* reply = static_cast<redisReply*>(
      redisCommandArgv(context_.get(), static_cast<int>(argv.size()), argv.data(), argv_len.data()));
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

}  // namespace hw_agent::sinks
