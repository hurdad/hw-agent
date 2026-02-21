import asyncio
from collections.abc import Awaitable, Callable
from typing import Any


class Sampler:
    def __init__(
        self,
        interval_s: float,
        collect_fn: Callable[[], Awaitable[dict[str, Any]]],
    ) -> None:
        self._interval_s = interval_s
        self._collect_fn = collect_fn
        self._running = False

    async def run(self, on_sample: Callable[[dict[str, Any]], None]) -> None:
        self._running = True
        while self._running:
            sample = await self._collect_fn()
            on_sample(sample)
            await asyncio.sleep(self._interval_s)

    def stop(self) -> None:
        self._running = False
