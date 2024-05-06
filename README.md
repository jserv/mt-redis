# mt-redis

A multi-threaded Redis fork with [Read-Copy-Update](https://liburcu.org/) (RCU) support to achieve high performance.

## Features
1. Use an event-loop per thread I/O model.
2. Support for scheduling request operations between threads.
3. RCU can support lock-free sharing between 1 writer thread and multiple reader threads to boost read operation performance.
4. Can achieve over 1 million ops/s powered by a normal commercial server.

## Performance

The following benchmarks show the performance of mt-redis and Redis using the the [memtier_benchmark](https://github.com/RedisLabs/memtier_benchmark) tool.

These were produced on Linux with a Intel Xeon CPU E5-2650 v4 processor.

### Valkey 7.2.5

Launch the server:
```shell
valkey-server --appendonly no --save ""
```

Run benchmark:
```shell
memtier_benchmark --hide-histogram -p 6379
```

Reference results:
```
============================================================================================================================
Type         Ops/sec     Hits/sec   Misses/sec    Avg. Latency     p50 Latency     p99 Latency   p99.9 Latency       KB/sec
----------------------------------------------------------------------------------------------------------------------------
Sets         7773.05          ---          ---         2.34606         2.15900         4.95900         7.29500       598.66
Gets        77645.07         0.00     77645.07         2.34192         2.15900         4.86300         7.39100      3024.61
Waits           0.00          ---          ---             ---             ---             ---             ---          ---
Totals      85418.12         0.00     77645.07         2.34230         2.15900         4.86300         7.39100      3623.27
```

### mt-redis

Launch the server:
```shell
redis-server --appendonly no --save ""
```

Run benchmark:
```shell
memtier_benchmark --hide-histogram -p 6379
```

Reference results:
```
============================================================================================================================
Type         Ops/sec     Hits/sec   Misses/sec    Avg. Latency     p50 Latency     p99 Latency   p99.9 Latency       KB/sec
----------------------------------------------------------------------------------------------------------------------------
Sets        23095.97          ---          ---         0.85914         0.76700         1.59100         3.51900      1778.79
Gets       230705.95         0.00    230705.95         0.80290         0.75100         1.36700         2.70300      8986.99
Waits           0.00          ---          ---             ---             ---             ---             ---          ---
Totals     253801.92         0.00    230705.95         0.80802         0.75100         1.39900         2.81500     10765.79
```
