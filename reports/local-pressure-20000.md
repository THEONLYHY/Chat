# Pressure Test Report

## Summary

16 核 15G Linux，2 个 I/O 线程，2 万 TCP 长连接稳定运行 30 分钟，异常断连率 0%

## Client

- target connections: 20000
- established: 20000
- failed: 0
- unexpected closed: 0
- disconnect rate: 0%
- echo ok: 34728871
- echo mismatch: 0
- send errors: 0
- recv errors: 0
- latency p50/p90/p99/max us: 73178/132547/152720/348150

## Server

- peak connections: 20000
- accepted total: 56000
- closed total: 56000
- messages in/out: 36345600/36345600
- bytes in/out: 2326118400/2326118400
- high water events: 0
- output buffer peak: 0

## System

- fd count: 12879
- RSS KB: 63076
- threads: 4
- CPU percent: 0.5
- TCP inuse/TIME_WAIT: 25737/0
