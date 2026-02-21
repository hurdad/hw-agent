from collections.abc import Mapping


UNITLESS = "-"

SECTION_SIGNAL_UNITS: dict[str, dict[str, str]] = {
    "raw": {
        "gpu_mem_free": "MiB",
        "gpu_temp": "°C",
    },
    "agent": {
        "heartbeat": "ms",
        "loop_jitter": "ms",
        "compute_time": "ms",
        "redis_latency": "ms",
        "redis_errors": "count",
        "sensor_failures": "count",
        "missed_cycles": "count",
    },
    "risk": {
        "state": "enum",
    },
}


def metric_unit(section: str, signal: str) -> str:
    if signal in {"cpu", "memory", "disk", "network", "thermal", "power", "cpufreq", "psi", "psi_memory", "psi_io", "gpu_util", "gpu_mem_util", "emc_util", "gpu_clock_ratio", "gpu_power_ratio", "gpu_throttle", "scheduler_pressure", "memory_pressure", "io_pressure", "thermal_pressure", "power_pressure", "latency_jitter", "realtime_risk", "saturation_risk"}:
        return "ratio"

    return SECTION_SIGNAL_UNITS.get(section, {}).get(signal, UNITLESS)


def format_dashboard(grouped_metrics: Mapping[str, Mapping[str, float]]) -> str:
    if not grouped_metrics:
        return "No hw-agent metrics found in Redis."

    lines: list[str] = []
    for section, section_metrics in grouped_metrics.items():
        lines.append(f"[{section.upper()}]")
        for signal, value in section_metrics.items():
            unit = metric_unit(section, signal)
            lines.append(f"{signal:<24} {value:>12.4f} {unit:<6}")
        lines.append("")

    return "\n".join(lines).rstrip()
