# RedisTimeSeries metrics populated by `hw-agent`

This document describes every RedisTimeSeries metric written by `hw-agent`, including:

- publish cadence (how often the key is written), and
- source refresh cadence (how often the underlying value is recomputed).

## Cadence model

- **Tick interval** is controlled by `tick_rate_hz` (default `10`), so one tick is `100 ms`.
- The Redis sink publishes with one `TS.MADD` per tick and includes every configured metric key in that write.
- Some sensors run every `N` ticks, so a metric can be published every tick while its value only changes when that sensor runs.

### Default timing at `tick_rate_hz: 10`

- `1 tick` = `100 ms`
- `N ticks` = `N * 100 ms`

## Prefix and key format

All keys use the configured Redis prefix (default `edge:node`) and are written as:

- `<prefix>:raw:<metric>`
- `<prefix>:derived:<metric>`
- `<prefix>:risk:<metric>`
- `<prefix>:agent:<metric>` (when health publishing is enabled)

## Raw metrics

| Redis key suffix | Published every | Source refresh cadence | Notes |
| --- | --- | --- | --- |
| `raw:psi` | every tick (`100 ms`) | every 1 tick (`100 ms`) | From PSI CPU avg10 (`/proc/pressure/cpu`). |
| `raw:psi_memory` | every tick | every 1 tick (`100 ms`) | From PSI memory avg10 (`/proc/pressure/memory`). |
| `raw:psi_io` | every tick | every 1 tick (`100 ms`) | From PSI I/O avg10 (`/proc/pressure/io`). |
| `raw:cpu` | every tick | every 2 ticks (`200 ms`) | Overwritten by `CpuSensor` every 2 ticks; initially seeded by PSI when that sensor runs. |
| `raw:irq` | every tick | every 3 ticks (`300 ms`) | From `/proc/stat` interrupts delta rate. |
| `raw:softirqs` | every tick | every 4 ticks (`400 ms`) | From `/proc/stat` softirq delta rate. |
| `raw:memory` | every tick | every 5 ticks (`500 ms`) | Dirty + writeback pressure. |
| `raw:thermal` | every tick | every 9 ticks (`900 ms`) and every 11 ticks (`1100 ms`) | Thermal headroom (`ThermalSensor`) can be overwritten by `CpuFreqSensor` at its cadence. |
| `raw:cpufreq` | every tick | every 11 ticks (`1100 ms`) | CPU frequency pressure ratio. |
| `raw:cpu_throttle_ratio` | every tick | every 10 ticks (`1000 ms`) | CPU thermal throttle ratio `[0,1]`. |
| `raw:disk` | every tick | every 6 ticks (`600 ms`) | `/proc/diskstats` weighted I/O wait estimate. |
| `raw:network` | every tick | every 7 ticks (`700 ms`) | Interface packet drop ratio. |
| `raw:nvml_gpu_util` | every tick | every 12 ticks (`1200 ms`) | NVML GPU utilization percentage (`[0,100]`). |
| `raw:tegra_gpu_util` | every tick | every 8 ticks (`800 ms`) | Jetson GPU utilization from `tegrastats` (`GR3D_FREQ`/`GPU`). |
| `raw:gpu_mem_util` | every tick | every 12 ticks (`1200 ms`) | GPU memory utilization percentage (`[0,100]`) from NVML backend metrics. |
| `raw:tegra_emc_util` | every tick | every 8 ticks (`800 ms`) | Jetson EMC utilization from `tegrastats` (`EMC_FREQ`). |
| `raw:gpu_mem_free` | every tick | every 12 ticks (`1200 ms`) | Free GPU memory in MiB from NVML backend metrics. |
| `raw:nvml_gpu_temp` | every tick | every 12 ticks (`1200 ms`) | NVML-reported GPU temperature in Celsius. |
| `raw:tegra_gpu_temp` | every tick | every 8 ticks (`800 ms`) | Jetson GPU temperature in Celsius from `tegrastats` (`GPU@...C`). |
| `raw:gpu_clock_ratio` | every tick | every 12 ticks (`1200 ms`) | GPU clock headroom ratio from NVML. |
| `raw:nvml_gpu_power_ratio` | every tick | every 12 ticks (`1200 ms`) | Normalized NVML GPU power ratio (`power_usage / power_limit`, `[0,1]`). |
| `raw:tegra_gpu_power_mw` | every tick | every 8 ticks (`800 ms`) | Jetson total `VDD_*` rail power sum from `tegrastats`, in milliwatts. |
| `raw:gpu_throttle` | every tick | every 12 ticks (`1200 ms`) | GPU throttle/thermal-limited ratio from NVML throttle reasons. |

## Derived metrics

Derived metrics are computed every tick from current frame state (which can include raw values last refreshed on slower cadences).

| Redis key suffix | Published every | Computed every |
| --- | --- | --- |
| `derived:scheduler_pressure` | every tick (`100 ms`) | every tick (`100 ms`) |
| `derived:memory_pressure` | every tick | every tick |
| `derived:io_pressure` | every tick | every tick |
| `derived:thermal_pressure` | every tick | every tick |
| `derived:power_pressure` | every tick | every tick |
| `derived:latency_jitter` | every tick | every tick |

## Risk metrics

Risk metrics are computed every tick after derived metrics.

| Redis key suffix | Published every | Computed every | Meaning |
| --- | --- | --- | --- |
| `risk:realtime_risk` | every tick (`100 ms`) | every tick (`100 ms`) | Probability latency-sensitive workloads miss deadlines. |
| `risk:saturation_risk` | every tick | every tick | Probability throughput collapse is imminent. |
| `risk:state` | every tick | every tick | Encoded enum: `0=STABLE`, `1=DEGRADED`, `2=UNSTABLE`, `3=CRITICAL`. |

## Agent health metrics (optional)

When `agent.publish_health: true`, these additional keys are included in the same `TS.MADD` write:

- `<prefix>:agent:heartbeat`
- `<prefix>:agent:loop_jitter`
- `<prefix>:agent:compute_time`
- `<prefix>:agent:redis_latency`
- `<prefix>:agent:redis_errors`
- `<prefix>:agent:sensor_failures`
- `<prefix>:agent:missed_cycles`

| Redis key suffix | Published every | Value refresh cadence |
| --- | --- | --- |
| `agent:heartbeat` | every tick (`100 ms`) | every tick (set to current wall-clock ms) |
| `agent:loop_jitter` | every tick | every tick |
| `agent:compute_time` | every tick | every tick |
| `agent:redis_latency` | every tick | every tick (measured around Redis publish call) |
| `agent:redis_errors` | every tick | monotonic counter, updated when Redis publish attempts fail |
| `agent:sensor_failures` | every tick | monotonic counter, updated on sensor sample failure events |
| `agent:missed_cycles` | every tick | monotonic counter, updated when compute time exceeds tick budget |

## Important operational detail

`TS.MADD` writes all listed keys every cycle. For metrics sourced by slower sensors, values are held from the last successful sample until the next sensor run.
