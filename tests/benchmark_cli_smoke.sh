#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:?build directory is required}"

"${build_dir}/echo_server_bench" --help | grep -q -- "--metrics-jsonl"
"${build_dir}/echo_server_bench" --help | grep -q -- "--high-water-mark"
"${build_dir}/long_conn_client" --help | grep -q -- "--connections"
"${build_dir}/long_conn_client" --help | grep -q -- "--verify-echo"
