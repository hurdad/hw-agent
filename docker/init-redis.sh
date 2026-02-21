#!/bin/sh
set -eu

REDIS_HOST="${REDIS_HOST:-127.0.0.1}"
REDIS_PORT="${REDIS_PORT:-6379}"
RETENTION_MS=86400000

wait_for_redis() {
  until redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" PING >/dev/null 2>&1; do
    sleep 1
  done
}

create_ts() {
  key="$1"
  redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" \
    TS.CREATE "$key" RETENTION "$RETENTION_MS" DUPLICATE_POLICY LAST >/dev/null 2>&1 || true
}

create_rule() {
  source_key="$1"
  dest_key="$2"
  redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" \
    TS.CREATERULE "$source_key" "$dest_key" AGGREGATION last 1 >/dev/null 2>&1 || true
}

wait_for_redis

# Source keys published by hw_agent.
for key in \
  edge:node:raw:psi \
  edge:node:raw:cpu \
  edge:node:raw:irq \
  edge:node:raw:memory \
  edge:node:raw:thermal \
  edge:node:raw:power \
  edge:node:raw:disk \
  edge:node:raw:network \
  edge:node:risk:realtime_risk \
  edge:node:risk:saturation_risk \
  edge:node:risk:state \
  edge:node:derived:scheduler_pressure \
  edge:node:derived:memory_pressure \
  edge:node:derived:io_pressure \
  edge:node:derived:thermal_pressure \
  edge:node:derived:power_pressure \
  edge:node:derived:latency_jitter \
  edge:node:agent:heartbeat \
  edge:node:agent:loop_jitter \
  edge:node:agent:compute_time \
  edge:node:agent:redis_latency \
  edge:node:agent:missed_cycles \
  edge:node:agent:sensor_failures
 do
  create_ts "$key"
done

# Dashboard-facing keys.
for key in \
  edge:node1:risk:realtime \
  edge:node1:risk:saturation \
  edge:node1:pressure:scheduler \
  edge:node1:pressure:memory \
  edge:node1:pressure:io \
  edge:node1:pressure:thermal \
  edge:node1:agent:loop_jitter \
  edge:node1:agent:compute_time \
  edge:node1:agent:redis_latency \
  edge:node1:agent:missed_cycles \
  edge:node1:agent:sensor_failures
 do
  create_ts "$key"
done

create_rule edge:node:risk:realtime_risk edge:node1:risk:realtime
create_rule edge:node:risk:saturation_risk edge:node1:risk:saturation
create_rule edge:node:derived:scheduler_pressure edge:node1:pressure:scheduler
create_rule edge:node:derived:memory_pressure edge:node1:pressure:memory
create_rule edge:node:derived:io_pressure edge:node1:pressure:io
create_rule edge:node:derived:thermal_pressure edge:node1:pressure:thermal
create_rule edge:node:agent:loop_jitter edge:node1:agent:loop_jitter
create_rule edge:node:agent:compute_time edge:node1:agent:compute_time
create_rule edge:node:agent:redis_latency edge:node1:agent:redis_latency
create_rule edge:node:agent:missed_cycles edge:node1:agent:missed_cycles
create_rule edge:node:agent:sensor_failures edge:node1:agent:sensor_failures
