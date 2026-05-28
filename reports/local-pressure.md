# Pressure Test Report

## Summary

16 核 15G Linux，2 个 I/O 线程，0.5 万 TCP 长连接稳定运行 1 分钟，异常断连率 0%

## Client

- target connections: 5000
- established: 5000
- failed: 0
- unexpected closed: 0
- disconnect rate: 0%
- echo ok: 270523
- echo mismatch: 0
- send errors: 0
- recv errors: 0
- latency p50/p90/p99/max us: 2109/3718/5557/34234

## Server

- peak connections: 5000
- accepted total: 6000
- closed total: 6000
- messages in/out: 329146/329146
- bytes in/out: 21065344/21065344
- high water events: 0
- output buffer peak: 0

## System

- fd count: 5015
- RSS KB: 19236
- threads: 4
- CPU percent: 1.1
- TCP inuse/TIME_WAIT: 10009/0
