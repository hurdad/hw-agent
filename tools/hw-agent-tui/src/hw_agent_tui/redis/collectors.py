from collections.abc import Mapping
from typing import Any

from redis import Redis
from redis.exceptions import ResponseError


def collect_info(client: Redis) -> dict[str, Any]:
    return client.info()


def collect_hw_metrics(client: Redis, prefix: str = "edge:node") -> dict[str, float]:
    metrics: dict[str, float] = {}
    keys = sorted(client.scan_iter(match=f"{prefix}:*"))

    for key in keys:
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
