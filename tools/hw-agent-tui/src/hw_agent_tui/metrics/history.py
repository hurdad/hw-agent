from collections import deque


class RollingHistory:
    def __init__(self, size: int = 30) -> None:
        self._values: deque[float] = deque(maxlen=size)

    def add(self, value: float) -> None:
        self._values.append(value)

    def values(self) -> list[float]:
        return list(self._values)
