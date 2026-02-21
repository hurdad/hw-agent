from hw_agent_tui.redis.collectors import collect_hw_metrics, group_hw_metrics
from redis.exceptions import ResponseError


class FakeRedis:
    def __init__(self) -> None:
        self._series = {
            "hw-agent:raw:cpu": [1000, "0.21"],
            "hw-agent:agent:missed_cycles": [1000, "2"],
            "other:key": [1000, "999"],
        }

    def scan_iter(self, match: str):
        assert match == "hw-agent:*"
        return ["hw-agent:raw:cpu", "hw-agent:agent:missed_cycles", "hw-agent:metadata"]

    def execute_command(self, command: str, key: str):
        assert command == "TS.GET"
        if key == "hw-agent:metadata":
            raise ResponseError("WRONGTYPE")
        return self._series[key]


def test_collect_hw_metrics_reads_ts_values() -> None:
    client = FakeRedis()
    metrics = collect_hw_metrics(client)  # type: ignore[arg-type]
    assert metrics == {
        "hw-agent:agent:missed_cycles": 2.0,
        "hw-agent:raw:cpu": 0.21,
    }


def test_group_hw_metrics_groups_by_signal_class() -> None:
    grouped = group_hw_metrics(
        {
            "hw-agent:raw:cpu": 0.21,
            "hw-agent:risk:state": 2.0,
            "hw-agent:agent:missed_cycles": 4.0,
        }
    )
    assert grouped == {
        "agent": {"missed_cycles": 4.0},
        "raw": {"cpu": 0.21},
        "risk": {"state": 2.0},
    }
