#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${project_root}/build}"
tmp="$(mktemp -d)"
server_pid=""
collector_pid=""

cleanup() {
  if [[ -n "${collector_pid}" ]]; then
    wait "${collector_pid}" 2>/dev/null || true
  fi
  if [[ -n "${server_pid}" ]] && kill -0 "${server_pid}" 2>/dev/null; then
    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
  fi
  rm -rf "${tmp}"
}
trap cleanup EXIT

"${build_dir}/echo_server_bench" \
  --host 127.0.0.1 \
  --port 6200 \
  --threads 1 \
  --backlog 128 \
  --quiet \
  --metrics-jsonl "${tmp}/server.jsonl" &
server_pid=$!

sleep 1

bash "${project_root}/scripts/collect_metrics.sh" \
  --pid "${server_pid}" \
  --interval 1 \
  --duration-sec 4 \
  --out "${tmp}/system.csv" &
collector_pid=$!

"${build_dir}/long_conn_client" \
  --host 127.0.0.1 \
  --port 6200 \
  --connections 20 \
  --duration-sec 3 \
  --ramp-rate 20 \
  --payload-bytes 32 \
  --send-interval-ms 500 \
  --verify-echo \
  --metrics-jsonl "${tmp}/client.jsonl"

wait "${collector_pid}"
collector_pid=""

python3 "${project_root}/scripts/report.py" \
  --server "${tmp}/server.jsonl" \
  --client "${tmp}/client.jsonl" \
  --system "${tmp}/system.csv" \
  --cpu-cores "$(nproc)" \
  --memory-gb "$(awk '/MemTotal:/ {printf "%d", ($2 / 1024 / 1024) + 0.5}' /proc/meminfo)" \
  --duration-sec 3 \
  --out "${tmp}/report.md"

grep -q "异常断连率 0%" "${tmp}/report.md"
sed -n '1,35p' "${tmp}/report.md"
