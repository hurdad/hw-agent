from dataclasses import dataclass
from typing import Any


@dataclass
class DashboardState:
    used_memory: int = 0
    connected_clients: int = 0
    ops_per_sec: float = 0.0
    hit_rate: float = 0.0
    metrics: dict[str, Any] | None = None
