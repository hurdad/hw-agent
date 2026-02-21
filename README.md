# hw-agent

**A deterministic Linux system stability sensor**

`hw-agent` converts low-level kernel signals into high-level machine stability indicators that schedulers, supervisors, and edge orchestration systems can safely act on.

Instead of graphing CPU usage, this agent answers the real question:

> *“Can this node safely run more work right now?”*

---

## What It Measures

The agent continuously evaluates two independent risks:

| Risk                | Meaning                                                 |
| ------------------- | ------------------------------------------------------- |
| **realtime_risk**   | Probability latency-sensitive tasks will miss deadlines |
| **saturation_risk** | Probability system throughput collapse is imminent      |

These signals are derived from kernel behavior — not utilization heuristics.

---

## Why This Exists

Traditional monitoring reports *resource usage*.
But system failures are caused by *loss of scheduling headroom*.

Examples where CPU% lies:

| Scenario         | CPU | Result               |
| ---------------- | --- | -------------------- |
| Interrupt storm  | 35% | audio/video glitches |
| Memory stalls    | 40% | dropped packets      |
| Thermal throttle | 50% | frame skips          |
| Disk pressure    | 20% | control loop delay   |

`hw-agent` observes Linux scheduler physics directly:

* PSI (pressure stall information)
* interrupt bursts
* reclaim stalls
* IO latency
* frequency scaling
* thermal headroom

Then converts them into actionable risk signals.

---

## Architecture

Linux kernel signals
→ Raw sensors
→ Derived physical pressures
→ Risk models
→ RedisTimeSeries signal bus
→ Schedulers / Supervisors / AI

This is not a metrics exporter.
It is a **machine state estimator**.

---

## Output Data Model

Redis keys:

```
edge:<node>:risk:realtime
edge:<node>:risk:saturation

edge:<node>:pressure:scheduler
edge:<node>:pressure:memory
edge:<node>:pressure:io
edge:<node>:pressure:thermal
edge:<node>:pressure:power

edge:<node>:agent:heartbeat
edge:<node>:agent:loop_jitter
edge:<node>:agent:compute_time
edge:<node>:agent:redis_latency
edge:<node>:agent:redis_errors
edge:<node>:agent:missed_cycles
edge:<node>:agent:sensor_failures
```

The agent publishes every cycle (default 100ms).

---

## Quick Start (Full Demo)

Requirements:

* Docker
* Docker Compose

Run (default, no GPU requirements):

```bash
cd docker
docker compose up --build
```

`hw_agent` is configured with `pid: host` in Compose so `/proc/*` sensors read host-kernel process/scheduler state instead of an isolated container PID namespace.

Run with GPU support (NVIDIA):

```bash
cd docker
docker compose -f docker-compose.yml -f docker-compose.gpu.yml up --build
```

Open Grafana:

```
http://localhost:3000
user: admin
pass: admin
```

You will immediately see the node stability dashboard.

No manual setup required.

---

## Building Locally

Requirements:

* CMake ≥ 3.16
* C++20 compiler
* yaml-cpp
* hiredis

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

## Configuration

`configs/agent.yaml`

```yaml
tick_ms: 100

redis:
  host: localhost
  port: 6379

thermal:
  throttle_temp: 92

agent:
  publish_health: true
  stdout_debug: false  # set false in production to disable stdout logs
```

---

## Design Goals

* Deterministic runtime (single thread)
* No background workers
* No allocation in hot path
* Works without GPU / special hardware
* Safe for realtime environments
* Fail-open (never blocks workload)

---

## Not a Monitoring Tool

This project does **not** replace Prometheus/Grafana metrics.

It provides a higher level signal:

> machine stability state

Think of it as a sensor feeding orchestration logic.

---

## Example Use Cases

* edge compute schedulers
* robotics controllers
* media pipelines
* inference batching control
* adaptive rate limiters
* cluster placement decisions

---

## License

Apache License 2.0
