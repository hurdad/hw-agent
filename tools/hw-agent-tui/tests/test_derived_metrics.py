from hw_agent_tui.metrics.derived import compute_hit_rate, ops_per_sec


def test_compute_hit_rate() -> None:
    rate = compute_hit_rate({"keyspace_hits": 90, "keyspace_misses": 10})
    assert rate == 0.9


def test_compute_hit_rate_zero_total() -> None:
    rate = compute_hit_rate({"keyspace_hits": 0, "keyspace_misses": 0})
    assert rate == 0.0


def test_ops_per_sec() -> None:
    value = ops_per_sec({"instantaneous_ops_per_sec": 42})
    assert value == 42.0
