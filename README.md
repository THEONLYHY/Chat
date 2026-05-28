# Chat

muduo-style TCP server core implemented with `EventLoop`, `Channel`, `Poller`,
`Acceptor`, `TcpServer`, `TcpConnection`, and `EventLoopThreadPool`.

## Build

Run commands from the project root:

```bash
cmake -S . -B build
cmake --build build -j 4
```

The build creates:

- `lib/libchat.so`: shared library
- `build/echo_server`: minimal echo server example
- `build/echo_server_bench`: configurable echo server for pressure tests
- `build/long_conn_client`: epoll-based long-connection pressure client

## Run Echo Server

Start the echo server on port `6000`:

```bash
./build/echo_server 6000
```

The example binds to `127.0.0.1` and uses `TcpServer::setThreadNum(2)`.

## Smoke Test

In another terminal, send one message and verify the same payload is echoed:

```bash
printf 'hello muduo\n' | nc -w 2 127.0.0.1 6000
```

Expected output:

```text
hello muduo
```

Run two clients concurrently:

```bash
bash -lc "(printf 'client-a\n' | nc -w 2 127.0.0.1 6000) & (printf 'client-b\n' | nc -w 2 127.0.0.1 6000) & wait"
```

Expected output includes both echoed messages:

```text
client-a
client-b
```

Order may vary because the clients run concurrently.

Stop the server with `Ctrl-C`.

## OpenSpec Change

This core server work is tracked by OpenSpec change `muduo-server-core`.

Check status and validate:

```bash
openspec instructions apply --change muduo-server-core --json
openspec validate muduo-server-core
```

After reviewing the implementation, archive the completed change:

```bash
openspec archive muduo-server-core
```

## Notes

- `muduo_server.cc` is reference/demo code and is not compiled into `chat`.
- `examples/echo_server.cc` uses the current global namespace API; there is no
  `muduo::net` namespace migration in this milestone.

## Pressure Test Workflow

This development profile targets repeatable local validation first, then larger
Linux hosts. The current WSL machine is suitable for smoke tests only; record the
target machine's CPU, memory, `ulimit -n`, and TCP sysctls before quoting larger
connection counts.

Check the environment:

```bash
ulimit -n
nproc
free -g
cat /proc/sys/net/core/somaxconn
cat /proc/sys/net/ipv4/ip_local_port_range
cat /proc/sys/net/ipv4/tcp_max_syn_backlog
```

Start a quiet benchmark server:

```bash
./build/echo_server_bench \
  --host 127.0.0.1 \
  --port 6000 \
  --threads 2 \
  --backlog 4096 \
  --tcp-no-delay \
  --quiet \
  --metrics-jsonl server_metrics.jsonl
```

Run a small local client smoke:

```bash
./build/long_conn_client \
  --host 127.0.0.1 \
  --port 6000 \
  --connections 1000 \
  --duration-sec 60 \
  --ramp-rate 500 \
  --payload-bytes 64 \
  --send-interval-ms 1000 \
  --verify-echo \
  --metrics-jsonl client_metrics.jsonl
```

Collect process/system metrics while the server is running:

```bash
bash scripts/collect_metrics.sh \
  --pid "$(pidof echo_server_bench)" \
  --interval 1 \
  --duration-sec 60 \
  --out system_metrics.csv
```

Generate a report:

```bash
python3 scripts/report.py \
  --server server_metrics.jsonl \
  --client client_metrics.jsonl \
  --system system_metrics.csv \
  --duration-sec 60 \
  --out reports/local-pressure.md
```

Scale in steps: `1 -> 100 -> 1000 -> 5000 -> 10000 -> 20000` connections.
For each level, record connection count, run duration, abnormal disconnect
rate, CPU, RSS, fd count, throughput, and error categories. Single-machine
loopback tests may hit ephemeral-port limits; use multiple client processes,
source IPs, or client machines for larger runs.
