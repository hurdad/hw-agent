#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>

#include "model/signal_frame.hpp"
#include "sensors/cpu.hpp"
#include "sensors/cpufreq.hpp"
#include "sensors/disk.hpp"
#include "sensors/power.hpp"
#include "sensors/psi.hpp"
#include "sensors/thermal.hpp"

using hw_agent::model::signal_frame;
using hw_agent::sensors::CpuFreqSensor;
using hw_agent::sensors::CpuSensor;
using hw_agent::sensors::DiskSensor;
using hw_agent::sensors::PowerSensor;
using hw_agent::sensors::PsiSensor;
using hw_agent::sensors::ThermalSensor;

namespace {

bool almost_equal(float a, float b, float eps = 1e-4F) { return std::fabs(a - b) <= eps; }

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

int test_cpu_sensor_with_injected_proc_stat() {
  std::FILE* stat_file = std::tmpfile();
  if (!write_temp_file(stat_file, "cpu  100 20 30 400 50 0 0 0 0 0\n")) {
    return fail("test_cpu_sensor_with_injected_proc_stat", "failed writing first proc/stat snapshot");
  }

  CpuSensor sensor(stat_file, false);
  signal_frame frame{};
  if (!sensor.sample(frame) || !almost_equal(frame.cpu, 0.0F)) {
    return fail("test_cpu_sensor_with_injected_proc_stat", "first sample should initialize baseline");
  }

  if (!write_temp_file(stat_file, "cpu  140 30 40 420 60 0 0 0 0 0\n")) {
    return fail("test_cpu_sensor_with_injected_proc_stat", "failed writing second proc/stat snapshot");
  }

  if (!sensor.sample(frame) || !almost_equal(frame.cpu, 66.6667F)) {
    return fail("test_cpu_sensor_with_injected_proc_stat", "computed utilization mismatch");
  }

  std::fclose(stat_file);
  return 0;
}

int test_cpu_sensor_counter_boundary_guard() {
  std::FILE* stat_file = std::tmpfile();
  if (!write_temp_file(stat_file, "cpu  500 20 30 400 50 0 0 0 0 0\n")) {
    return fail("test_cpu_sensor_counter_boundary_guard", "failed writing first proc/stat snapshot");
  }

  CpuSensor sensor(stat_file, false);
  signal_frame frame{};
  if (!sensor.sample(frame) || !almost_equal(frame.cpu, 0.0F)) {
    return fail("test_cpu_sensor_counter_boundary_guard", "first sample should initialize baseline");
  }

  if (!write_temp_file(stat_file, "cpu  100 20 30 50 10 0 0 0 0 0\n")) {
    return fail("test_cpu_sensor_counter_boundary_guard", "failed writing wrapped proc/stat snapshot");
  }

  if (!sensor.sample(frame) || !almost_equal(frame.cpu, 0.0F)) {
    return fail("test_cpu_sensor_counter_boundary_guard", "wrapped counters should clamp utilization to zero");
  }

  std::fclose(stat_file);
  return 0;
}

int test_disk_sensor_with_injected_diskstats() {
  std::FILE* diskstats = std::tmpfile();
  if (!write_temp_file(diskstats, "8 0 sda 100 0 0 0 200 0 0 0 1 1000 3000\n8 1 sda1 5 0 0 0 10 0 0 0 0 50 75\n7 0 loop0 11 0 0 0 12 0 0 0 0 40 40\n")) {
    return fail("test_disk_sensor_with_injected_diskstats", "failed writing first diskstats snapshot");
  }

  DiskSensor sensor(diskstats, false);
  signal_frame frame{};
  if (!sensor.sample(frame) || !almost_equal(frame.disk, 0.0F)) {
    return fail("test_disk_sensor_with_injected_diskstats", "first sample should initialize baseline");
  }

  if (!write_temp_file(diskstats, "8 0 sda 110 0 0 0 220 0 0 0 1 1200 3600\n")) {
    return fail("test_disk_sensor_with_injected_diskstats", "failed writing second diskstats snapshot");
  }

  if (!sensor.sample(frame) || !almost_equal(frame.disk, 20.0F)) {
    return fail("test_disk_sensor_with_injected_diskstats", "disk wait estimate mismatch");
  }

  if (!write_temp_file(
          diskstats,
          "259 0 nvme0n1 100 0 0 0 200 0 0 0 1 1000 3000\n259 1 nvme0n1p1 5 0 0 0 10 0 0 0 0 50 75\n")) {
    return fail("test_disk_sensor_with_injected_diskstats", "failed writing third diskstats snapshot");
  }

  if (!sensor.sample(frame) || !almost_equal(frame.disk, 0.0F)) {
    return fail("test_disk_sensor_with_injected_diskstats", "nvme baseline sample should initialize");
  }

  if (!write_temp_file(diskstats, "259 0 nvme0n1 110 0 0 0 220 0 0 0 1 1200 3600\n")) {
    return fail("test_disk_sensor_with_injected_diskstats", "failed writing fourth diskstats snapshot");
  }

  if (!sensor.sample(frame) || !almost_equal(frame.disk, 20.0F)) {
    return fail("test_disk_sensor_with_injected_diskstats", "nvme disk wait estimate mismatch");
  }

  std::fclose(diskstats);
  return 0;
}

int test_thermal_sensor_with_injected_zone_files() {
  std::FILE* zone0 = std::tmpfile();
  std::FILE* zone1 = std::tmpfile();
  if (!write_temp_file(zone0, "65000\n") || !write_temp_file(zone1, "72000\n")) {
    return fail("test_thermal_sensor_with_injected_zone_files", "failed writing thermal zone temp files");
  }

  std::vector<ThermalSensor::ZoneSource> zones{{"cpu", "", zone0}, {"gpu", "", zone1}};
  ThermalSensor sensor(85.0F, std::move(zones), false);
  signal_frame frame{};
  if (!sensor.sample(frame)) {
    return fail("test_thermal_sensor_with_injected_zone_files", "sample should succeed");
  }

  if (!almost_equal(frame.thermal, 13.0F) || sensor.raw().hottest_zone != "gpu") {
    return fail("test_thermal_sensor_with_injected_zone_files", "headroom or hottest zone mismatch");
  }

  std::fclose(zone0);
  std::fclose(zone1);
  return 0;
}

int test_power_sensor_with_injected_throttle_files() {
  std::FILE* core0 = std::tmpfile();
  std::FILE* pkg0 = std::tmpfile();
  std::FILE* core1 = std::tmpfile();
  std::FILE* pkg1 = std::tmpfile();

  if (!write_temp_file(core0, "10\n") || !write_temp_file(pkg0, "20\n") || !write_temp_file(core1, "3\n") ||
      !write_temp_file(pkg1, "7\n")) {
    return fail("test_power_sensor_with_injected_throttle_files", "failed writing first throttle count snapshot");
  }

  std::vector<PowerSensor::ThermalThrottleSource> cores{{core0, pkg0}, {core1, pkg1}};
  PowerSensor sensor(std::move(cores), false);
  signal_frame frame{};
  if (!sensor.sample(frame) || !almost_equal(frame.power, 0.0F)) {
    return fail("test_power_sensor_with_injected_throttle_files", "first sample should initialize baseline");
  }

  if (!write_temp_file(core0, "11\n") || !write_temp_file(pkg0, "20\n") || !write_temp_file(core1, "3\n") ||
      !write_temp_file(pkg1, "7\n")) {
    return fail("test_power_sensor_with_injected_throttle_files", "failed writing second throttle count snapshot");
  }

  if (!sensor.sample(frame) || !almost_equal(frame.power, 0.5F)) {
    return fail("test_power_sensor_with_injected_throttle_files", "throttle ratio mismatch");
  }

  std::fclose(core0);
  std::fclose(pkg0);
  std::fclose(core1);
  std::fclose(pkg1);
  return 0;
}

int test_cpufreq_sensor_with_injected_scaling_cur_freq_files() {
  std::FILE* cpu0 = std::tmpfile();
  std::FILE* cpu1 = std::tmpfile();
  if (!write_temp_file(cpu0, "1000000\n") || !write_temp_file(cpu1, "2000000\n")) {
    return fail("test_cpufreq_sensor_with_injected_scaling_cur_freq_files", "failed writing first cpufreq snapshot");
  }

  CpuFreqSensor sensor({cpu0, cpu1}, false);
  signal_frame frame{};
  if (!sensor.sample(frame) || !almost_equal(frame.cpufreq, 1500.0F)) {
    return fail("test_cpufreq_sensor_with_injected_scaling_cur_freq_files", "first average MHz mismatch");
  }

  if (!write_temp_file(cpu0, "2000000\n") || !write_temp_file(cpu1, "2000000\n")) {
    return fail("test_cpufreq_sensor_with_injected_scaling_cur_freq_files", "failed writing second cpufreq snapshot");
  }

  if (!sensor.sample(frame) || !almost_equal(frame.cpufreq, 1625.0F)) {
    return fail("test_cpufreq_sensor_with_injected_scaling_cur_freq_files", "EMA MHz mismatch");
  }

  std::fclose(cpu0);
  std::fclose(cpu1);
  return 0;
}

int test_psi_sensor_with_injected_pressure_files() {
  std::FILE* cpu = std::tmpfile();
  std::FILE* memory = std::tmpfile();
  std::FILE* io = std::tmpfile();

  if (!write_temp_file(cpu, "some avg10=12.34 avg60=0.0 avg300=0.0 total=1\n") ||
      !write_temp_file(memory, "full avg10=2.50 avg60=0.0 avg300=0.0 total=2\n") ||
      !write_temp_file(io, "full avg10=0.75 avg60=0.0 avg300=0.0 total=3\n")) {
    return fail("test_psi_sensor_with_injected_pressure_files", "failed writing psi snapshots");
  }

  PsiSensor sensor({{{"cpu", cpu}, {"memory", memory}, {"io", io}}}, false);
  signal_frame frame{};

  if (!sensor.sample(frame)) {
    return fail("test_psi_sensor_with_injected_pressure_files", "sample should succeed");
  }

  if (!almost_equal(frame.psi, 12.34F) || !almost_equal(frame.psi_memory, 2.5F) || !almost_equal(frame.psi_io, 0.75F)) {
    return fail("test_psi_sensor_with_injected_pressure_files", "avg10 parsing mismatch");
  }

  std::fclose(cpu);
  std::fclose(memory);
  std::fclose(io);
  return 0;
}

}  // namespace

int main() {
  if (int rc = test_cpu_sensor_with_injected_proc_stat(); rc != 0) {
    return rc;
  }
  if (int rc = test_cpu_sensor_counter_boundary_guard(); rc != 0) {
    return rc;
  }
  if (int rc = test_disk_sensor_with_injected_diskstats(); rc != 0) {
    return rc;
  }
  if (int rc = test_thermal_sensor_with_injected_zone_files(); rc != 0) {
    return rc;
  }
  if (int rc = test_power_sensor_with_injected_throttle_files(); rc != 0) {
    return rc;
  }
  if (int rc = test_cpufreq_sensor_with_injected_scaling_cur_freq_files(); rc != 0) {
    return rc;
  }
  if (int rc = test_psi_sensor_with_injected_pressure_files(); rc != 0) {
    return rc;
  }

  std::cout << "[PASS] sensors unit tests\n";
  return 0;
}
