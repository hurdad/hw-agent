# hw-agent Redis TUI (MVP)

A lightweight [`textual`](https://textual.textualize.io/) dashboard for viewing Redis operational and `hw-agent` metrics in a terminal UI.

## Quickstart

```bash
cd tools/hw-agent-tui
python -m venv .venv
source .venv/bin/activate
pip install -e .[dev]
python -m hw_agent_tui
```

## Configuration

- `REDIS_URL`: Redis connection string.
  - Default: `redis://localhost:6379/0`
- `HW_AGENT_METRIC_PREFIX`: Prefix used to discover metrics.
  - Default: `edge:node`

Example:

```bash
export REDIS_URL=redis://localhost:6379/0
export HW_AGENT_METRIC_PREFIX=edge:node
python -m hw_agent_tui
```

## MVP Scope

- Single dashboard screen.
- Core Redis metrics: memory used, ops/sec, connected clients, and hit rate.
- `hw-agent` key discovery + grouped display.
- Redis `INFO` parsing and derived metrics tested with fixtures.

## Keybindings

- `q`: Quit.
- `r`: Refresh now.

## Development

Run tests:

```bash
pytest
```
