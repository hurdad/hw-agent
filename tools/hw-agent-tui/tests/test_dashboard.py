from hw_agent_tui.ui.screens.dashboard import format_dashboard


def test_format_dashboard_empty() -> None:
    assert format_dashboard({}) == "No hw-agent metrics found in Redis."


def test_format_dashboard_sections() -> None:
    output = format_dashboard({"raw": {"cpu": 0.2}, "agent": {"missed_cycles": 3.0}})
    assert "[RAW]" in output
    assert "[AGENT]" in output
    assert "cpu" in output
    assert "missed_cycles" in output
    assert "ratio" in output
    assert "count" in output


def test_format_dashboard_unknown_metric_uses_unitless_marker() -> None:
    output = format_dashboard({"other": {"mystery_metric": 1.0}})
    assert "mystery_metric" in output
    assert "-" in output


def test_format_dashboard_renamed_gpu_metrics_units() -> None:
    output = format_dashboard(
        {
            "raw": {
                "nvml_gpu_util": 82.0,
                "tegra_emc_util": 49.0,
                "nvml_gpu_temp": 70.0,
                "tegra_gpu_power_mw": 12400.0,
            }
        }
    )
    assert "nvml_gpu_util" in output and "ratio" in output
    assert "tegra_emc_util" in output and "ratio" in output
    assert "nvml_gpu_temp" in output and "°C" in output
    assert "tegra_gpu_power_mw" in output and "mW" in output
