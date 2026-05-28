#!/usr/bin/env bash
set -euo pipefail

project_root="${1:?project root is required}"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "${tmp_dir}"' EXIT

cat >"${tmp_dir}/server.jsonl" <<'JSON'
{"timestamp_ms":1,"io_threads":2,"active_connections":100,"peak_connections":100,"accepted_total":100,"closed_total":0,"messages_in_total":50,"messages_out_total":50,"bytes_in_total":3200,"bytes_out_total":3200,"high_water_events":0,"output_buffer_peak":0}
JSON

cat >"${tmp_dir}/client.jsonl" <<'JSON'
{"timestamp_ms":2,"connections_target":100,"connections_attempted":100,"connections_established":100,"connections_failed":0,"active_connections":100,"normal_closed":0,"unexpected_closed":0,"disconnect_rate_percent":0,"echo_ok":50,"echo_mismatch":0,"send_errors":0,"recv_errors":0,"bytes_sent":3200,"bytes_received":3200}
{"timestamp_ms":3,"latency_p50_us":1000,"latency_p90_us":2000,"latency_p99_us":3000,"latency_max_us":4000}
JSON

cat >"${tmp_dir}/system.csv" <<'CSV'
timestamp,pid,fd_count,rss_kb,threads,cpu_percent,tcp_inuse,tcp_tw
1,123,130,2048,3,1.0,100,0
CSV

python3 "${project_root}/scripts/report.py" \
  --server "${tmp_dir}/server.jsonl" \
  --client "${tmp_dir}/client.jsonl" \
  --system "${tmp_dir}/system.csv" \
  --cpu-cores 2 \
  --memory-gb 16 \
  --out "${tmp_dir}/report.md"

grep -q "2 核 16G Linux" "${tmp_dir}/report.md"
grep -q "异常断连率 0%" "${tmp_dir}/report.md"
