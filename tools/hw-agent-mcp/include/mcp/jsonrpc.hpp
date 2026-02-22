#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace hw::mcp {

constexpr const char* kJsonRpcVersion = "2.0";

struct JsonRpcError {
  int code;
  std::string message;
};

struct JsonRpcRequest {
  std::string method;
  nlohmann::json params;
  std::optional<nlohmann::json> id;
};

JsonRpcRequest parse_request(const nlohmann::json& request);

nlohmann::json make_result_response(const nlohmann::json& id, const nlohmann::json& result);
nlohmann::json make_error_response(const nlohmann::json& id, const JsonRpcError& error);

}  // namespace hw::mcp
