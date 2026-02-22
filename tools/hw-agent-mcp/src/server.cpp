#include "mcp/server.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#include "mcp/jsonrpc.hpp"

namespace hw::mcp {

Server::Server(ToolRegistry tools) : tools_(std::move(tools)) {}

int Server::run(std::istream& in, std::ostream& out, std::ostream& err) const {
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }

    bool should_respond = true;
    try {
      const auto request = nlohmann::json::parse(line);
      const auto response = handle_request(request, should_respond);
      if (should_respond) {
        out << response.dump() << '\n';
        out.flush();
      }
    } catch (const std::exception& ex) {
      err << "hw-agent-mcp: failed to process request: " << ex.what() << '\n';
      if (should_respond) {
        out << make_error_response(nullptr, JsonRpcError{.code = -32603, .message = "internal error"}).dump()
            << '\n';
        out.flush();
      }
    }
  }

  return 0;
}

nlohmann::json Server::handle_request(const nlohmann::json& request, bool& should_respond) const {
  nlohmann::json id = nullptr;
  try {
    const auto parsed = parse_request(request);
    should_respond = parsed.id.has_value();
    if (parsed.id.has_value()) {
      id = *parsed.id;
    }

    if (parsed.method == "initialize") {
      return make_result_response(id, handle_initialize(parsed.params));
    }
    if (parsed.method == "tools/list") {
      return make_result_response(id, handle_tools_list());
    }
    if (parsed.method == "tools/call") {
      return make_result_response(id, handle_tools_call(parsed.params));
    }
    if (parsed.method == "resources/list") {
      return make_result_response(id, handle_resources_list());
    }
    if (parsed.method == "resources/read") {
      return make_result_response(id, handle_resources_read(parsed.params));
    }

    return make_error_response(id, JsonRpcError{.code = -32601, .message = "method not found"});
  } catch (const std::invalid_argument& ex) {
    if (!should_respond) {
      return {};
    }
    return make_error_response(id, JsonRpcError{.code = -32602, .message = ex.what()});
  }
}

nlohmann::json Server::handle_initialize(const nlohmann::json& params) const {
  if (!params.is_object()) {
    throw std::invalid_argument("params must be an object");
  }

  return nlohmann::json{{"serverInfo", {{"name", "hw-agent-mcp"}, {"version", "0.1.0"}}},
                        {"capabilities",
                         {{"tools", nlohmann::json::object()}, {"resources", nlohmann::json::object()}}}};
}

nlohmann::json Server::handle_tools_list() const {
  nlohmann::json tools = nlohmann::json::array();
  for (const auto& [_, tool] : tools_) {
    tools.push_back({{"name", tool.name}, {"description", tool.description}, {"inputSchema", tool.input_schema}});
  }
  return nlohmann::json{{"tools", tools}};
}

nlohmann::json Server::handle_tools_call(const nlohmann::json& params) const {
  if (!params.is_object()) {
    throw std::invalid_argument("params must be an object");
  }

  const auto name_it = params.find("name");
  if (name_it == params.end() || !name_it->is_string()) {
    throw std::invalid_argument("name must be a string");
  }

  const auto args_it = params.find("arguments");
  if (args_it == params.end() || !args_it->is_object()) {
    throw std::invalid_argument("arguments must be an object");
  }

  const auto tool_it = tools_.find(name_it->get<std::string>());
  if (tool_it == tools_.end()) {
    throw std::invalid_argument("unknown tool");
  }

  return nlohmann::json{{"content", tool_it->second.handler(*args_it)}};
}

nlohmann::json Server::handle_resources_list() const {
  return nlohmann::json{
      {"resources", nlohmann::json::array({{{"uri", "redis://metrics/catalog"},
                                             {"name", "Metrics Catalog"},
                                             {"description", "Stub metrics catalog resource"}},
                                            {{"uri", "redis://schema/labels"},
                                             {"name", "Label Schema"},
                                             {"description", "Stub labels schema resource"}}})}};
}

nlohmann::json Server::handle_resources_read(const nlohmann::json& params) const {
  if (!params.is_object()) {
    throw std::invalid_argument("params must be an object");
  }

  const auto uri_it = params.find("uri");
  if (uri_it == params.end() || !uri_it->is_string()) {
    throw std::invalid_argument("uri must be a string");
  }

  const auto& uri = uri_it->get_ref<const std::string&>();
  if (uri == "redis://metrics/catalog") {
    return nlohmann::json{{"uri", uri},
                          {"contents", nlohmann::json{{"metrics", {"cpu.usage", "memory.used", "thermal.celsius"}}}}};
  }
  if (uri == "redis://schema/labels") {
    return nlohmann::json{{"uri", uri},
                          {"contents", nlohmann::json{{"labels", {"host", "cpu", "gpu", "sensor"}}}}};
  }

  throw std::invalid_argument("unknown resource uri");
}

}  // namespace hw::mcp
