#include <iostream>

#include "mcp/server.hpp"
#include "mcp/tools.hpp"

int main() {
  hw::mcp::Server server(hw::mcp::build_tool_registry());
  return server.run(std::cin, std::cout, std::cerr);
}
