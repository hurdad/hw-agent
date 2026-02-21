def status_line(redis_url: str, refresh_s: float) -> str:
    return f"{redis_url} | refresh={refresh_s:.1f}s"
