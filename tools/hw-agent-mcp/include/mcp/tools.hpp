#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace hw::mcp {

struct Tool {
  std::string name;
  std::string description;
  nlohmann::json input_schema;
  std::function<nlohmann::json(const nlohmann::json&)> handler;
};

using ToolRegistry = std::unordered_map<std::string, Tool>;

ToolRegistry build_tool_registry();

}  // namespace hw::mcp
