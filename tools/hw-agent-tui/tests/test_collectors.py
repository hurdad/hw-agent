from hw_agent_tui.redis.collectors import collect_hw_metrics, group_hw_metrics
from redis.exceptions import ResponseError


class FakeRedis:
    def __init__(self) -> None:
        self._series = {
            "edge:node:raw:cpu": [1000, "0.21"],
            "edge:node:agent:missed_cycles": [1000, "2"],
            "edge:node:custom:temperature": [1000, "42"],
            "other:key": [1000, "999"],
        }

    def scan_iter(self, match: str):
        assert match == "edge:node:*"
        return [
            "edge:node:raw:cpu",
            "edge:node:agent:missed_cycles",
            "edge:node:custom:temperature",
            "edge:node:metadata",
        ]

    def execute_command(self, command: str, key: str):
        assert command == "TS.GET"
        if key == "edge:node:metadata":
            raise ResponseError("WRONGTYPE")
        return self._series[key]


def test_collect_hw_metrics_reads_ts_values() -> None:
    client = FakeRedis()
    metrics = collect_hw_metrics(client)  # type: ignore[arg-type]
    assert metrics == {
        "edge:node:agent:missed_cycles": 2.0,
        "edge:node:raw:cpu": 0.21,
    }


def test_group_hw_metrics_groups_by_signal_class() -> None:
    grouped = group_hw_metrics(
        {
            "edge:node:raw:cpu": 0.21,
            "edge:node:risk:state": 2.0,
            "edge:node:agent:missed_cycles": 4.0,
        }
    )
    assert grouped == {
        "agent": {"missed_cycles": 4.0},
        "raw": {"cpu": 0.21},
        "risk": {"state": 2.0},
    }


def test_collect_hw_metrics_accepts_renamed_gpu_metric_keys() -> None:
    class RenamedGpuRedis(FakeRedis):
        def __init__(self) -> None:
            super().__init__()
            self._series.update(
                {
                    "edge:node:raw:nvml_gpu_util": [1000, "77.0"],
                    "edge:node:raw:tegra_emc_util": [1000, "33.0"],
                }
            )

        def scan_iter(self, match: str):
            keys = list(super().scan_iter(match))
            keys.extend(["edge:node:raw:nvml_gpu_util", "edge:node:raw:tegra_emc_util"])
            return keys

    client = RenamedGpuRedis()
    metrics = collect_hw_metrics(client)  # type: ignore[arg-type]
    assert metrics["edge:node:raw:nvml_gpu_util"] == 77.0
    assert metrics["edge:node:raw:tegra_emc_util"] == 33.0
