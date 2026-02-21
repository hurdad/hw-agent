cmake_minimum_required(VERSION 3.16)
project(hw_agent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(hiredis INTERFACE)
add_library(hiredis::hiredis ALIAS hiredis)

target_include_directories(hiredis INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/hiredis)
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
if(HIREDIS_INCLUDE_DIR)
  target_include_directories(hiredis INTERFACE ${HIREDIS_INCLUDE_DIR})
endif()

find_library(HIREDIS_LIBRARY hiredis)
if(HIREDIS_LIBRARY)
  target_link_libraries(hiredis INTERFACE ${HIREDIS_LIBRARY})
endif()

find_package(yaml-cpp QUIET)
if(NOT TARGET yaml-cpp)
  add_library(yaml-cpp INTERFACE)
endif()

add_executable(hw_agent
  src/main.cpp
  src/core/agent.cpp
  src/core/sampler.cpp
  src/sensors/tegrastats.cpp
  src/sensors/psi.cpp
  src/sensors/cpu.cpp
  src/sensors/interrupts.cpp
  src/sensors/softirqs.cpp
  src/sensors/cpufreq.cpp
  src/sensors/thermal.cpp
  src/sensors/power.cpp
  src/sensors/memory.cpp
  src/sensors/disk.cpp
  src/sensors/network.cpp
  src/sensors/gpu.cpp
  src/derived/scheduler_pressure.cpp
  src/derived/memory_pressure.cpp
  src/derived/io_pressure.cpp
  src/derived/thermal_pressure.cpp
  src/derived/power_pressure.cpp
  src/derived/latency_jitter.cpp
  src/risk/realtime_risk.cpp
  src/risk/saturation_risk.cpp
  src/risk/system_state.cpp
  src/sinks/redis_ts.cpp
  src/sinks/stdout_debug.cpp
)

target_include_directories(hw_agent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(hw_agent PRIVATE yaml-cpp hiredis::hiredis)
