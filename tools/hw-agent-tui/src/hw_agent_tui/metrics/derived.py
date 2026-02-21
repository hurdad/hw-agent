from collections.abc import Mapping


def compute_hit_rate(metrics: Mapping[str, float | int]) -> float:
    hits = float(metrics.get("keyspace_hits", 0))
    misses = float(metrics.get("keyspace_misses", 0))
    total = hits + misses
    if total <= 0:
        return 0.0
    return hits / total


def ops_per_sec(metrics: Mapping[str, float | int]) -> float:
    return float(metrics.get("instantaneous_ops_per_sec", 0.0))
