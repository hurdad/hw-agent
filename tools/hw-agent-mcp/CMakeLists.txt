find_package(nlohmann_json REQUIRED)

add_executable(hw-agent-mcp
  src/main.cpp
  src/server.cpp
  src/jsonrpc.cpp
  src/tools/metrics_summary.cpp
)

target_include_directories(hw-agent-mcp
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(hw-agent-mcp
  PRIVATE
    nlohmann_json::nlohmann_json
    hiredis::hiredis
)
