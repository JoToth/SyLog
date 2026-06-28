# SyLog

SyLog is a C++20 logging/event routing library built around open policy classes
instead of closed enums.

Core ideas:

- `severity::*` describes what an event means.
- `delivery::*` describes how a route behaves under queue pressure.
- `rotation::*` describes when a file sink rotates.
- Workers provides the async queue/worker runtime.
- Crossbar routing gives each sink route its own worker lane.

SyLog is licensed under BSD-3-Clause.

## Status

SyLog is currently `0.1.0` pre-release software. The public API is already
structured around extension points (`severity_base`, `delivery::strategy_base`,
`rotation::mode_base`, `IFilter`, `IRenderer`, and `ISink`), but source and ABI
stability are not guaranteed before a tagged 1.0 release. The current async route
mask is 64 bits, so `0.1.x` dispatches only the first 64 configured routes.

## Requirements

- CMake 3.21 or newer.
- A C++20 compiler with `<format>` support.
- Git access during configure, unless dependencies are already available through
  the configured `cppdeps` cache.

Dependencies are declared in `deps.cmake` and resolved during configure:

- Workers (`Workers::Workers`) for async queues and worker lanes.
- cpptest (`cpptest::cpptest`) for tests.
- argtree (`argtree::argtree`) for benchmark/tool command-line parsing.

The installed CMake package exposes the library target and requires
`Workers::Workers`. cpptest and argtree are project-development dependencies;
applications using the installed library do not include their headers through
SyLog's public API.

## Quick start

```cpp
#include <sylog/sylog.hpp>

#include <memory>
#include <utility>

int main() {
  using namespace sylog;

  Pipeline::instance().configure(Config::asynchronous(16384));

  Route console;
  console.send_to(std::make_shared<ConsoleSink>())
         .render_with(std::make_shared<PatternRenderer>("{time} {severity} {message}\n"))
         .filtered_by(SeverityFilter::at_least<severity::info>())
         .block_when_full();

  Pipeline::instance().add_route(std::move(console));
  Pipeline::instance().start();

  Logger log("app.main");
  log.info("hello {}", "world");
  log.warn("disk is almost full");

  Pipeline::instance().close();
}
```

## Custom severity

Severity is open and type-based. Custom severities derive from
`sylog::severity_base`; built-in severities are intentionally normal classes, not
closed enum values.

```cpp
#include <sylog/sylog.hpp>

#include <cstdint>
#include <string_view>

class audit : public sylog::severity_base {
public:
  static constexpr std::string_view name() noexcept { return "AUDIT"; }
  static constexpr std::uint16_t priority() noexcept { return 450; }
};

int user_id = 42;
sylog::Logger log("security");
log.log<audit>("user {} changed permissions", user_id);
```

## Natural-language route configuration

```cpp
sylog::Route file;
file.send_to(std::make_shared<sylog::RotatingFileSink>(
      "app.log",
      sylog::rotation::when_file_size_exceeds_or_new_day_begins(10 * 1024 * 1024, 5)))
    .render_with(std::make_shared<sylog::JsonRenderer>())
    .filtered_by(sylog::SeverityFilter::at_least<sylog::severity::debug>())
    .drop_when_full();
```

## Build

```sh
cmake -S . -B build -DSYLOG_BUILD_TESTS=ON -DSYLOG_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Useful CMake options:

| Option | Default | Meaning |
| --- | --- | --- |
| `SYLOG_BUILD_TESTS` | `ON` | Build unit/integration tests. |
| `SYLOG_BUILD_EXAMPLES` | `ON` | Build example programs. |
| `SYLOG_BUILD_BENCHMARKS` | `ON` | Build benchmark executable. |
| `SYLOG_BUILD_TOOLS` | `OFF` | Build command-line tools. |
| `SYLOG_BUILD_SHARED` | `OFF` | Build `SyLog` as a shared library instead of static. |
| `SYLOG_WARNINGS_AS_ERRORS` | `OFF` | Treat compiler warnings as errors. |
| `SYLOG_ENABLE_ASAN` / `SYLOG_ENABLE_UBSAN` / `SYLOG_ENABLE_TSAN` | `OFF` | Enable sanitizer builds on supported non-MSVC toolchains. |

CMake presets are also available:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

## Examples

```sh
cmake -S . -B build -DSYLOG_BUILD_EXAMPLES=ON
cmake --build build --target SyLogExampleBasic
cmake --build build --target SyLogExampleRouting
```

Example programs:

- `examples/basic.cpp` — synchronous console route.
- `examples/async_workers.cpp` — Workers-backed async route.
- `examples/custom_severity.cpp` — user-defined severity.
- `examples/routing.cpp` — console and rotating file routes.

## Benchmarks

```sh
cmake -S . -B build -DSYLOG_BUILD_BENCHMARKS=ON
cmake --build build --target SyLogBenchmarks
./build/benchmarks/SyLogBenchmarks --scenario all --events 100000 --producers 4
```

Benchmark output is diagnostic. Do not treat one global `events/sec` value as a
portable product claim; renderer cost, sink cost, producer count, queue capacity,
and delivery policy all change the result.

## Documentation

Start here:

- `docs/getting_started.md` — first route, async delivery, file sink, structured fields, and custom severity.
- `docs/architecture.md` — runtime layers, route cache, crossbar lanes, lifecycle, and dependency boundary.
- `docs/severity.md` — type-based severity model and built-in priorities.

Public API notes:

- `docs/api/public_api.md` — public type/policy API summary.
- `docs/api/lifecycle.md` — `start()`, `close()`, `request_stop()`, and `flush()` semantics.
- `docs/api/routing.md` — route matching, filters, fallback behavior, and per-route delivery policy.
- `docs/api/renderers_filters.md` — renderer and filter extension points.
- `docs/api/sinks.md` — built-in sinks, rotation policies, and sink extension point.
- `docs/api/queue.md` — async delivery and backpressure behavior.
- `docs/api/examples.md` — example target index.
- `docs/benchmarks/scenarios.md` — benchmark CLI and metric interpretation.

Internal design notes live under `docs/internals/`. They describe the current
implementation and dependency boundaries, not stable application-facing APIs.

## Testing boundary

SyLog tests verify SyLog behavior: routing, rendering, filters, sink isolation,
delivery policy integration, lifecycle semantics, file rotation, examples, and
benchmarks.

SyLog does not re-test the Workers library internals. Queue correctness, worker
scheduling, and stop-token mechanics are owned by the Workers project and are
treated here as dependency contracts.
