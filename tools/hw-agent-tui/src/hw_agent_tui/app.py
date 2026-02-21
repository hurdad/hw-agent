import asyncio
import os

from textual.app import App, ComposeResult
from textual.containers import Container
from textual.widgets import Footer, Header, Static

from hw_agent_tui.redis.client import make_client
from hw_agent_tui.redis.collectors import collect_info
from hw_agent_tui.runtime.controller import to_dashboard_state
from hw_agent_tui.ui.screens.dashboard import format_dashboard


class DashboardApp(App[None]):
    BINDINGS = [("q", "quit", "Quit"), ("r", "refresh", "Refresh")]

    def compose(self) -> ComposeResult:
        yield Header()
        with Container():
            yield Static("Loading...", id="dashboard")
        yield Footer()

    async def on_mount(self) -> None:
        self.set_interval(float(os.getenv("REFRESH_INTERVAL", "1.0")), self.refresh_data)
        await self.refresh_data()

    async def action_refresh(self) -> None:
        await self.refresh_data()

    async def refresh_data(self) -> None:
        info = await asyncio.to_thread(self._collect)
        state = to_dashboard_state(info)
        self.query_one("#dashboard", Static).update(format_dashboard(state))

    def _collect(self) -> dict[str, int | float]:
        client = make_client()
        return collect_info(client)


def run() -> None:
    DashboardApp().run()
