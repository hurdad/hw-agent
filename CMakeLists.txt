cmake_minimum_required(VERSION 3.16)
project(hw_agent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_NVML "Enable NVML GPU monitoring support" ON)

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

if(USE_NVML)
  find_path(NVML_INCLUDE_DIR nvml.h)
  find_library(NVML_LIBRARY nvidia-ml)
  if(NVML_INCLUDE_DIR AND NVML_LIBRARY)
    set(HW_AGENT_HAVE_NVML TRUE)
    message(STATUS "NVML found: compile-time headers and library enabled")
  else()
    message(STATUS "NVML not found, building without compile-time NVML (runtime dlopen fallback remains)")
  endif()
endif()

add_executable(hw_agent
  src/main.cpp
  src/core/agent.cpp
  src/core/config.cpp
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
  src/sensors/gpu/gpu_nvml.cpp
  src/sensors/gpu/gpu_none.cpp
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
target_link_libraries(hw_agent PRIVATE yaml-cpp hiredis::hiredis ${CMAKE_DL_LIBS})

if(HW_AGENT_HAVE_NVML)
  target_compile_definitions(hw_agent PRIVATE HW_AGENT_HAVE_NVML)
  target_include_directories(hw_agent PRIVATE ${NVML_INCLUDE_DIR})
  target_link_libraries(hw_agent PRIVATE ${NVML_LIBRARY})
endif()


enable_testing()

add_executable(hw_agent_risk_unit_tests
  tests/risk_unit_tests.cpp
  src/core/config.cpp
  src/risk/realtime_risk.cpp
  src/risk/saturation_risk.cpp
  src/risk/system_state.cpp
)

target_include_directories(hw_agent_risk_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_test(NAME hw_agent_risk_unit_tests COMMAND hw_agent_risk_unit_tests)

add_executable(hw_agent_agent_unit_tests
  tests/agent_unit_tests.cpp
  src/core/config.cpp
  src/core/sampler.cpp
  src/derived/memory_pressure.cpp
  src/derived/scheduler_pressure.cpp
  src/derived/io_pressure.cpp
  src/derived/power_pressure.cpp
  src/derived/latency_jitter.cpp
  src/derived/thermal_pressure.cpp
  src/risk/realtime_risk.cpp
  src/risk/saturation_risk.cpp
  src/risk/system_state.cpp
  src/sensors/cpu.cpp
  src/sensors/memory.cpp
  src/sensors/interrupts.cpp
  src/sensors/power.cpp
  src/sensors/psi.cpp
  src/sensors/softirqs.cpp
  src/sensors/thermal.cpp
  src/sinks/redis_ts.cpp
)

target_include_directories(hw_agent_agent_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(hw_agent_agent_unit_tests PRIVATE hiredis::hiredis)

add_test(NAME hw_agent_agent_unit_tests COMMAND hw_agent_agent_unit_tests)


add_executable(hw_agent_sensors_unit_tests
  tests/sensors_unit_tests.cpp
  src/sensors/cpu.cpp
  src/sensors/disk.cpp
  src/sensors/thermal.cpp
  src/sensors/power.cpp
  src/sensors/cpufreq.cpp
  src/sensors/psi.cpp
  src/sensors/softirqs.cpp
)

target_include_directories(hw_agent_sensors_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_test(NAME hw_agent_sensors_unit_tests COMMAND hw_agent_sensors_unit_tests)
