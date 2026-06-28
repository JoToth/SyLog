# SyLog benchmark scenarios

`SyLogBenchmarks` is a diagnostic tool. It is scenario-based because one global
`events/sec` number is misleading for an async logger. Observed performance
depends on producer contention, route count, renderer cost, sink cost, queue
capacity, and drop/block policy.

## CLI

The benchmark CLI is implemented with `argtree`.

```sh
./build/benchmarks/SyLogBenchmarks -h
./build/benchmarks/SyLogBenchmarks --scenario all --events 100000 --producers 4 --capacity 16384
./build/benchmarks/SyLogBenchmarks --scenario slow --events 100000 --producers 8 --capacity 1024 --slow-us 100
./build/benchmarks/SyLogBenchmarks --scenario file --file bench.log
```

Options:

| Option | Default | Meaning |
| --- | ---: | --- |
| `--scenario` | `all` | `all`, `null`, `null-format`, `file`, `slow`, or `burst`. |
| `--events` | `100000` | Total logger calls per scenario. |
| `--producers` | `4` | Producer thread count. |
| `--capacity` | `16384` | Per-route async queue capacity. |
| `--slow-us` | `100` | Artificial delay per write for the slow sink scenario. |
| `--file` | `sylog_bench.log` | Output path for the file scenario. |

## Scenarios

### `null`

Measures producer/enqueue overhead with no renderer and a counting sink. This is
closest to the cost of getting events into the async runtime.

### `null-format`

Uses a full pattern renderer and a counting sink. The sink is cheap, so the
difference from `null` mostly reflects formatting/rendering cost.

### `file`

Uses a file sink and message renderer. This captures file write and flush/drain
behavior on the current filesystem.

### `slow`

Uses a deliberately slow sink with drop policy. This shows how much the route
sheds under pressure and what producer-side latency looks like when the system
chooses loss instead of blocking.

### `burst`

Submits a burst and then measures graceful drain time with block policy.

## Metrics

| Metric | Meaning |
| --- | --- |
| `produced` | Number of logger calls attempted by producers. |
| `consumed` | Number of sink writes observed after `Pipeline::close()`. |
| `dropped_or_filtered` | `produced - consumed`; in drop scenarios this approximates backpressure loss. |
| `enqueue_seconds` | Time spent by producers issuing log calls. |
| `total_seconds` | Producer time plus close/drain time. |
| `drain_seconds` | Time spent in `Pipeline::close()` after producers finish. |
| `producer_call_rate_per_sec` | Producer-side submission rate. |
| `end_to_end_consumed_per_sec` | Sink-observed rate over total scenario time. |
| `enqueue_latency_ns p50/p95/p99` | Sampled producer-side call latency. |

## Reading results

Interpret results as workload diagnostics, not absolute product claims. For
release notes, record the full command line, compiler, build type, CPU, OS, and
filesystem. Compare scenario-to-scenario trends from the same machine before
making performance claims.
