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
#include <utility>

#include <unistd.h>

#include <hiredis/hiredis.h>

#include "core/config.hpp"
#include "core/sampler.hpp"
#include "derived/memory_pressure.hpp"
#include "sensors/thermal.hpp"
#include "sensors/psi.hpp"
#include "sensors/power.hpp"
#include "sensors/disk.hpp"
#include "sensors/cpufreq.hpp"
#include "sensors/cpu.hpp"
#include "model/signal_frame.hpp"
#include "sensors/interrupts.hpp"
#include "sensors/memory.hpp"
#include "sensors/softirqs.hpp"
#include "sinks/redis_ts.hpp"

using hw_agent::core::Sampler;
using hw_agent::core::load_agent_config;
using hw_agent::derived::MemoryPressure;
using hw_agent::model::signal_frame;
using hw_agent::sensors::CpuFreqSensor;
using hw_agent::sensors::CpuSensor;
using hw_agent::sensors::DiskSensor;
using hw_agent::sensors::InterruptsSensor;
using hw_agent::sensors::MemorySensor;
using hw_agent::sensors::PowerSensor;
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

  frame.timestamp = 1'000'000'000ULL;
  interrupts.sample(frame);
  softirqs.sample(frame);

  if (!almost_equal(frame.irq, 0.0F) || !almost_equal(frame.softirqs, 0.0F)) {
    return fail("test_interrupts_and_softirqs_delta_and_underflow_protection", "first sample must initialize baseline");
  }

  if (!write_temp_file(interrupts_file, "intr 150\n") ||
      !write_temp_file(softirqs_file, "NET_RX: 130 110\nTIMER: 60 55\n")) {
    return fail("test_interrupts_and_softirqs_delta_and_underflow_protection", "failed to write second sample");
  }

  frame.timestamp = 2'000'000'000ULL;
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

  frame.timestamp = 3'000'000'000ULL;
  interrupts.sample(frame);
  softirqs.sample(frame);

  if (!almost_equal(frame.irq, 0.0F) || !almost_equal(frame.softirqs, 0.0F)) {
    return fail("test_interrupts_and_softirqs_delta_and_underflow_protection", "counter underflow must clamp to zero");
  }

  return 0;
}


int test_cpu_sensor_with_injected_proc_stat() {
  std::FILE* proc_stat = std::tmpfile();
  if (!write_temp_file(proc_stat, "cpu  100 0 100 100 0 0 0 0 0 0\n")) {
    return fail("test_cpu_sensor_with_injected_proc_stat", "failed to write initial /proc/stat sample");
  }

  CpuSensor sensor(proc_stat, true);
  signal_frame frame{};
  sensor.sample(frame);

  if (!almost_equal(frame.cpu, 0.0F)) {
    return fail("test_cpu_sensor_with_injected_proc_stat", "first sample should initialize baseline");
  }

  if (!write_temp_file(proc_stat, "cpu  150 0 100 120 0 0 0 0 0 0\n")) {
    return fail("test_cpu_sensor_with_injected_proc_stat", "failed to write second /proc/stat sample");
  }

  sensor.sample(frame);
  if (!almost_equal(frame.cpu, 71.4286F, 1e-3F)) {
    return fail("test_cpu_sensor_with_injected_proc_stat", "cpu utilization delta parsing mismatch");
  }

  return 0;
}

int test_disk_sensor_with_injected_diskstats() {
  std::FILE* diskstats = std::tmpfile();
  if (!write_temp_file(diskstats,
                       "7 0 loop0 1 0 1 1 1 0 1 1 0 1 1\n"
                       "8 0 sda 100 0 0 0 200 0 0 0 0 300 500\n"
                       "8 1 sda1 10 0 0 0 10 0 0 0 0 10 10\n")) {
    return fail("test_disk_sensor_with_injected_diskstats", "failed to write initial /proc/diskstats sample");
  }

  DiskSensor sensor(diskstats, true);
  signal_frame frame{};
  sensor.sample(frame);

  if (!almost_equal(frame.disk, 0.0F)) {
    return fail("test_disk_sensor_with_injected_diskstats", "first sample should initialize baseline");
  }

  if (!write_temp_file(diskstats,
                       "8 0 sda 130 0 0 0 230 0 0 0 0 350 620\n")) {
    return fail("test_disk_sensor_with_injected_diskstats", "failed to write second /proc/diskstats sample");
  }

  sensor.sample(frame);
  if (!almost_equal(frame.disk, 2.0F)) {
    return fail("test_disk_sensor_with_injected_diskstats", "disk wait estimation mismatch");
  }

  return 0;
}

int test_thermal_and_power_sensors_with_injected_files() {
  std::FILE* zone0 = std::tmpfile();
  std::FILE* zone1 = std::tmpfile();
  if (!write_temp_file(zone0, "70000\n") || !write_temp_file(zone1, "82000\n")) {
    return fail("test_thermal_and_power_sensors_with_injected_files", "failed to write thermal zone samples");
  }

  std::vector<ThermalSensor::ZoneHandle> zones;
  zones.push_back({"cpu", zone0});
  zones.push_back({"gpu", zone1});
  ThermalSensor thermal(90.0F, std::move(zones), true);

  signal_frame frame{};
  thermal.sample(frame);
  if (!almost_equal(frame.thermal, 8.0F) || thermal.raw().hottest_zone != "gpu") {
    return fail("test_thermal_and_power_sensors_with_injected_files", "thermal headroom or hottest zone mismatch");
  }

  std::FILE* core0 = std::tmpfile();
  std::FILE* pkg0 = std::tmpfile();
  std::FILE* core1 = std::tmpfile();
  std::FILE* pkg1 = std::tmpfile();

  if (!write_temp_file(core0, "1\n") || !write_temp_file(pkg0, "1\n") || !write_temp_file(core1, "5\n") ||
      !write_temp_file(pkg1, "2\n")) {
    return fail("test_thermal_and_power_sensors_with_injected_files", "failed to write initial power samples");
  }

  std::vector<PowerSensor::CoreThrottleFiles> cores;
  cores.push_back({core0, pkg0});
  cores.push_back({core1, pkg1});
  PowerSensor power(std::move(cores), true);

  power.sample(frame);
  if (!almost_equal(frame.power, 0.0F)) {
    return fail("test_thermal_and_power_sensors_with_injected_files", "first power sample should initialize baseline");
  }

  if (!write_temp_file(core0, "2\n") || !write_temp_file(pkg0, "1\n") || !write_temp_file(core1, "5\n") ||
      !write_temp_file(pkg1, "3\n")) {
    return fail("test_thermal_and_power_sensors_with_injected_files", "failed to write second power samples");
  }

  power.sample(frame);
  if (!almost_equal(frame.power, 1.0F) || power.raw().throttled_cores != 2) {
    return fail("test_thermal_and_power_sensors_with_injected_files", "power throttle ratio mismatch");
  }

  return 0;
}

int test_cpufreq_and_psi_sensors_with_injected_files() {
  std::FILE* cpuinfo = std::tmpfile();
  if (!write_temp_file(cpuinfo, "cpu MHz\t\t: 1000.0\ncpu MHz\t\t: 2000.0\n")) {
    return fail("test_cpufreq_and_psi_sensors_with_injected_files", "failed to write initial /proc/cpuinfo sample");
  }

  CpuFreqSensor cpufreq(cpuinfo, true);
  signal_frame frame{};
  cpufreq.sample(frame);
  if (!almost_equal(frame.cpufreq, 1500.0F)) {
    return fail("test_cpufreq_and_psi_sensors_with_injected_files", "first cpufreq average mismatch");
  }

  if (!write_temp_file(cpuinfo, "cpu MHz\t\t: 3000.0\ncpu MHz\t\t: 3000.0\n")) {
    return fail("test_cpufreq_and_psi_sensors_with_injected_files", "failed to write second /proc/cpuinfo sample");
  }

  cpufreq.sample(frame);
  if (!almost_equal(frame.cpufreq, 1875.0F)) {
    return fail("test_cpufreq_and_psi_sensors_with_injected_files", "cpufreq EMA mismatch");
  }

  std::FILE* psi_cpu = std::tmpfile();
  std::FILE* psi_mem = std::tmpfile();
  std::FILE* psi_io = std::tmpfile();
  if (!write_temp_file(psi_cpu, "some avg10=1.23 avg60=0 avg300=0 total=1\n") ||
      !write_temp_file(psi_mem, "full avg10=2.50 avg60=0 avg300=0 total=1\n") ||
      !write_temp_file(psi_io, "some avg10=3.75 avg60=0 avg300=0 total=1\n")) {
    return fail("test_cpufreq_and_psi_sensors_with_injected_files", "failed to write PSI samples");
  }

  PsiSensor psi(psi_cpu, psi_mem, psi_io, true);
  psi.sample(frame);

  if (!almost_equal(frame.psi, 1.23F) || !almost_equal(frame.psi_memory, 2.50F) ||
      !almost_equal(frame.psi_io, 3.75F)) {
    return fail("test_cpufreq_and_psi_sensors_with_injected_files", "PSI avg10 parsing mismatch");
  }

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
  frame.emc_util = 73.0F;

  if (!sink.publish(frame)) {
    return fail("test_gpu_memory_and_emc_metrics_are_distinct", "publish should succeed with mock redis");
  }

  bool found_gpu_mem_util = false;
  bool found_emc_util = false;
  for (std::size_t i = 0; i + 2 < g_redis_mock.last_argv.size(); ++i) {
    if (g_redis_mock.last_argv[i] == "edge:test:raw:gpu_mem_util") {
      found_gpu_mem_util = g_redis_mock.last_argv[i + 2].find("41") != std::string::npos;
    }
    if (g_redis_mock.last_argv[i] == "edge:test:raw:emc_util") {
      found_emc_util = g_redis_mock.last_argv[i + 2].find("73") != std::string::npos;
    }
  }

  if (!found_gpu_mem_util || !found_emc_util) {
    return fail("test_gpu_memory_and_emc_metrics_are_distinct", "expected distinct gpu_mem_util and emc_util outputs");
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

}  // namespace

int main() {
  if (int rc = test_memory_sensor_parser(); rc != 0) return rc;
  if (int rc = test_memory_pressure_computation_and_ema(); rc != 0) return rc;
  if (int rc = test_sampler_should_sample_every(); rc != 0) return rc;
  if (int rc = test_config_parsing_edge_cases(); rc != 0) return rc;
  if (int rc = test_interrupts_and_softirqs_delta_and_underflow_protection(); rc != 0) return rc;
  if (int rc = test_cpu_sensor_with_injected_proc_stat(); rc != 0) return rc;
  if (int rc = test_disk_sensor_with_injected_diskstats(); rc != 0) return rc;
  if (int rc = test_thermal_and_power_sensors_with_injected_files(); rc != 0) return rc;
  if (int rc = test_cpufreq_and_psi_sensors_with_injected_files(); rc != 0) return rc;
  if (int rc = test_redis_sink_publish_logic(); rc != 0) return rc;
  if (int rc = test_redis_health_metrics_include_error_counter(); rc != 0) return rc;
  if (int rc = test_gpu_memory_and_emc_metrics_are_distinct(); rc != 0) return rc;

  std::cout << "[PASS] agent unit tests\n";
  return 0;
}
