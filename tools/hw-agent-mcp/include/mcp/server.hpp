#pragma once

#include <iosfwd>

#include "mcp/tools.hpp"

namespace hw::mcp {

class Server {
 public:
  explicit Server(ToolRegistry tools);

  int run(std::istream& in, std::ostream& out, std::ostream& err) const;

 private:
  nlohmann::json handle_request(const nlohmann::json& request, bool& should_respond) const;
  nlohmann::json handle_initialize(const nlohmann::json& params) const;
  nlohmann::json handle_tools_list() const;
  nlohmann::json handle_tools_call(const nlohmann::json& params) const;
  nlohmann::json handle_resources_list() const;
  nlohmann::json handle_resources_read(const nlohmann::json& params) const;

  ToolRegistry tools_;
};

}  // namespace hw::mcp
