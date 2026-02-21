# Python Redis TUI Layout (In-Repo)

This document captures the initial in-repo structure for the Redis Textual dashboard under `tools/hw-agent-tui`.

## MVP files

- `src/hw_agent_tui/app.py`
- `src/hw_agent_tui/redis/{client.py,collectors.py,parser.py,sampler.py}`
- `src/hw_agent_tui/metrics/{derived.py,history.py}`
- `src/hw_agent_tui/runtime/{controller.py,state.py}`
- `src/hw_agent_tui/ui/screens/dashboard.py`
- `src/hw_agent_tui/ui/widgets/{stat_tile.py,sparkline.py,status_bar.py}`
- `tests/{test_parser.py,test_derived_metrics.py,test_collectors.py,test_dashboard.py}`

## Layering intent

- **Redis layer (`redis/`)**: read and normalize raw Redis data.
- **Metrics layer (`metrics/`)**: calculate derived values and maintain short histories.
- **Runtime layer (`runtime/`)**: orchestrate refresh loops and app state.
- **UI layer (`ui/`)**: render screens/widgets only; avoid direct Redis coupling.

## Follow-up phases

- Add slowlog, clients, and replication screens.
- Add thresholds and alert summarization.
- Add richer tables and filtering.
- Expand keyboard shortcuts and navigation patterns.
