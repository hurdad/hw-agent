from hw_agent_tui.ui.screens.dashboard import format_dashboard


def test_format_dashboard_empty() -> None:
    assert format_dashboard({}) == "No hw-agent metrics found in Redis."


def test_format_dashboard_sections() -> None:
    output = format_dashboard({"raw": {"cpu": 0.2}, "agent": {"missed_cycles": 3.0}})
    assert "[RAW]" in output
    assert "[AGENT]" in output
    assert "cpu" in output
    assert "missed_cycles" in output
