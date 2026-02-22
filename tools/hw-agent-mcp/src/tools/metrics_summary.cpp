#include "mcp/tools.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <hiredis/hiredis.h>

namespace hw::mcp {

namespace {

struct RedisConfig {
  std::string host{"127.0.0.1"};
  std::uint16_t port{6379};
  std::string unix_socket{};
  std::string password{};
  int db{0};
  std::string key_prefix{"edge:node"};
  std::uint32_t connect_timeout_ms{1000};
};

struct RedisReplyDeleter {
  void operator()(redisReply* reply) const {
    if (reply != nullptr) {
      freeReplyObject(reply);
    }
  }
};

using RedisReplyPtr = std::unique_ptr<redisReply, RedisReplyDeleter>;

class RedisConnection {
 public:
  explicit RedisConnection(RedisConfig config) : config_(std::move(config)) {}

  void connect() {
    timeval timeout{};
    timeout.tv_sec = static_cast<time_t>(config_.connect_timeout_ms / 1000U);
    timeout.tv_usec = static_cast<suseconds_t>((config_.connect_timeout_ms % 1000U) * 1000U);

    redisContext* raw = nullptr;
    if (!config_.unix_socket.empty()) {
      raw = redisConnectUnixWithTimeout(config_.unix_socket.c_str(), timeout);
    } else {
      raw = redisConnectWithTimeout(config_.host.c_str(), static_cast<int>(config_.port), timeout);
    }

    if (raw == nullptr) {
      throw std::runtime_error("redis connection failed: out of memory");
    }
    context_.reset(raw);

    if (context_->err != REDIS_OK) {
      throw std::runtime_error(std::string("redis connection failed: ") + context_->errstr);
    }

    if (!config_.password.empty()) {
      auto auth_reply = command("AUTH %s", config_.password.c_str());
      if (auth_reply->type == REDIS_REPLY_ERROR) {
        throw std::runtime_error("redis AUTH failed");
      }
    }

    if (config_.db != 0) {
      auto db_reply = command("SELECT %d", config_.db);
      if (db_reply->type == REDIS_REPLY_ERROR) {
        throw std::runtime_error("redis SELECT failed");
      }
    }
  }

  RedisReplyPtr command(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    auto* raw = static_cast<redisReply*>(redisvCommand(context_.get(), format, ap));
    va_end(ap);

    if (raw == nullptr) {
      throw std::runtime_error("redis command failed");
    }
    return RedisReplyPtr(raw);
  }

  const RedisConfig& config() const { return config_; }

 private:
  struct ContextDeleter {
    void operator()(redisContext* context) const {
      if (context != nullptr) {
        redisFree(context);
      }
    }
  };

  RedisConfig config_;
  std::unique_ptr<redisContext, ContextDeleter> context_;
};

std::string getenv_or(const char* name, const std::string& fallback) {
  if (const auto* value = std::getenv(name); value != nullptr) {
    return std::string(value);
  }
  return fallback;
}

int getenv_or_int(const char* name, const int fallback) {
  if (const auto* value = std::getenv(name); value != nullptr) {
    return std::stoi(value);
  }
  return fallback;
}

RedisConfig load_redis_config() {
  RedisConfig config;
  config.host = getenv_or("HW_AGENT_REDIS_HOST", config.host);
  config.port = static_cast<std::uint16_t>(getenv_or_int("HW_AGENT_REDIS_PORT", static_cast<int>(config.port)));
  config.unix_socket = getenv_or("HW_AGENT_REDIS_UNIX_SOCKET", config.unix_socket);
  config.password = getenv_or("HW_AGENT_REDIS_PASSWORD", config.password);
  config.db = getenv_or_int("HW_AGENT_REDIS_DB", config.db);
  config.key_prefix = getenv_or("HW_AGENT_REDIS_PREFIX", config.key_prefix);
  config.connect_timeout_ms =
      static_cast<std::uint32_t>(getenv_or_int("HW_AGENT_REDIS_CONNECT_TIMEOUT_MS", config.connect_timeout_ms));
  return config;
}

std::int64_t now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::int64_t parse_window_ms(const std::string& window) {
  if (window.empty()) {
    throw std::invalid_argument("window must not be empty");
  }

  std::size_t suffix_pos = window.size();
  while (suffix_pos > 0 && std::isalpha(static_cast<unsigned char>(window[suffix_pos - 1])) != 0) {
    --suffix_pos;
  }

  if (suffix_pos == 0) {
    throw std::invalid_argument("window must start with a numeric value");
  }

  const auto value = std::stoll(window.substr(0, suffix_pos));
  const auto suffix = window.substr(suffix_pos);

  if (suffix == "ms" || suffix.empty()) {
    return value;
  }
  if (suffix == "s") {
    return value * 1000;
  }
  if (suffix == "m") {
    return value * 60 * 1000;
  }
  if (suffix == "h") {
    return value * 60 * 60 * 1000;
  }

  throw std::invalid_argument("unsupported window suffix; use ms, s, m, or h");
}

std::vector<std::string> parse_filters(const nlohmann::json& params) {
  const auto filters_it = params.find("filters");
  if (filters_it == params.end() || !filters_it->is_array()) {
    throw std::invalid_argument("filters must be an array of strings");
  }

  std::vector<std::string> filters;
  filters.reserve(filters_it->size());
  for (const auto& filter : *filters_it) {
    if (!filter.is_string()) {
      throw std::invalid_argument("filters must be an array of strings");
    }
    filters.push_back(filter.get<std::string>());
  }

  if (filters.empty()) {
    throw std::invalid_argument("filters must contain at least one value");
  }

  return filters;
}

std::vector<std::string> list_metric_keys(RedisConnection& redis) {
  auto reply = redis.command("KEYS %s:*", redis.config().key_prefix.c_str());
  if (reply->type == REDIS_REPLY_ERROR) {
    throw std::runtime_error("failed to list metric keys");
  }
  if (reply->type != REDIS_REPLY_ARRAY) {
    throw std::runtime_error("unexpected response from KEYS");
  }

  std::vector<std::string> keys;
  keys.reserve(reply->elements);
  for (std::size_t i = 0; i < reply->elements; ++i) {
    const auto* element = reply->element[i];
    if (element != nullptr && element->type == REDIS_REPLY_STRING && element->str != nullptr) {
      keys.emplace_back(element->str, static_cast<std::size_t>(element->len));
    }
  }
  return keys;
}

std::vector<std::string> select_keys(const std::vector<std::string>& all_keys, const std::vector<std::string>& filters) {
  std::vector<std::string> selected;

  for (const auto& key : all_keys) {
    for (const auto& filter : filters) {
      if (key.find(filter) != std::string::npos) {
        selected.push_back(key);
        break;
      }
    }
  }

  return selected;
}

nlohmann::json query_timeseries(RedisConnection& redis, const std::vector<std::string>& keys, const std::int64_t from_ms,
                                const std::int64_t to_ms) {
  nlohmann::json series = nlohmann::json::array();

  for (const auto& key : keys) {
    auto reply = redis.command("TS.RANGE %s %lld %lld", key.c_str(), static_cast<long long>(from_ms),
                               static_cast<long long>(to_ms));

    if (reply->type == REDIS_REPLY_ERROR) {
      throw std::runtime_error(std::string("TS.RANGE failed for key ") + key);
    }
    if (reply->type != REDIS_REPLY_ARRAY) {
      throw std::runtime_error(std::string("unexpected TS.RANGE response for key ") + key);
    }

    double sum = 0.0;
    std::size_t sample_count = 0;
    nlohmann::json points = nlohmann::json::array();

    for (std::size_t i = 0; i < reply->elements; ++i) {
      const auto* point = reply->element[i];
      if (point == nullptr || point->type != REDIS_REPLY_ARRAY || point->elements != 2) {
        continue;
      }

      const auto* ts = point->element[0];
      const auto* value = point->element[1];
      if (ts == nullptr || value == nullptr || ts->str == nullptr || value->str == nullptr) {
        continue;
      }

      const auto timestamp = std::stoll(ts->str);
      const auto numeric_value = std::stod(value->str);

      points.push_back({{"timestamp", timestamp}, {"value", numeric_value}});
      sum += numeric_value;
      ++sample_count;
    }

    series.push_back({{"key", key},
                      {"sample_count", sample_count},
                      {"avg", sample_count > 0 ? (sum / static_cast<double>(sample_count)) : 0.0},
                      {"points", points}});
  }

  return series;
}

nlohmann::json handle_metrics_summary(const nlohmann::json& params) {
  if (!params.is_object()) {
    throw std::invalid_argument("params must be an object");
  }

  const auto filters = parse_filters(params);

  const auto window_it = params.find("window");
  if (window_it == params.end() || !window_it->is_string()) {
    throw std::invalid_argument("window must be a string");
  }

  const auto window_ms = parse_window_ms(window_it->get<std::string>());
  const auto to_ms = now_ms();
  const auto from_ms = to_ms - window_ms;

  RedisConnection redis(load_redis_config());
  redis.connect();

  const auto keys = select_keys(list_metric_keys(redis), filters);
  const auto series = query_timeseries(redis, keys, from_ms, to_ms);

  std::size_t total_samples = 0;
  for (const auto& item : series) {
    total_samples += item.at("sample_count").get<std::size_t>();
  }

  return nlohmann::json{{"tool", "metrics.summary"},
                        {"window", *window_it},
                        {"from", from_ms},
                        {"to", to_ms},
                        {"filters", filters},
                        {"matched_keys", keys},
                        {"summary", {{"series_count", series.size()}, {"sample_count", total_samples}}},
                        {"series", series}};
}

nlohmann::json handle_metrics_timeseries_query(const nlohmann::json& params) {
  if (!params.is_object()) {
    throw std::invalid_argument("params must be an object");
  }

  const auto keys_it = params.find("keys");
  if (keys_it == params.end() || !keys_it->is_array() || keys_it->empty()) {
    throw std::invalid_argument("keys must be a non-empty array of strings");
  }

  std::vector<std::string> keys;
  keys.reserve(keys_it->size());
  for (const auto& key : *keys_it) {
    if (!key.is_string()) {
      throw std::invalid_argument("keys must be a non-empty array of strings");
    }
    keys.push_back(key.get<std::string>());
  }

  const auto window_it = params.find("window");
  if (window_it == params.end() || !window_it->is_string()) {
    throw std::invalid_argument("window must be a string");
  }

  const auto window_ms = parse_window_ms(window_it->get<std::string>());
  const auto to_ms = now_ms();
  const auto from_ms = to_ms - window_ms;

  RedisConnection redis(load_redis_config());
  redis.connect();

  return nlohmann::json{{"tool", "metrics.timeseries.query"},
                        {"window", *window_it},
                        {"from", from_ms},
                        {"to", to_ms},
                        {"series", query_timeseries(redis, keys, from_ms, to_ms)}};
}

}  // namespace

ToolRegistry build_tool_registry() {
  ToolRegistry registry;

  Tool metrics_summary{.name = "metrics.summary",
                       .description = "Summarize RedisTimeSeries metrics over a window using filter matching.",
                       .input_schema =
                           nlohmann::json{{"type", "object"},
                                          {"properties",
                                           {{"filters", {{"type", "array"}, {"items", {{"type", "string"}}}}},
                                            {"window", {{"type", "string"}}}}},
                                          {"required", {"filters", "window"}},
                                          {"additionalProperties", false}},
                       .handler = handle_metrics_summary};

  Tool metrics_timeseries_query{.name = "metrics.timeseries.query",
                                .description = "Query one or more RedisTimeSeries keys for recent points.",
                                .input_schema =
                                    nlohmann::json{{"type", "object"},
                                                   {"properties",
                                                    {{"keys", {{"type", "array"}, {"items", {{"type", "string"}}}}},
                                                     {"window", {{"type", "string"}}}}},
                                                   {"required", {"keys", "window"}},
                                                   {"additionalProperties", false}},
                                .handler = handle_metrics_timeseries_query};

  registry.emplace(metrics_summary.name, std::move(metrics_summary));
  registry.emplace(metrics_timeseries_query.name, std::move(metrics_timeseries_query));
  return registry;
}

}  // namespace hw::mcp
