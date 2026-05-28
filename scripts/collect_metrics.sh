#!/usr/bin/env bash
set -euo pipefail

pid=""
interval=1
duration=0
out="system_metrics.csv"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --pid)
      pid="$2"
      shift 2
      ;;
    --interval)
      interval="$2"
      shift 2
      ;;
    --duration-sec)
      duration="$2"
      shift 2
      ;;
    --out)
      out="$2"
      shift 2
      ;;
    --help|-h)
      cat <<'EOF'
Usage: collect_metrics.sh --pid <pid> [--interval 1] [--duration-sec 60] [--out system_metrics.csv]

Collects fd count, RSS, thread count, CPU percent, and TCP socket counters.
EOF
      exit 0
      ;;
    *)
      echo "unknown option: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "${pid}" ]]; then
  echo "--pid is required" >&2
  exit 2
fi

echo "timestamp,pid,fd_count,rss_kb,threads,cpu_percent,tcp_inuse,tcp_tw" >"${out}"

start_ts="$(date +%s)"
while kill -0 "${pid}" 2>/dev/null; do
  now="$(date +%s)"
  if [[ "${duration}" -gt 0 && $((now - start_ts)) -ge "${duration}" ]]; then
    break
  fi

  fd_count="0"
  if [[ -d "/proc/${pid}/fd" ]]; then
    fd_count="$(find "/proc/${pid}/fd" -maxdepth 1 -type l 2>/dev/null | wc -l)"
  fi

  rss_kb="$(awk '/VmRSS:/ {print $2}' "/proc/${pid}/status" 2>/dev/null || echo 0)"
  threads="$(awk '/Threads:/ {print $2}' "/proc/${pid}/status" 2>/dev/null || echo 0)"
  cpu_percent="$(ps -p "${pid}" -o %cpu= 2>/dev/null | awk '{print $1 + 0}')"

  tcp_inuse="0"
  tcp_tw="0"
  if [[ -r /proc/net/sockstat ]]; then
    tcp_inuse="$(awk '/TCP:/ {for (i=1;i<=NF;i++) if ($i=="inuse") print $(i+1)}' /proc/net/sockstat)"
    tcp_tw="$(awk '/TCP:/ {for (i=1;i<=NF;i++) if ($i=="tw") print $(i+1)}' /proc/net/sockstat)"
  fi

  echo "${now},${pid},${fd_count},${rss_kb:-0},${threads:-0},${cpu_percent:-0},${tcp_inuse:-0},${tcp_tw:-0}" >>"${out}"
  sleep "${interval}"
done
