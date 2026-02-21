from collections.abc import Mapping
from typing import Any

from hw_agent_tui.metrics.derived import compute_hit_rate, ops_per_sec
from hw_agent_tui.runtime.state import DashboardState


def to_dashboard_state(info: Mapping[str, Any]) -> DashboardState:
    metrics = dict(info)
    return DashboardState(
        used_memory=int(info.get("used_memory", 0)),
        connected_clients=int(info.get("connected_clients", 0)),
        ops_per_sec=ops_per_sec(info),
        hit_rate=compute_hit_rate(info),
        metrics=metrics,
    )
