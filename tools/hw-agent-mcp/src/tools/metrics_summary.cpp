#include "mcp/tools.hpp"

#include <stdexcept>
#include <vector>

namespace hw::mcp {

namespace {

nlohmann::json handle_metrics_summary(const nlohmann::json& params) {
  if (!params.is_object()) {
    throw std::invalid_argument("params must be an object");
  }

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

  const auto window_it = params.find("window");
  if (window_it == params.end() || !window_it->is_string()) {
    throw std::invalid_argument("window must be a string");
  }

  return nlohmann::json{{"tool", "metrics.summary"},
                        {"window", *window_it},
                        {"filters", filters},
                        {"summary",
                         {{"sample_count", 42},
                          {"avg_cpu_pct", 57.4},
                          {"avg_mem_pct", 68.1},
                          {"status", "stubbed"}}}};
}

}  // namespace

ToolRegistry build_tool_registry() {
  ToolRegistry registry;

  Tool metrics_summary{.name = "metrics.summary",
                       .description = "Return a summary of metrics over a time window.",
                       .input_schema =
                           nlohmann::json{{"type", "object"},
                                          {"properties",
                                           {{"filters", {{"type", "array"}, {"items", {{"type", "string"}}}}},
                                            {"window", {{"type", "string"}}}}},
                                          {"required", {"filters", "window"}},
                                          {"additionalProperties", false}},
                       .handler = handle_metrics_summary};

  registry.emplace(metrics_summary.name, std::move(metrics_summary));
  return registry;
}

}  // namespace hw::mcp
