#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "model/signal_frame.hpp"

struct redisContext;

namespace hw_agent::sinks {

struct RedisTsOptions {
  std::string host{"127.0.0.1"};
  std::uint16_t port{6379};
  std::string password{};
  int db{0};
  std::string key_prefix{"edge:node"};
  std::uint32_t connect_timeout_ms{1000};
  bool publish_health{true};
};

class RedisTsSink {
 public:
  explicit RedisTsSink(RedisTsOptions options = {});
  ~RedisTsSink();

  RedisTsSink(const RedisTsSink&) = delete;
  RedisTsSink& operator=(const RedisTsSink&) = delete;
  RedisTsSink(RedisTsSink&&) noexcept;
  RedisTsSink& operator=(RedisTsSink&&) noexcept;

  bool check_connectivity();
  bool publish(model::signal_frame& frame);

 private:
  struct ContextDeleter {
    void operator()(redisContext* context) const;
  };

  bool ensure_connected();
  bool reconnect();
  bool authenticate();
  bool select_db();
  bool ensure_schema();
  bool publish_impl(model::signal_frame& frame);
  void reserve_command_buffers();

  RedisTsOptions options_;
  std::unique_ptr<redisContext, ContextDeleter> context_;
  std::vector<std::string> command_args_;
  std::vector<const char*> command_argv_;
  std::vector<std::size_t> command_argv_len_;
  bool timeseries_available_{true};
  bool schema_ready_{false};
};

}  // namespace hw_agent::sinks
