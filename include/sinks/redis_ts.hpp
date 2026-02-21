#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "model/signal_frame.hpp"

struct redisContext;

namespace hw_agent::sinks {

struct RedisTsOptions {
  std::string host{"127.0.0.1"};
  std::uint16_t port{6379};
  std::string password{};
  int db{0};
  std::string key_prefix{"hw-agent"};
  std::uint32_t connect_timeout_ms{1000};
};

class RedisTsSink {
 public:
  explicit RedisTsSink(RedisTsOptions options = {});
  ~RedisTsSink();

  RedisTsSink(const RedisTsSink&) = delete;
  RedisTsSink& operator=(const RedisTsSink&) = delete;
  RedisTsSink(RedisTsSink&&) noexcept;
  RedisTsSink& operator=(RedisTsSink&&) noexcept;

  bool publish(const model::signal_frame& frame);

 private:
  struct ContextDeleter {
    void operator()(redisContext* context) const;
  };

  bool ensure_connected();
  bool reconnect();
  bool authenticate();
  bool select_db();
  bool publish_impl(const model::signal_frame& frame);

  RedisTsOptions options_;
  std::unique_ptr<redisContext, ContextDeleter> context_;
};

}  // namespace hw_agent::sinks
