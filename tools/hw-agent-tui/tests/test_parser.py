from pathlib import Path

from hw_agent_tui.redis.parser import parse_info_text


FIXTURES = Path(__file__).parent / "fixtures"


def test_parse_info_default_fixture() -> None:
    parsed = parse_info_text((FIXTURES / "info_default.txt").read_text())
    assert parsed["instantaneous_ops_per_sec"] == 112
    assert parsed["keyspace_hits"] == 900
    assert parsed["connected_clients"] == 31
    assert parsed["used_memory"] == 2097152


def test_parse_info_replica_fixture() -> None:
    parsed = parse_info_text((FIXTURES / "info_replica.txt").read_text())
    assert parsed["role"] == "slave"
    assert parsed["keyspace_misses"] == 5
