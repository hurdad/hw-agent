from typing import Any

from redis import Redis


def collect_info(client: Redis) -> dict[str, Any]:
    return client.info()
