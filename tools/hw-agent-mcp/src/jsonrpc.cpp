#include "mcp/jsonrpc.hpp"

#include <stdexcept>

namespace hw::mcp {

namespace {

void validate_id(const nlohmann::json& id) {
  if (id.is_null() || id.is_string() || id.is_number_integer() || id.is_number_unsigned()) {
    return;
  }
  throw std::invalid_argument("JSON-RPC id must be string, integer, or null");
}

}  // namespace

JsonRpcRequest parse_request(const nlohmann::json& request) {
  if (!request.is_object()) {
    throw std::invalid_argument("Request must be a JSON object");
  }

  const auto jsonrpc_it = request.find("jsonrpc");
  if (jsonrpc_it == request.end() || !jsonrpc_it->is_string() || *jsonrpc_it != kJsonRpcVersion) {
    throw std::invalid_argument("jsonrpc must be \"2.0\"");
  }

  const auto method_it = request.find("method");
  if (method_it == request.end() || !method_it->is_string()) {
    throw std::invalid_argument("method must be a string");
  }

  JsonRpcRequest parsed{.method = method_it->get<std::string>(), .params = nlohmann::json::object(), .id = std::nullopt};

  const auto params_it = request.find("params");
  if (params_it != request.end()) {
    if (!params_it->is_object() && !params_it->is_array()) {
      throw std::invalid_argument("params must be an object or an array");
    }
    parsed.params = *params_it;
  }

  const auto id_it = request.find("id");
  if (id_it != request.end()) {
    validate_id(*id_it);
    parsed.id = *id_it;
  }

  return parsed;
}

nlohmann::json make_result_response(const nlohmann::json& id, const nlohmann::json& result) {
  return nlohmann::json{{"jsonrpc", kJsonRpcVersion}, {"id", id}, {"result", result}};
}

nlohmann::json make_error_response(const nlohmann::json& id, const JsonRpcError& error) {
  return nlohmann::json{{"jsonrpc", kJsonRpcVersion},
                        {"id", id},
                        {"error", {{"code", error.code}, {"message", error.message}}}};
}

}  // namespace hw::mcp
