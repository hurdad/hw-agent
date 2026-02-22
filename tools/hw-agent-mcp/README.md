# hw-agent-mcp

`hw-agent-mcp` is a standalone MCP (Model Context Protocol) sidecar server for `hw-agent`.
It speaks JSON-RPC 2.0 over standard input/output, so MCP clients can launch it as a separate
process and exchange one JSON request per line.

## What MCP is

Model Context Protocol (MCP) is a protocol for exposing tools and resources to LLM-based clients.
In this service:

- `tools/list` advertises available tools.
- `tools/call` executes a tool.
- `resources/list` advertises readable resources.
- `resources/read` returns resource content.

## Build

From repository root:

```bash
mkdir -p build
cd build
cmake ..
make -j
```

The binary will be built as:

```bash
./tools/hw-agent-mcp/hw-agent-mcp
```

(or under your generator-specific output directory).

## Run

You can run it directly and send newline-delimited JSON-RPC messages:

```bash
./build/tools/hw-agent-mcp/hw-agent-mcp
```

## Example JSON-RPC requests

Initialize:

```json
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}
```

List tools:

```json
{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}
```

Call `metrics.summary` (reads RedisTimeSeries via hiredis):

```json
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"metrics.summary","arguments":{"filters":["cpu","memory"],"window":"5m"}}}
```

Call `metrics.timeseries.query` for explicit keys:

```json
{"jsonrpc":"2.0","id":31,"method":"tools/call","params":{"name":"metrics.timeseries.query","arguments":{"keys":["edge:node:raw:cpu","edge:node:risk:realtime_risk"],"window":"30s"}}}
```

List resources:

```json
{"jsonrpc":"2.0","id":4,"method":"resources/list","params":{}}
```

Read a resource:

```json
{"jsonrpc":"2.0","id":5,"method":"resources/read","params":{"uri":"redis://metrics/catalog"}}
```

Notifications are supported by omitting `id`; the server returns no response for notifications.

## Redis connection configuration

`hw-agent-mcp` reads Redis connection settings from environment variables:

- `HW_AGENT_REDIS_HOST` (default `127.0.0.1`)
- `HW_AGENT_REDIS_PORT` (default `6379`)
- `HW_AGENT_REDIS_UNIX_SOCKET` (default empty)
- `HW_AGENT_REDIS_PASSWORD` (default empty)
- `HW_AGENT_REDIS_DB` (default `0`)
- `HW_AGENT_REDIS_PREFIX` (default `edge:node`)
- `HW_AGENT_REDIS_CONNECT_TIMEOUT_MS` (default `1000`)
