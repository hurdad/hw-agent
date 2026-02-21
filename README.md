# hw-agent

**A deterministic Linux system stability sensor**

`hw-agent` converts low-level kernel signals into high-level machine stability indicators that schedulers, supervisors, and edge orchestration systems can safely act on.

Instead of graphing CPU usage, this agent answers the real question:

> *"Can this node safely run more work right now?"*

---

## What It Measures

The agent continuously evaluates two independent risks:

| Risk                | Meaning                                                 |
| ------------------- | ------------------------------------------------------- |
| **realtime_risk**   | Probability latency-sensitive tasks will miss deadlines |
| **saturation_risk** | Probability system throughput collapse is imminent      |

These signals are derived from kernel behavior, not utilization heuristics.

---

## Why This Exists

Traditional monitoring reports *resource usage*.
System failures are often caused by *loss of scheduling headroom*.

Examples where CPU% can be misleading:

| Scenario         | CPU | Result               |
| ---------------- | --- | -------------------- |
| Interrupt storm  | 35% | audio/video glitches |
| Memory stalls    | 40% | dropped packets      |
| Thermal throttle | 50% | frame skips          |
| Disk pressure    | 20% | control loop delay   |

`hw-agent` observes Linux scheduler physics directly:

- PSI (pressure stall information)
- interrupt bursts
- reclaim stalls
- I/O latency
- frequency scaling
- thermal headroom

It then converts those signals into actionable risk metrics.

---

## Architecture

Linux kernel signals
→ Raw sensors
→ Derived physical pressures
→ Risk models
→ RedisTimeSeries signal bus
→ Schedulers / Supervisors / AI

This is not a generic metrics exporter.
It is a **machine state estimator**.

---

## Output Data Model

Redis key format (`<key_prefix>:<metric_suffix>`):

- `key_prefix` defaults to `edge:node` and is configurable in the Redis sink.
- Metric families are `raw:*`, `derived:*`, `risk:*`, and optional `agent:*` health metrics.

Metric suffixes:

```text
raw:psi
raw:psi_memory
raw:psi_io
raw:cpu
raw:irq
raw:softirqs
raw:memory
raw:thermal
raw:cpufreq
raw:cpu_throttle_ratio
raw:disk
raw:network
raw:gpu_util
raw:gpu_mem_util
raw:emc_util
raw:gpu_mem_free
raw:gpu_temp
raw:gpu_clock_ratio
raw:gpu_power_ratio
raw:gpu_throttle

derived:scheduler_pressure
derived:memory_pressure
derived:io_pressure
derived:thermal_pressure
derived:power_pressure
derived:latency_jitter

risk:realtime_risk
risk:saturation_risk
risk:state

agent:heartbeat
agent:loop_jitter
agent:compute_time
agent:redis_latency
agent:redis_errors
agent:sensor_failures
agent:missed_cycles
```

Examples with the default prefix:

- `edge:node:risk:realtime_risk`
- `edge:node:derived:scheduler_pressure`
- `edge:node:agent:heartbeat`

The agent publishes every cycle (default `100ms` / `tick_rate_hz: 10`).
For cadence details by metric, see [`docs/redis-timeseries-metrics.md`](docs/redis-timeseries-metrics.md).

---

## Quick Start (Full Demo)

Requirements:

- Docker
- Docker Compose

Run (default profile: `configs/agent.all.debug.yaml`):

```bash
cd docker
docker compose up --build
```

`hw_agent` is configured with `pid: host` in Compose so `/proc/*` sensors read host-kernel process/scheduler state instead of an isolated container PID namespace.

Select a different config profile (for example, CPU-only):

```bash
cd docker
AGENT_CONFIG=agent.cpu-only.yaml docker compose up --build
```

Run with GPU support (NVIDIA) and the discrete-GPU profile:

```bash
cd docker
AGENT_CONFIG=agent.cpu-discrete-gpu.yaml docker compose -f docker-compose.yml -f docker-compose.gpu.yml up --build
```

Open Grafana:

```text
http://localhost:3000
user: admin
pass: admin
```

You should immediately see the node stability dashboard.

Run the TUI:

```bash
docker run -it --rm --network=host ghcr.io/hurdad/hw-agent-tui:latest
```

---

## Building Locally

Requirements:

- CMake ≥ 3.16
- C++20 compiler
- yaml-cpp
- hiredis

Ubuntu:

```bash
sudo apt install build-essential cmake libyaml-cpp-dev libhiredis-dev
```

Build:

```bash
mkdir build
cd build
cmake ..
cmake --build . -j
```

Run:

```bash
./hw_agent
```

---

## Configuration Profiles

The agent ships with four config profiles under `configs/`:

| File | Intended host | Enabled sensors |
| --- | --- | --- |
| `agent.all.debug.yaml` | Generic Linux host with full sensor set and GPU support when available | `psi`, `cpu`, `interrupts`, `softirqs`, `memory`, `disk`, `network`, `tegrastats`, `thermal`, `power`, `cpufreq`, `gpu` |
| `agent.cpu-only.yaml` | CPU-only hosts (no GPU metrics) | `psi`, `cpu`, `interrupts`, `softirqs`, `memory`, `disk`, `network`, `thermal`, `power`, `cpufreq` |
| `agent.cpu-discrete-gpu.yaml` | x86/ARM hosts with discrete GPU via NVML | `psi`, `cpu`, `interrupts`, `softirqs`, `memory`, `disk`, `network`, `thermal`, `power`, `cpufreq`, `gpu` |
| `agent.cpu-tegrastats-jetson.yaml` | NVIDIA Jetson hosts using tegrastats integration | `psi`, `cpu`, `interrupts`, `softirqs`, `memory`, `disk`, `network`, `tegrastats`, `thermal`, `power`, `cpufreq` |

In Docker Compose, pick the profile with `AGENT_CONFIG`:

```bash
cd docker
AGENT_CONFIG=agent.cpu-tegrastats-jetson.yaml docker compose up --build
```

To run without Docker, pass the config file path directly:

```bash
./hw_agent configs/agent.all.debug.yaml
```

To target a specific discrete GPU on multi-GPU systems, set `gpu.device_index` in config (defaults to `0`).

---

## Design Goals

- Deterministic runtime (single thread)
- No background workers
- No allocations in the hot path
- Works without GPU / special hardware
- Safe for realtime environments
- Fail-open behavior (never blocks workload)

---

## Not a Monitoring Tool

This project does **not** replace Prometheus/Grafana metrics.

It provides a higher-level signal:

> machine stability state

Think of it as a sensor feeding orchestration logic.

---

## Example Use Cases

- edge compute schedulers
- robotics controllers
- media pipelines
- inference batching control
- adaptive rate limiters
- cluster placement decisions

---

## Additional Documentation

- Metric cadence and RedisTimeSeries behavior: [`docs/redis-timeseries-metrics.md`](docs/redis-timeseries-metrics.md)
- Redis TUI structure notes: [`docs/python-redis-tui-layout.md`](docs/python-redis-tui-layout.md)
- Redis TUI usage: [`tools/hw-agent-tui/README.md`](tools/hw-agent-tui/README.md)

---

## License

Apache License 2.0
