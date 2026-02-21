from collections.abc import Mapping
from typing import Any

from hw_agent_tui.runtime.state import DashboardState


def format_dashboard(state: DashboardState) -> str:
    hit_rate = state.hit_rate * 100
    summary = (
        f"Memory: {state.used_memory} B\n"
        f"Clients: {state.connected_clients}\n"
        f"Ops/sec: {state.ops_per_sec:.1f}\n"
        f"Hit rate: {hit_rate:.1f}%"
    )

    metrics = _format_metrics(state.metrics or {})
    if not metrics:
        return summary

    return f"{summary}\n\nAll metrics:\n{metrics}"


def _format_metrics(metrics: Mapping[str, Any]) -> str:
    lines: list[str] = []
    for key in sorted(metrics):
        lines.append(f"{key}: {_stringify(metrics[key])}")
    return "\n".join(lines)


def _stringify(value: Any) -> str:
    if isinstance(value, dict):
        return "{" + ", ".join(f"{k}={_stringify(v)}" for k, v in sorted(value.items())) + "}"
    if isinstance(value, list):
        return "[" + ", ".join(_stringify(v) for v in value) + "]"
    return str(value)
