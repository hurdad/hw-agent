#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "model/signal_frame.hpp"
#include "core/config.hpp"
#include "risk/realtime_risk.hpp"
#include "risk/saturation_risk.hpp"
#include "risk/system_state.hpp"

using hw_agent::model::signal_frame;
using hw_agent::model::system_state;
using hw_agent::core::load_agent_config;
using hw_agent::risk::RealtimeRisk;
using hw_agent::risk::SaturationRisk;
using hw_agent::risk::SystemState;

namespace {

bool almost_equal(float a, float b, float epsilon = 1e-5F) {
  return std::fabs(a - b) <= epsilon;
}

int fail(const char* name, const char* msg) {
  std::cerr << "[FAIL] " << name << ": " << msg << '\n';
  return 1;
}

int test_realtime_risk_initial_and_ema() {
  RealtimeRisk risk;
  signal_frame frame{};

  frame.latency_jitter = 0.80F;
  frame.scheduler_pressure = 0.20F;
  frame.thermal_pressure = 0.90F;
  frame.io_pressure = 0.60F;

  risk.sample(frame);
  const float first_expected = (0.55F * 0.80F) + (0.25F * 0.20F) + (0.15F * 0.90F) + (0.05F * 0.60F);
  if (!almost_equal(frame.realtime_risk, first_expected)) {
    return fail("test_realtime_risk_initial_and_ema", "first sample mismatch");
  }

  frame.latency_jitter = 0.10F;
  frame.scheduler_pressure = 0.40F;
  frame.thermal_pressure = 0.20F;
  frame.io_pressure = 0.30F;

  risk.sample(frame);
  const float second_raw = (0.55F * 0.10F) + (0.25F * 0.40F) + (0.15F * 0.20F) + (0.05F * 0.30F);
  const float second_expected = (0.65F * first_expected) + (0.35F * second_raw);

  if (!almost_equal(frame.realtime_risk, second_expected)) {
    return fail("test_realtime_risk_initial_and_ema", "ema sample mismatch");
  }

  return 0;
}

int test_realtime_risk_clamps_inputs_and_output() {
  RealtimeRisk risk;
  signal_frame frame{};

  frame.latency_jitter = -10.0F;
  frame.scheduler_pressure = 2.0F;
  frame.thermal_pressure = 10.0F;
  frame.io_pressure = -3.0F;

  risk.sample(frame);
  const float expected = (0.55F * 0.0F) + (0.25F * 1.0F) + (0.15F * 1.0F) + (0.05F * 0.0F);
  if (!almost_equal(frame.realtime_risk, expected)) {
    return fail("test_realtime_risk_clamps_inputs_and_output", "clamp behavior mismatch");
  }

  return 0;
}

int test_saturation_risk_initial_and_ema() {
  SaturationRisk risk;
  signal_frame frame{};

  frame.scheduler_pressure = 0.90F;
  frame.memory_pressure = 0.40F;
  frame.io_pressure = 0.30F;
  frame.power_pressure = 0.70F;
  frame.thermal_pressure = 0.20F;

  risk.sample(frame);
  const float first_expected =
      (0.30F * 0.90F) + (0.25F * 0.40F) + (0.20F * 0.30F) + (0.15F * 0.70F) + (0.10F * 0.20F);
  if (!almost_equal(frame.saturation_risk, first_expected)) {
    return fail("test_saturation_risk_initial_and_ema", "first sample mismatch");
  }

  frame.scheduler_pressure = 0.20F;
  frame.memory_pressure = 0.80F;
  frame.io_pressure = 0.10F;
  frame.power_pressure = 0.50F;
  frame.thermal_pressure = 1.20F;

  risk.sample(frame);
  const float second_raw =
      (0.30F * 0.20F) + (0.25F * 0.80F) + (0.20F * 0.10F) + (0.15F * 0.50F) + (0.10F * 1.0F);
  const float second_expected = (0.82F * first_expected) + (0.18F * second_raw);

  if (!almost_equal(frame.saturation_risk, second_expected)) {
    return fail("test_saturation_risk_initial_and_ema", "ema sample mismatch");
  }

  return 0;
}

int test_system_state_hysteresis_transitions() {
  SystemState state;
  signal_frame frame{};

  frame.realtime_risk = 0.20F;
  frame.saturation_risk = 0.10F;
  state.sample(frame);
  if (frame.state != system_state::STABLE) {
    return fail("test_system_state_hysteresis_transitions", "expected STABLE");
  }

  frame.realtime_risk = 0.45F;
  frame.saturation_risk = 0.10F;
  state.sample(frame);
  if (frame.state != system_state::DEGRADED) {
    return fail("test_system_state_hysteresis_transitions", "expected DEGRADED");
  }

  frame.realtime_risk = 0.69F;
  frame.saturation_risk = 0.10F;
  state.sample(frame);
  if (frame.state != system_state::DEGRADED) {
    return fail("test_system_state_hysteresis_transitions", "expected DEGRADED hysteresis");
  }

  frame.realtime_risk = 0.70F;
  frame.saturation_risk = 0.10F;
  state.sample(frame);
  if (frame.state != system_state::UNSTABLE) {
    return fail("test_system_state_hysteresis_transitions", "expected UNSTABLE");
  }

  frame.realtime_risk = 0.88F;
  frame.saturation_risk = 0.10F;
  state.sample(frame);
  if (frame.state != system_state::CRITICAL) {
    return fail("test_system_state_hysteresis_transitions", "expected CRITICAL");
  }

  frame.realtime_risk = 0.79F;
  frame.saturation_risk = 0.10F;
  state.sample(frame);
  if (frame.state != system_state::CRITICAL) {
    return fail("test_system_state_hysteresis_transitions", "expected CRITICAL hysteresis");
  }

  frame.realtime_risk = 0.78F;
  frame.saturation_risk = 0.10F;
  state.sample(frame);
  if (frame.state != system_state::UNSTABLE) {
    return fail("test_system_state_hysteresis_transitions", "expected back to UNSTABLE");
  }

  frame.realtime_risk = 0.55F;
  frame.saturation_risk = 0.10F;
  state.sample(frame);
  if (frame.state != system_state::DEGRADED) {
    return fail("test_system_state_hysteresis_transitions", "expected back to DEGRADED");
  }

  frame.realtime_risk = 0.30F;
  frame.saturation_risk = 0.10F;
  state.sample(frame);
  if (frame.state != system_state::STABLE) {
    return fail("test_system_state_hysteresis_transitions", "expected back to STABLE");
  }

  return 0;
}

int test_config_rejects_excessive_tick_rate() {
  const auto config_path = std::filesystem::temp_directory_path() / "hw_agent_config_over_1khz.yaml";

  {
    std::ofstream out(config_path);
    out << "tick_rate_hz: 2000\n";
  }

  bool threw = false;
  try {
    (void)load_agent_config(config_path.string());
  } catch (const std::runtime_error&) {
    threw = true;
  }

  std::error_code ec;
  std::filesystem::remove(config_path, ec);

  if (!threw) {
    return fail("test_config_rejects_excessive_tick_rate", "expected load_agent_config to reject tick_rate_hz > 1000");
  }

  return 0;
}

int test_config_rejects_out_of_range_redis_port() {
  const auto config_path = std::filesystem::temp_directory_path() / "hw_agent_config_bad_redis_port.yaml";

  {
    std::ofstream out(config_path);
    out << "redis:\n";
    out << "  address: localhost:70000\n";
  }

  bool threw = false;
  try {
    (void)load_agent_config(config_path.string());
  } catch (const std::runtime_error&) {
    threw = true;
  }

  std::error_code ec;
  std::filesystem::remove(config_path, ec);

  if (!threw) {
    return fail("test_config_rejects_out_of_range_redis_port", "expected load_agent_config to reject redis port > 65535");
  }

  return 0;
}

}  // namespace

int main() {
  if (int rc = test_realtime_risk_initial_and_ema(); rc != 0) {
    return rc;
  }
  if (int rc = test_realtime_risk_clamps_inputs_and_output(); rc != 0) {
    return rc;
  }
  if (int rc = test_saturation_risk_initial_and_ema(); rc != 0) {
    return rc;
  }
  if (int rc = test_system_state_hysteresis_transitions(); rc != 0) {
    return rc;
  }
  if (int rc = test_config_rejects_excessive_tick_rate(); rc != 0) {
    return rc;
  }
  if (int rc = test_config_rejects_out_of_range_redis_port(); rc != 0) {
    return rc;
  }

  std::cout << "[PASS] risk unit tests\n";
  return 0;
}
