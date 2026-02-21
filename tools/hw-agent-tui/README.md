# hw-agent Redis TUI (MVP)

A lightweight `textual` dashboard for Redis operational metrics used by `hw-agent` operators.

## Quickstart

```bash
cd tools/hw-agent-tui
python -m venv .venv
source .venv/bin/activate
pip install -e .[dev]
python -m hw_agent_tui
```

Set `REDIS_URL` in your environment (defaults to `redis://localhost:6379/0`).

## MVP Scope

- Single dashboard screen.
- Summary metrics: memory used, ops/sec, connected clients, hit rate, plus a full sorted dump of all Redis INFO fields.
- Redis INFO parsing and derived metrics tested with fixtures.

## Keybinding

- `q`: quit
- `r`: refresh now
