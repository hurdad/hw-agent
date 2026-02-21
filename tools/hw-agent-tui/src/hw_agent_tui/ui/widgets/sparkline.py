def render_sparkline(values: list[float]) -> str:
    if not values:
        return ""
    bars = "▁▂▃▄▅▆▇█"
    low, high = min(values), max(values)
    span = high - low
    if span == 0:
        return bars[0] * len(values)
    out = []
    for value in values:
        idx = int((value - low) / span * (len(bars) - 1))
        out.append(bars[idx])
    return "".join(out)
