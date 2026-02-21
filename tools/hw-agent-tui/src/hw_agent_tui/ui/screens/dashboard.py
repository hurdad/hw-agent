from collections.abc import Mapping


def format_dashboard(grouped_metrics: Mapping[str, Mapping[str, float]]) -> str:
    if not grouped_metrics:
        return "No hw-agent metrics found in Redis."

    lines: list[str] = []
    for section, section_metrics in grouped_metrics.items():
        lines.append(f"[{section.upper()}]")
        for signal, value in section_metrics.items():
            lines.append(f"{signal:<24} {value:>12.4f}")
        lines.append("")

    return "\n".join(lines).rstrip()
