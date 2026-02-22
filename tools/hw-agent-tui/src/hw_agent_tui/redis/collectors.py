from collections.abc import Mapping
from typing import Any

from redis import Redis
from redis.exceptions import ResponseError


HW_AGENT_METRIC_SUFFIXES = {
    "raw:psi",
    "raw:psi_memory",
    "raw:psi_io",
    "raw:cpu",
    "raw:irq",
    "raw:softirqs",
    "raw:memory",
    "raw:thermal",
    "raw:cpufreq",
    "raw:cpu_throttle_ratio",
    "raw:disk",
    "raw:network",
    "raw:nvml_gpu_util",
    "raw:tegra_gpu_util",
    "raw:emc_util",
    "raw:tegra_emc_util",
    "raw:nvml_gpu_temp",
    "raw:tegra_gpu_temp",
    "raw:nvml_gpu_power_ratio",
    "raw:tegra_gpu_power_mw",
    "derived:scheduler_pressure",
    "derived:memory_pressure",
    "derived:io_pressure",
    "derived:thermal_pressure",
    "derived:power_pressure",
    "derived:latency_jitter",
    "risk:realtime_risk",
    "risk:saturation_risk",
    "risk:state",
    "agent:heartbeat",
    "agent:loop_jitter",
    "agent:compute_time",
    "agent:redis_latency",
    "agent:redis_errors",
    "agent:sensor_failures",
    "agent:missed_cycles",
}


def _is_hw_agent_metric_key(key: str, prefix: str) -> bool:
    prefix_with_sep = f"{prefix}:"
    if not key.startswith(prefix_with_sep):
        return False

    suffix = key[len(prefix_with_sep) :]
    return suffix in HW_AGENT_METRIC_SUFFIXES


def collect_info(client: Redis) -> dict[str, Any]:
    return client.info()


def collect_hw_metrics(client: Redis, prefix: str = "edge:node") -> dict[str, float]:
    metrics: dict[str, float] = {}
    keys = sorted(client.scan_iter(match=f"{prefix}:*"))

    for key in keys:
        if not _is_hw_agent_metric_key(key, prefix):
            continue

        try:
            entry = client.execute_command("TS.GET", key)
        except ResponseError:
            continue

        if not isinstance(entry, list | tuple) or len(entry) != 2:
            continue

        _, value = entry
        try:
            metrics[key] = float(value)
        except (TypeError, ValueError):
            continue

    return metrics


def group_hw_metrics(metrics: Mapping[str, float], prefix: str = "edge:node") -> dict[str, dict[str, float]]:
    grouped: dict[str, dict[str, float]] = {}
    prefix_with_sep = f"{prefix}:"

    for key, value in sorted(metrics.items()):
        metric_name = key
        if key.startswith(prefix_with_sep):
            metric_name = key[len(prefix_with_sep) :]

        section, _, signal = metric_name.partition(":")
        if not signal:
            section = "other"
            signal = metric_name

        grouped.setdefault(section, {})[signal] = value

    return grouped
