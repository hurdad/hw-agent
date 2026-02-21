from typing import Any


def parse_info_text(raw: str) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for line in raw.splitlines():
        line = line.strip()
        if not line or line.startswith("#") or ":" not in line:
            continue
        key, value = line.split(":", 1)
        result[key] = _coerce(value)
    return result


def _coerce(value: str) -> Any:
    lower = value.lower()
    if lower in {"yes", "no"}:
        return lower == "yes"
    if value.isdigit() or (value.startswith("-") and value[1:].isdigit()):
        return int(value)
    try:
        return float(value)
    except ValueError:
        return value
