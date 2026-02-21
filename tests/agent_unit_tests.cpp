#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

#include <hiredis/hiredis.h>

#include "core/config.hpp"
#include "core/sampler.hpp"
#include "derived/io_pressure.hpp"
#include "derived/latency_jitter.hpp"
#include "derived/memory_pressure.hpp"
#include "derived/power_pressure.hpp"
#include "derived/scheduler_pressure.hpp"
#include "derived/thermal_pressure.hpp"
#include "model/signal_frame.hpp"
#include "risk/realtime_risk.hpp"
#include "risk/saturation_risk.hpp"
#include "risk/system_state.hpp"
#include "sensors/cpu.hpp"
#include "sensors/interrupts.hpp"
#include "sensors/memory.hpp"
#include "sensors/power.hpp"
#include "sensors/psi.hpp"
#include "sensors/softirqs.hpp"
#include "sensors/thermal.hpp"
#include "sinks/redis_ts.hpp"

using hw_agent::core::Sampler;
using hw_agent::core::load_agent_config;
using hw_agent::derived::IoPressure;
using hw_agent::derived::LatencyJitter;
using hw_agent::derived::MemoryPressure;
using hw_agent::derived::PowerPressure;
using hw_agent::derived::SchedulerPressure;
using hw_agent::derived::ThermalPressure;
using hw_agent::model::signal_frame;
using hw_agent::model::system_state;
using hw_agent::risk::RealtimeRisk;
using hw_agent::risk::SaturationRisk;
using hw_agent::risk::SystemState;
using hw_agent::sensors::CpuSensor;
using hw_agent::sensors::InterruptsSensor;
using hw_agent::sensors::MemorySensor;
using hw_agent::sensors::CpuThrottleSensor;
using hw_agent::sensors::PsiSensor;
using hw_agent::sensors::SoftirqsSensor;
using hw_agent::sensors::ThermalSensor;
using hw_agent::sinks::RedisTsOptions;
using hw_agent::sinks::RedisTsSink;

namespace {

struct RedisMockState {
  std::vector<std::string> last_argv{};
  int command_argv_calls{0};
};

RedisMockState g_redis_mock{};

extern "C" {

redisContext* redisConnectWithTimeout(const char*, int, const struct timeval) {
  auto* context = static_cast<redisContext*>(std::calloc(1, sizeof(redisContext)));
  context->err = REDIS_OK;
  return context;
}

redisContext* redisConnectUnixWithTimeout(const char*, const struct timeval) {
  auto* context = static_cast<redisContext*>(std::calloc(1, sizeof(redisContext)));
  context->err = REDIS_OK;
  return context;
}

void redisFree(redisContext* c) { std::free(c); }

void* redisCommand(redisContext*, const char*, ...) {
  auto* reply = static_cast<redisReply*>(std::calloc(1, sizeof(redisReply)));
  reply->type = REDIS_REPLY_STATUS;
  return reply;
}

void* redisCommandArgv(redisContext*, int argc, const char** argv, const size_t*) {
  g_redis_mock.command_argv_calls += 1;
  g_redis_mock.last_argv.clear();
  for (int i = 0; i < argc; ++i) {
    g_redis_mock.last_argv.emplace_back(argv[i]);
  }
  auto* reply = static_cast<redisReply*>(std::calloc(1, sizeof(redisReply)));
  reply->type = REDIS_REPLY_ARRAY;
  return reply;
}

void freeReplyObject(void* reply) { std::free(reply); }

}  // extern "C"

bool almost_equal(float a, float b, float eps = 1e-4F) {
  return std::fabs(a - b) <= eps;
}

int fail(const char* name, const char* msg) {
  std::cerr << "[FAIL] " << name << ": " << msg << '\n';
  return 1;
}

bool write_temp_file(std::FILE* file, const std::string& content) {
  if (file == nullptr) {
    return false;
  }
  const int fd = fileno(file);
  if (fd < 0) {
    return false;
  }
  if (ftruncate(fd, 0) != 0) {
    return false;
  }
  if (std::fseek(file, 0L, SEEK_SET) != 0) {
    return false;
  }
  if (!content.empty() && std::fwrite(content.data(), 1, content.size(), file) != content.size()) {
    return false;
  }
  std::fflush(file);
  return std::fseek(file, 0L, SEEK_SET) == 0;
}

int test_memory_sensor_parser() {
  std::FILE* meminfo = std::tmpfile();
  std::FILE* vmstat = std::tmpfile();

  if (!write_temp_file(meminfo, "MemTotal:       1000 kB\nMemAvailable:    500 kB\nDirty:            10 kB\nWriteback:         5 kB\n") ||
      !write_temp_file(vmstat, "pgscan_kswapd 4\npgscan_direct 6\npgsteal_kswapd 8\npgsteal_direct 2\n")) {
    return fail("test_memory_sensor_parser", "failed writing temp proc files");
  }

  MemorySensor sensor(meminfo, vmstat, true);
  signal_frame frame{};
  sensor.sample(frame);

  if (!almost_equal(frame.memory, 15.0F)) {
    return fail("test_memory_sensor_parser", "memory pressure should equal Dirty+Writeback");
  }

  if (sensor.raw().pgscan_total != 10 || sensor.raw().pgsteal_total != 10) {
    return fail("test_memory_sensor_parser", "vmstat totals parsed incorrectly");
  }

  return 0;
}

int test_memory_pressure_computation_and_ema() {
  MemoryPressure pressure;
  signal_frame frame{};

  frame.memory = 262144.0F;
  frame.psi_memory = 10.0F;
  pressure.sample(frame);

  const float first = (0.70F * (1.0F - std::exp(-1.0F))) + (0.30F * 0.5F);
  if (!almost_equal(frame.memory_pressure, first)) {
    return fail("test_memory_pressure_computation_and_ema", "first pressure sample mismatch");
  }

  frame.memory = 0.0F;
  frame.psi_memory = 0.0F;
  pressure.sample(frame);
  const float second = 0.75F * first;
  if (!almost_equal(frame.memory_pressure, second)) {
    return fail("test_memory_pressure_computation_and_ema", "EMA behavior mismatch");
  }

  return 0;
}


int test_thermal_pressure_warning_window_configurable() {
  ThermalPressure default_pressure;
  ThermalPressure narrow_window(10.0F);
  signal_frame default_frame{};
  signal_frame narrow_frame{};

  default_frame.thermal = 20.0F;
  narrow_frame.thermal = 20.0F;

  default_pressure.sample(default_frame);
  narrow_window.sample(narrow_frame);

  if (!almost_equal(default_frame.thermal_pressure, 0.23333333F, 1e-3F)) {
    return fail("test_thermal_pressure_warning_window_configurable", "default warning window baseline mismatch");
  }

  if (!almost_equal(narrow_frame.thermal_pressure, 0.0F)) {
    return fail("test_thermal_pressure_warning_window_configurable", "narrow warning window should clamp pressure at zero");
  }

  return 0;
}

int test_sampler_should_sample_every() {
  Sampler sampler;

  if (!sampler.should_sample_every(1)) {
    return fail("test_sampler_should_sample_every", "tick 0 should sample every 1");
  }
  if (!sampler.should_sample_every(5)) {
    return fail("test_sampler_should_sample_every", "tick 0 should sample every 5");
  }
  if (sampler.should_sample_every(0)) {
    return fail("test_sampler_should_sample_every", "every 0 must never sample");
  }

  sampler.advance();
  if (sampler.should_sample_every(5)) {
    return fail("test_sampler_should_sample_every", "tick 1 should not sample every 5");
  }

  for (int i = 0; i < 4; ++i) {
    sampler.advance();
  }
  if (!sampler.should_sample_every(5)) {
    return fail("test_sampler_should_sample_every", "tick 5 should sample every 5");
  }

  return 0;
}

int test_sensor_dispatches_once_per_tick() {
  struct SensorDispatchProbe {
    std::uint64_t every_ticks;
    std::size_t dispatch_count{0};
  };

  std::vector<SensorDispatchProbe> sensors{{1}, {2}, {3}, {5}};
  constexpr std::size_t total_ticks = 30;

  Sampler sampler;
  for (std::size_t tick = 0; tick < total_ticks; ++tick) {
    for (auto& sensor : sensors) {
      std::size_t calls_this_tick = 0;
      if (sampler.should_sample_every(sensor.every_ticks)) {
        ++sensor.dispatch_count;
        ++calls_this_tick;
      }

      if (calls_this_tick > 1) {
        return fail("test_sensor_dispatches_once_per_tick", "sensor dispatched more than once in the same tick");
      }
    }

    sampler.advance();
  }

  for (const auto& sensor : sensors) {
    std::size_t expected_dispatches = 0;
    for (std::size_t tick = 0; tick < total_ticks; ++tick) {
      if ((tick % sensor.every_ticks) == 0) {
        ++expected_dispatches;
      }
    }

    if (sensor.dispatch_count != expected_dispatches) {
      return fail("test_sensor_dispatches_once_per_tick", "sensor dispatch count did not match expected schedule");
    }
  }

  return 0;
}

int test_config_parsing_edge_cases() {
  const auto bad_port = std::filesystem::temp_directory_path() / "hw_agent_bad_port.yaml";
  {
    std::ofstream out(bad_port);
    out << "redis:\n  address: localhost:99999\n";
  }

  bool bad_port_threw = false;
  try {
    (void)load_agent_config(bad_port.string());
  } catch (const std::exception&) {
    bad_port_threw = true;
  }
  std::filesystem::remove(bad_port);

  if (!bad_port_threw) {
    return fail("test_config_parsing_edge_cases", "bad redis port should throw");
  }

  const auto invalid_float = std::filesystem::temp_directory_path() / "hw_agent_bad_float.yaml";
  {
    std::ofstream out(invalid_float);
    out << "thermal_throttle_temp_c: not_a_float\n";
  }

  bool invalid_float_threw = false;
  try {
    (void)load_agent_config(invalid_float.string());
  } catch (const std::exception&) {
    invalid_float_threw = true;
  }
  std::filesystem::remove(invalid_float);

  if (!invalid_float_threw) {
    return fail("test_config_parsing_edge_cases", "invalid float should throw");
  }

  const auto bad_warning_window = std::filesystem::temp_directory_path() / "hw_agent_bad_warning_window.yaml";
  {
    std::ofstream out(bad_warning_window);
    out << "thermal_pressure_warning_window_c: 0\n";
  }

  bool bad_warning_window_threw = false;
  try {
    (void)load_agent_config(bad_warning_window.string());
  } catch (const std::exception&) {
    bad_warning_window_threw = true;
  }
  std::filesystem::remove(bad_warning_window);

  if (!bad_warning_window_threw) {
    return fail("test_config_parsing_edge_cases", "thermal_pressure_warning_window_c <= 0 should throw");
  }

  const auto unix_socket = std::filesystem::temp_directory_path() / "hw_agent_unix_redis.yaml";
  {
    std::ofstream out(unix_socket);
    out << "redis:\n  address: unix:///var/run/redis/redis.sock\n";
  }

  const auto unix_config = load_agent_config(unix_socket.string());
  std::filesystem::remove(unix_socket);

  if (!unix_config.redis.enabled || unix_config.redis.unix_socket != "/var/run/redis/redis.sock") {
    return fail("test_config_parsing_edge_cases", "unix socket redis address should parse");
  }

  const auto missing_redis = std::filesystem::temp_directory_path() / "hw_agent_missing_redis.yaml";
  {
    std::ofstream out(missing_redis);
    out << "tick_rate_hz: 10\nagent:\n  publish_health: false\n";
  }

  const auto config = load_agent_config(missing_redis.string());
  std::filesystem::remove(missing_redis);

  if (config.redis.enabled) {
    return fail("test_config_parsing_edge_cases", "redis should remain disabled when section is missing");
  }

  const auto gpu_index = std::filesystem::temp_directory_path() / "hw_agent_gpu_index.yaml";
  {
    std::ofstream out(gpu_index);
    out << "gpu:\n  device_index: 2\n";
  }

  const auto gpu_config = load_agent_config(gpu_index.string());
  std::filesystem::remove(gpu_index);

  if (gpu_config.gpu_device_index != 2U) {
    return fail("test_config_parsing_edge_cases", "gpu.device_index should parse");
  }

  const auto bad_gpu_index = std::filesystem::temp_directory_path() / "hw_agent_bad_gpu_index.yaml";
  {
    std::ofstream out(bad_gpu_index);
    out << "gpu:\n  device_index: -1\n";
  }

  bool bad_gpu_index_threw = false;
  try {
    (void)load_agent_config(bad_gpu_index.string());
  } catch (const std::exception&) {
    bad_gpu_index_threw = true;
  }
  std::filesystem::remove(bad_gpu_index);

  if (!bad_gpu_index_threw) {
    return fail("test_config_parsing_edge_cases", "negative gpu.device_index should throw");
  }

  return 0;
}

int test_interrupts_and_softirqs_delta_and_underflow_protection() {
  std::FILE* interrupts_file = std::tmpfile();
  std::FILE* softirqs_file = std::tmpfile();

  InterruptsSensor interrupts(interrupts_file, true);
  SoftirqsSensor softirqs(softirqs_file, true);
  signal_frame frame{};

  if (!write_temp_file(interrupts_file, "cpu 1 2 3 4\nintr 100\n") ||
      !write_temp_file(softirqs_file, "NET_RX: 100 100\nTIMER: 50 50\n")) {
    return fail("test_interrupts_and_softirqs_delta_and_underflow_protection", "failed to seed initial proc data");
  }

  frame.monotonic_ns = 1'000'000'000ULL;
  interrupts.sample(frame);
  softirqs.sample(frame);

  if (!almost_equal(frame.irq, 0.0F) || !almost_equal(frame.softirqs, 0.0F)) {
    return fail("test_interrupts_and_softirqs_delta_and_underflow_protection", "first sample must initialize baseline");
  }

  if (!write_temp_file(interrupts_file, "intr 150\n") ||
      !write_temp_file(softirqs_file, "NET_RX: 130 110\nTIMER: 60 55\n")) {
    return fail("test_interrupts_and_softirqs_delta_and_underflow_protection", "failed to write second sample");
  }

  frame.monotonic_ns = 2'000'000'000ULL;
  interrupts.sample(frame);
  softirqs.sample(frame);

  if (!almost_equal(frame.irq, 50.0F)) {
    return fail("test_interrupts_and_softirqs_delta_and_underflow_protection", "interrupt delta/sec mismatch");
  }

  if (!almost_equal(frame.softirqs, 0.0F)) {
    return fail("test_interrupts_and_softirqs_delta_and_underflow_protection", "second softirq sample should set baseline");
  }

  if (!write_temp_file(interrupts_file, "intr 120\n") ||
      !write_temp_file(softirqs_file, "NET_RX: 40 30\nTIMER: 20 10\n")) {
    return fail("test_interrupts_and_softirqs_delta_and_underflow_protection", "failed to write underflow sample");
  }

  frame.monotonic_ns = 3'000'000'000ULL;
  interrupts.sample(frame);
  softirqs.sample(frame);

  if (!almost_equal(frame.irq, 0.0F) || !almost_equal(frame.softirqs, 0.0F)) {
    return fail("test_interrupts_and_softirqs_delta_and_underflow_protection", "counter underflow must clamp to zero");
  }

  return 0;
}


int test_thermal_sensor_headroom_and_all_zones_fail_fallback() {
  const auto thermal_root = std::filesystem::temp_directory_path() / "hw_agent_thermal_test";
  std::filesystem::remove_all(thermal_root);
  std::filesystem::create_directories(thermal_root / "thermal_zone0");
  std::filesystem::create_directories(thermal_root / "thermal_zone1");

  {
    std::ofstream out(thermal_root / "thermal_zone0" / "type");
    out << "cpu-thermal\n";
  }
  {
    std::ofstream out(thermal_root / "thermal_zone1" / "type");
    out << "gpu-thermal\n";
  }
  {
    std::ofstream out(thermal_root / "thermal_zone0" / "temp");
    out << "53000\n";
  }
  {
    std::ofstream out(thermal_root / "thermal_zone1" / "temp");
    out << "61000\n";
  }

  ThermalSensor sensor(85.0F, thermal_root.string());
  signal_frame frame{};
  sensor.sample(frame);

  if (!almost_equal(frame.thermal, 24.0F)) {
    std::filesystem::remove_all(thermal_root);
    return fail("test_thermal_sensor_headroom_and_all_zones_fail_fallback", "headroom should be throttle minus hottest zone temperature");
  }

  if (!almost_equal(sensor.raw().hottest_temp_c, 61.0F) || sensor.raw().hottest_zone != "gpu-thermal") {
    std::filesystem::remove_all(thermal_root);
    return fail("test_thermal_sensor_headroom_and_all_zones_fail_fallback", "hottest zone tracking is incorrect");
  }

  std::filesystem::remove(thermal_root / "thermal_zone0" / "temp");
  std::filesystem::remove(thermal_root / "thermal_zone1" / "temp");

  ThermalSensor failing_sensor(85.0F, thermal_root.string());
  failing_sensor.sample(frame);

  if (!almost_equal(frame.thermal, 0.0F) || !almost_equal(failing_sensor.raw().headroom_c, 0.0F)) {
    std::filesystem::remove_all(thermal_root);
    return fail("test_thermal_sensor_headroom_and_all_zones_fail_fallback", "all-zones-fail fallback should clamp headroom to zero");
  }

  std::filesystem::remove_all(thermal_root);
  return 0;
}

int test_gpu_memory_and_emc_metrics_are_distinct() {
  g_redis_mock = {};

  RedisTsOptions options;
  options.publish_health = false;
  options.key_prefix = "edge:test";

  RedisTsSink sink(options);
  signal_frame frame{};
  frame.gpu_mem_util = 41.0F;
  frame.tegra_emc_util = 73.0F;

  if (!sink.publish(frame)) {
    return fail("test_gpu_memory_and_emc_metrics_are_distinct", "publish should succeed with mock redis");
  }

  bool found_gpu_mem_util = false;
  bool found_emc_util = false;
  for (std::size_t i = 0; i + 2 < g_redis_mock.last_argv.size(); ++i) {
    if (g_redis_mock.last_argv[i] == "edge:test:raw:gpu_mem_util") {
      found_gpu_mem_util = g_redis_mock.last_argv[i + 2].find("41") != std::string::npos;
    }
    if (g_redis_mock.last_argv[i] == "edge:test:raw:tegra_emc_util") {
      found_emc_util = g_redis_mock.last_argv[i + 2].find("73") != std::string::npos;
    }
  }

  if (!found_gpu_mem_util || !found_emc_util) {
    return fail("test_gpu_memory_and_emc_metrics_are_distinct", "expected distinct gpu_mem_util and tegra_emc_util outputs");
  }

  return 0;
}

int test_redis_health_metrics_include_error_counter() {
  g_redis_mock = {};

  RedisTsOptions options;
  options.publish_health = true;
  options.key_prefix = "edge:test";

  RedisTsSink sink(options);
  signal_frame frame{};
  frame.agent.redis_errors = 3;

  if (!sink.publish(frame)) {
    return fail("test_redis_health_metrics_include_error_counter", "publish should succeed with mock redis");
  }

  bool found_redis_errors = false;
  for (std::size_t i = 0; i + 2 < g_redis_mock.last_argv.size(); ++i) {
    if (g_redis_mock.last_argv[i] == "edge:test:agent:redis_errors") {
      found_redis_errors = g_redis_mock.last_argv[i + 2].find("3") != std::string::npos;
      break;
    }
  }

  if (!found_redis_errors) {
    return fail("test_redis_health_metrics_include_error_counter", "expected agent:redis_errors metric in TS.MADD payload");
  }

  return 0;
}

int test_redis_sink_publish_logic() {
  g_redis_mock = {};

  RedisTsOptions options;
  options.publish_health = false;
  options.key_prefix = "edge:test";

  RedisTsSink sink(options);
  signal_frame frame{};
  frame.cpu = 12.0F;

  if (!sink.publish(frame)) {
    return fail("test_redis_sink_publish_logic", "publish should succeed with mock redis");
  }

  if (g_redis_mock.command_argv_calls != 1) {
    return fail("test_redis_sink_publish_logic", "expected one TS.MADD call");
  }

  if (g_redis_mock.last_argv.empty() || g_redis_mock.last_argv.front() != "TS.MADD") {
    return fail("test_redis_sink_publish_logic", "TS.MADD command not emitted");
  }

  for (const auto& arg : g_redis_mock.last_argv) {
    if (arg.find("agent:heartbeat") != std::string::npos) {
      return fail("test_redis_sink_publish_logic", "health metrics should be omitted when disabled");
    }
  }

  return 0;
}

int test_end_to_end_sensor_to_sink_pipeline() {
  g_redis_mock = {};

  std::FILE* cpu_file = std::tmpfile();
  std::FILE* psi_cpu_file = std::tmpfile();
  std::FILE* psi_mem_file = std::tmpfile();
  std::FILE* psi_io_file = std::tmpfile();
  std::FILE* meminfo_file = std::tmpfile();
  std::FILE* vmstat_file = std::tmpfile();
  std::FILE* interrupts_file = std::tmpfile();
  std::FILE* softirqs_file = std::tmpfile();
  std::FILE* core_throttle_file = std::tmpfile();
  std::FILE* package_throttle_file = std::tmpfile();

  if (!write_temp_file(cpu_file,
                       "cpu  100 0 100 800 0 0 0 0 0 0\n") ||
      !write_temp_file(psi_cpu_file, "some avg10=9.00 avg60=0.00 avg300=0.00 total=1\n") ||
      !write_temp_file(psi_mem_file, "some avg10=16.00 avg60=0.00 avg300=0.00 total=1\n") ||
      !write_temp_file(psi_io_file, "some avg10=17.00 avg60=0.00 avg300=0.00 total=1\n") ||
      !write_temp_file(meminfo_file,
                       "MemTotal: 1000 kB\nMemAvailable: 500 kB\nDirty: 250000 kB\nWriteback: 150000 kB\n") ||
      !write_temp_file(vmstat_file, "pgscan_kswapd 100\npgscan_direct 50\npgsteal_kswapd 80\npgsteal_direct 30\n") ||
      !write_temp_file(interrupts_file, "           CPU0\n 0: 100 IO-APIC-edge timer\n") ||
      !write_temp_file(softirqs_file,
                       "                    CPU0\nHI: 0\nTIMER: 100\nNET_TX: 100\nNET_RX: 100\n") ||
      !write_temp_file(core_throttle_file, "10\n") || !write_temp_file(package_throttle_file, "10\n")) {
    return fail("test_end_to_end_sensor_to_sink_pipeline", "failed writing synthetic sensor fixtures");
  }

  CpuSensor cpu_sensor(cpu_file, true);
  PsiSensor psi_sensor({{{"cpu", psi_cpu_file}, {"memory", psi_mem_file}, {"io", psi_io_file}}}, true);
  MemorySensor memory_sensor(meminfo_file, vmstat_file, true);
  InterruptsSensor interrupts_sensor(interrupts_file, true);
  SoftirqsSensor softirqs_sensor(softirqs_file, true);
  std::vector<CpuThrottleSensor::ThermalThrottleSource> cores{};
  cores.push_back(CpuThrottleSensor::ThermalThrottleSource{core_throttle_file, package_throttle_file, 0, 0, false});
  CpuThrottleSensor power_sensor(std::move(cores), true);

  SchedulerPressure scheduler_pressure;
  MemoryPressure memory_pressure;
  IoPressure io_pressure;
  ThermalPressure thermal_pressure;
  PowerPressure power_pressure;
  LatencyJitter latency_jitter;
  RealtimeRisk realtime_risk;
  SaturationRisk saturation_risk;
  SystemState system_state;

  RedisTsOptions options;
  options.publish_health = false;
  options.key_prefix = "edge:test";
  RedisTsSink sink(options);

  signal_frame frame{};

  cpu_sensor.sample(frame);
  psi_sensor.sample(frame);
  memory_sensor.sample(frame);
  interrupts_sensor.sample(frame);
  softirqs_sensor.sample(frame);
  power_sensor.sample(frame);

  if (!write_temp_file(cpu_file,
                       "cpu  200 0 200 900 0 0 0 0 0 0\n") ||
      !write_temp_file(interrupts_file, "           CPU0\n 0: 300 IO-APIC-edge timer\n") ||
      !write_temp_file(softirqs_file,
                       "                    CPU0\nHI: 0\nTIMER: 250\nNET_TX: 250\nNET_RX: 250\n") ||
      !write_temp_file(core_throttle_file, "11\n") || !write_temp_file(package_throttle_file, "11\n")) {
    return fail("test_end_to_end_sensor_to_sink_pipeline", "failed updating synthetic sensor fixtures");
  }

  for (int tick = 0; tick < 3; ++tick) {
    frame.monotonic_ns = 1'000'000'000ULL * static_cast<std::uint64_t>(tick + 1);
    cpu_sensor.sample(frame);
    psi_sensor.sample(frame);
    memory_sensor.sample(frame);
    interrupts_sensor.sample(frame);
    softirqs_sensor.sample(frame);
    power_sensor.sample(frame);

    frame.disk = 80.0F;
    frame.network = 0.90F;
    frame.thermal = 5.0F;

    scheduler_pressure.sample(frame);
    memory_pressure.sample(frame);
    io_pressure.sample(frame);
    thermal_pressure.sample(frame);
    power_pressure.sample(frame);
    latency_jitter.sample(frame);
    realtime_risk.sample(frame);
    saturation_risk.sample(frame);
    system_state.sample(frame);
  }

  if (frame.state < system_state::UNSTABLE) {
    return fail("test_end_to_end_sensor_to_sink_pipeline", "expected state machine to leave stable/degraded states");
  }

  if (!sink.publish(frame)) {
    return fail("test_end_to_end_sensor_to_sink_pipeline", "pipeline frame should publish to sink");
  }

  bool found_realtime = false;
  bool found_saturation = false;
  bool found_state = false;
  for (std::size_t i = 0; i + 2 < g_redis_mock.last_argv.size(); ++i) {
    if (g_redis_mock.last_argv[i] == "edge:test:risk:realtime_risk") {
      found_realtime = true;
    }
    if (g_redis_mock.last_argv[i] == "edge:test:risk:saturation_risk") {
      found_saturation = true;
    }
    if (g_redis_mock.last_argv[i] == "edge:test:risk:state") {
      found_state = g_redis_mock.last_argv[i + 2] == "2.000000" || g_redis_mock.last_argv[i + 2] == "3.000000";
    }
  }

  if (!found_realtime || !found_saturation || !found_state) {
    return fail("test_end_to_end_sensor_to_sink_pipeline", "sink payload missing integrated risk/state signals");
  }

  return 0;
}

}  // namespace

int main() {
  if (int rc = test_memory_sensor_parser(); rc != 0) return rc;
  if (int rc = test_memory_pressure_computation_and_ema(); rc != 0) return rc;
  if (int rc = test_thermal_pressure_warning_window_configurable(); rc != 0) return rc;
  if (int rc = test_sampler_should_sample_every(); rc != 0) return rc;
  if (int rc = test_sensor_dispatches_once_per_tick(); rc != 0) return rc;
  if (int rc = test_config_parsing_edge_cases(); rc != 0) return rc;
  if (int rc = test_interrupts_and_softirqs_delta_and_underflow_protection(); rc != 0) return rc;
  if (int rc = test_thermal_sensor_headroom_and_all_zones_fail_fallback(); rc != 0) return rc;
  if (int rc = test_redis_sink_publish_logic(); rc != 0) return rc;
  if (int rc = test_redis_health_metrics_include_error_counter(); rc != 0) return rc;
  if (int rc = test_gpu_memory_and_emc_metrics_are_distinct(); rc != 0) return rc;
  if (int rc = test_end_to_end_sensor_to_sink_pipeline(); rc != 0) return rc;

  std::cout << "[PASS] agent unit tests\n";
  return 0;
}
