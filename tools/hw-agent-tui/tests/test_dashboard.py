from hw_agent_tui.runtime.state import DashboardState
from hw_agent_tui.ui.screens.dashboard import format_dashboard


def test_format_dashboard_shows_all_metrics() -> None:
    state = DashboardState(
        used_memory=1024,
        connected_clients=3,
        ops_per_sec=42.5,
        hit_rate=0.8,
        metrics={
            "zeta": 1,
            "alpha": "ok",
            "nested": {"b": 2, "a": True},
        },
    )

    rendered = format_dashboard(state)

    assert "Memory: 1024 B" in rendered
    assert "Clients: 3" in rendered
    assert "Ops/sec: 42.5" in rendered
    assert "Hit rate: 80.0%" in rendered
    assert "All metrics:" in rendered
    assert "alpha: ok" in rendered
    assert "nested: {a=True, b=2}" in rendered
    assert "zeta: 1" in rendered

    assert rendered.index("alpha: ok") < rendered.index("nested: {a=True, b=2}")
    assert rendered.index("nested: {a=True, b=2}") < rendered.index("zeta: 1")
