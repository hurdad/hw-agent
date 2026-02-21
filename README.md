# hw-agent

Skeleton repository for a C++20 Linux system agent.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/hw_agent configs/agent.yaml
```

## Configuration

`configs/agent.yaml` controls:
- `tick_rate_hz`
- `sensors.<name>` enable/disable flags
- `redis.address` (`host:port`)
- `thermal_throttle_temp_c`
