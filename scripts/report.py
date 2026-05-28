#!/usr/bin/env python3
import argparse
import csv
import json
import os
from typing import Dict, Any


def load_jsonl(path: str) -> Dict[str, Any]:
    result: Dict[str, Any] = {}
    if not path:
        return result
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            result.update(json.loads(line))
    return result


def load_last_csv(path: str) -> Dict[str, Any]:
    if not path:
        return {}
    with open(path, "r", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    return rows[-1] if rows else {}


def detect_memory_gb() -> int:
    try:
        with open("/proc/meminfo", "r", encoding="utf-8") as handle:
            for line in handle:
                if line.startswith("MemTotal:"):
                    kb = int(line.split()[1])
                    return max(1, round(kb / 1024 / 1024))
    except OSError:
        pass
    return 0


def fmt_number(value: Any) -> str:
    try:
        numeric = float(value)
    except (TypeError, ValueError):
        return "0"
    if numeric.is_integer():
        return str(int(numeric))
    return f"{numeric:.2f}".rstrip("0").rstrip(".")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate a pressure-test Markdown report.")
    parser.add_argument("--server", required=True)
    parser.add_argument("--client", required=True)
    parser.add_argument("--system", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--cpu-cores", type=int, default=os.cpu_count() or 0)
    parser.add_argument("--memory-gb", type=int, default=detect_memory_gb())
    parser.add_argument("--duration-sec", type=int, default=0)
    args = parser.parse_args()

    server = load_jsonl(args.server)
    client = load_jsonl(args.client)
    system = load_last_csv(args.system)

    io_threads = int(server.get("io_threads", 0))
    peak_connections = int(server.get("peak_connections", client.get("connections_established", 0)))
    established = int(client.get("connections_established", 0))
    unexpected = int(client.get("unexpected_closed", 0))
    disconnect_rate = float(client.get("disconnect_rate_percent", 0.0))
    duration_minutes = args.duration_sec / 60.0 if args.duration_sec else 0
    connection_wan = peak_connections / 10000.0

    headline = (
        f"{args.cpu_cores} 核 {args.memory_gb}G Linux，"
        f"{io_threads} 个 I/O 线程，"
        f"{fmt_number(connection_wan)} 万 TCP 长连接稳定运行 "
        f"{fmt_number(duration_minutes)} 分钟，"
        f"异常断连率 {fmt_number(disconnect_rate)}%"
    )

    lines = [
        "# Pressure Test Report",
        "",
        "## Summary",
        "",
        headline,
        "",
        "## Client",
        "",
        f"- target connections: {client.get('connections_target', 0)}",
        f"- established: {established}",
        f"- failed: {client.get('connections_failed', 0)}",
        f"- unexpected closed: {unexpected}",
        f"- disconnect rate: {fmt_number(disconnect_rate)}%",
        f"- echo ok: {client.get('echo_ok', 0)}",
        f"- echo mismatch: {client.get('echo_mismatch', 0)}",
        f"- send errors: {client.get('send_errors', 0)}",
        f"- recv errors: {client.get('recv_errors', 0)}",
        f"- latency p50/p90/p99/max us: "
        f"{client.get('latency_p50_us', 0)}/"
        f"{client.get('latency_p90_us', 0)}/"
        f"{client.get('latency_p99_us', 0)}/"
        f"{client.get('latency_max_us', 0)}",
        "",
        "## Server",
        "",
        f"- peak connections: {peak_connections}",
        f"- accepted total: {server.get('accepted_total', 0)}",
        f"- closed total: {server.get('closed_total', 0)}",
        f"- messages in/out: {server.get('messages_in_total', 0)}/{server.get('messages_out_total', 0)}",
        f"- bytes in/out: {server.get('bytes_in_total', 0)}/{server.get('bytes_out_total', 0)}",
        f"- high water events: {server.get('high_water_events', 0)}",
        f"- output buffer peak: {server.get('output_buffer_peak', 0)}",
        "",
        "## System",
        "",
        f"- fd count: {system.get('fd_count', 0)}",
        f"- RSS KB: {system.get('rss_kb', 0)}",
        f"- threads: {system.get('threads', 0)}",
        f"- CPU percent: {system.get('cpu_percent', 0)}",
        f"- TCP inuse/TIME_WAIT: {system.get('tcp_inuse', 0)}/{system.get('tcp_tw', 0)}",
        "",
    ]

    out_dir = os.path.dirname(args.out)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
