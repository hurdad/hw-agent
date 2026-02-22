#!/usr/bin/env bash
set -euo pipefail

REDIS_HOST="${REDIS_HOST:-127.0.0.1}"
REDIS_PORT="${REDIS_PORT:-6379}"
REDIS_DB="${REDIS_DB:-0}"
REDIS_AUTH="${REDIS_AUTH:-}"
KEY_PREFIX="${KEY_PREFIX:-hw-agent}"
RETENTION_MS="${RETENTION_MS:-0}"

redis_cli=(redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" -n "$REDIS_DB")
if [[ -n "$REDIS_AUTH" ]]; then
  redis_cli+=( -a "$REDIS_AUTH" )
fi

create_ts() {
  local key="$1"
  local group="$2"
  local field="$3"

  "${redis_cli[@]}" TS.CREATE "$key" RETENTION "$RETENTION_MS" DUPLICATE_POLICY LAST \
    LABELS app hw-agent signal_group "$group" signal "$field" >/dev/null
}

raw_fields=(
  psi
  cpu
  irq
  softirqs
  memory
  thermal
  cpufreq
  power
  disk
  network
  nvml_gpu_util
  tegra_gpu_util
  tegra_emc_util
  nvml_gpu_temp
  tegra_gpu_temp
  nvml_gpu_power_ratio
  tegra_gpu_power_mw
)
derived_fields=(scheduler_pressure memory_pressure io_pressure thermal_pressure power_pressure latency_jitter)
risk_fields=(realtime_risk saturation_risk state)
agent_fields=(heartbeat loop_jitter compute_time redis_latency sensor_failures missed_cycles)

for field in "${raw_fields[@]}"; do
  create_ts "$KEY_PREFIX:raw:$field" "raw" "$field"
done

for field in "${derived_fields[@]}"; do
  create_ts "$KEY_PREFIX:derived:$field" "derived" "$field"
done

for field in "${risk_fields[@]}"; do
  create_ts "$KEY_PREFIX:risk:$field" "risk" "$field"
done

for field in "${agent_fields[@]}"; do
  create_ts "$KEY_PREFIX:agent:$field" "agent" "$field"
done

echo "RedisTimeSeries schema ensured for prefix '$KEY_PREFIX' on $REDIS_HOST:$REDIS_PORT db=$REDIS_DB"
