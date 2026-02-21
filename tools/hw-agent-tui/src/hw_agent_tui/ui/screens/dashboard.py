from hw_agent_tui.runtime.state import DashboardState


def format_dashboard(state: DashboardState) -> str:
    hit_rate = state.hit_rate * 100
    return (
        f"Memory: {state.used_memory} B\n"
        f"Clients: {state.connected_clients}\n"
        f"Ops/sec: {state.ops_per_sec:.1f}\n"
        f"Hit rate: {hit_rate:.1f}%"
    )
