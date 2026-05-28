# Pressure Test Report

## Summary

16 核 15G Linux，2 个 I/O 线程，1 万 TCP 长连接稳定运行 1 分钟，异常断连率 0%

## Client

- target connections: 10000
- established: 10000
- failed: 0
- unexpected closed: 0
- disconnect rate: 0%
- echo ok: 492215
- echo mismatch: 0
- send errors: 0
- recv errors: 0
- latency p50/p90/p99/max us: 2971/5044/6900/26971

## Server

- peak connections: 10000
- accepted total: 16000
- closed total: 16000
- messages in/out: 821361/821361
- bytes in/out: 52567104/52567104
- high water events: 0
- output buffer peak: 0

## System

- fd count: 10015
- RSS KB: 33764
- threads: 4
- CPU percent: 0.9
- TCP inuse/TIME_WAIT: 20009/0
