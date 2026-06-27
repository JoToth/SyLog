# SyLog

SyLog is a C++20 logging/event routing library built around open policy classes instead of closed enums.

Core ideas:

- `severity::*` describes what an event means.
- `delivery::*` describes how a route behaves under queue pressure.
- `rotation::*` describes when a file sink rotates.
- Workers provides the async queue/worker runtime.
- Crossbar routing gives each sink route its own worker lane.

SyLog is licensed under BSD-3-Clause.

## Quick start

```cpp
#include <sylog/sylog.hpp>

int main() {
  using namespace sylog;

  Pipeline::instance().configure(Config::asynchronous(16384));

  Route console;
  console.send_to(std::make_shared<ConsoleSink>())
         .render_with(std::make_shared<PatternRenderer>("{time} {severity} {message}\n"))
         .filtered_by(SeverityFilter::at_least<severity::info>())
         .block_when_full();

  Pipeline::instance().add_route(console);
  Pipeline::instance().start();

  Logger log("app.main");
  log.info("hello {}", "world");
  log.warn("disk is almost full");

  Pipeline::instance().close();
}
```

## Custom severity

```cpp
class audit : public sylog::severity_base {
public:
  static constexpr std::string_view name() noexcept { return "AUDIT"; }
  static constexpr std::uint16_t priority() noexcept { return 450; }
};

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

## Examples

```sh
cmake -S . -B build -DSYLOG_BUILD_EXAMPLES=ON
cmake --build build --target SyLogExampleBasic
cmake --build build --target SyLogExampleRouting
```

Example programs:

- `examples/basic.cpp` — synchronous console route
- `examples/async_workers.cpp` — Workers-backed async route
- `examples/custom_severity.cpp` — user-defined severity
- `examples/routing.cpp` — console and rotating file routes

## Benchmarks

```sh
cmake -S . -B build -DSYLOG_BUILD_BENCHMARKS=ON
cmake --build build --target SyLogBenchmarks
./build/benchmarks/SyLogBenchmarks --scenario all --events 100000 --producers 4
```

## Build

```sh
cmake -S . -B build -DSYLOG_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Testing boundary

SyLog tests verify SyLog behavior: routing, rendering, filters, sink isolation, delivery policy integration, lifecycle semantics, file rotation, examples, and benchmarks.

SyLog does not re-test the Workers library internals. Queue correctness, worker scheduling, and stop-token mechanics are owned by the Workers project and are treated here as dependency contracts.
