# Public API

SyLog's public API is intentionally type/policy based.

There is no public `Level` enum, no public `QueueStrategy` enum, and no public
`Rotation::Mode` enum. Built-in behaviors are normal classes with methods, and
user code can define new classes that derive from the appropriate base class.

The umbrella include is:

```cpp
#include <sylog/sylog.hpp>
```

## Severity

Severity is an event classification type. A severity must derive from
`sylog::severity_base` and provide `name()`, `priority()`, `force_sync()`, and
`flush_after()` either directly or through the base defaults.

```cpp
#include <sylog/sylog.hpp>

#include <cstdint>
#include <string_view>

class audit : public sylog::severity_base {
public:
  static constexpr std::string_view name() noexcept { return "AUDIT"; }
  static constexpr std::uint16_t priority() noexcept { return 450; }
};

sylog::Logger log("security.audit");
log.log<audit>("user logged in");
```

Built-ins, ordered by priority:

| Type | Name | Priority | Notes |
| --- | --- | ---: | --- |
| `sylog::severity::trace` | `TRACE` | `100` | lowest built-in severity |
| `sylog::severity::debug` | `DEBUG` | `200` | diagnostic details |
| `sylog::severity::info` | `INFO` | `300` | normal application events |
| `sylog::severity::warn` / `warning` | `WARN` | `400` | warning condition |
| `sylog::severity::error` | `ERROR` | `500` | error condition |
| `sylog::severity::fatal` | `FATAL` | `600` | force-sync and flush-after built in |

## Logger

```cpp
sylog::Logger log("service.http");

log.info("started on port {}", 8080);
log.warn("request queue depth is {}", 128);
log.log<audit>("security-relevant event");
```

Convenience methods exist for the built-in severities:

- `trace()`, `debug()`, `info()`, `warn()`, `error()`, `fatal()`;
- `trace_at()`, `debug_at()`, `info_at()`, `warn_at()`, `error_at()`, `fatal_at()`
  when the caller wants to provide an explicit `std::source_location`.

`Logger::enabled<Severity>()` can be used to avoid preparing expensive arguments
when a severity is disabled for the endpoint.

## Event fields

`Logger::log_fields()` attaches structured fields to an event. The built-in JSON
renderer includes fields by default.

```cpp
#include <array>
#include <cstdint>

std::array fields{
    sylog::Field{"user_id", std::int64_t{42}},
    sylog::Field{"authenticated", true},
};

log.log_fields<sylog::severity::info>(fields, "login accepted");
```

Supported field values are `std::monostate`, `bool`, `std::int64_t`, `double`,
and `std::string`.

## Delivery policy

Delivery is route configuration, not severity metadata.

```cpp
route.drop_when_full();
route.block_when_full();
route.deliver_with<my_delivery_strategy>();
```

Custom delivery policies derive from `sylog::delivery::strategy_base`. Prefer the
built-in route helpers unless the policy needs custom queue behavior.

## Rotation policy

Rotation is a sink behavior object.

```cpp
auto sink = std::make_shared<sylog::RotatingFileSink>(
    "app.log",
    sylog::rotation::when_file_size_exceeds(10 * 1024 * 1024, 5));
```

Custom rotation policies derive from `sylog::rotation::mode_base`.

## Routes

A route combines sink, renderer, filters, and delivery policy.

```cpp
auto console = std::make_shared<sylog::ConsoleSink>();
auto renderer = std::make_shared<sylog::PatternRenderer>(
    "[{severity}] {endpoint}: {message}\n");

sylog::Route route;
route.send_to(console)
     .render_with(renderer)
     .filtered_by(sylog::SeverityFilter::at_least<sylog::severity::info>())
     .block_when_full();

sylog::Pipeline::instance().add_route(std::move(route));
```

If a route has no renderer, SyLog writes the event message and adds a trailing
newline when missing. If a route has no sink, it matches but has no output.

## Filters

Built-in filters are:

- `SeverityFilter::at_least<T>()`, which compares numeric severity priorities;
- `EndpointFilter`, which applies prefix-based deny rules first, then allow rules.

Custom filters derive from `sylog::IFilter` and implement `pass(const Event&)`.

## Sinks and renderers

Built-in sinks are `ConsoleSink` and `RotatingFileSink`. Built-in renderers are
`PatternRenderer`, `AnsiConsoleRenderer`, and `JsonRenderer`. Custom sinks derive
from `ISink`; custom renderers derive from `IRenderer`.

## Pipeline and configuration

`Pipeline` is the process-wide routing singleton.

```cpp
sylog::Pipeline::instance().configure(sylog::Config::asynchronous(16384));
sylog::Pipeline::instance().add_route(std::move(route));
sylog::Pipeline::instance().start();

// application work

sylog::Pipeline::instance().close();
```

Configure routes before `start()` in normal applications. Tests and short-lived
tools may call `close()`, clear/rebuild routes, and start again.

`Config::synchronous()` dispatches matching routes on the caller thread.
`Config::asynchronous(capacity)` uses the Workers-backed crossbar with the given
per-route queue capacity. In `0.1.x`, the optimized dispatch mask is 64 bits, so
only the first 64 configured routes are considered by the dispatcher.
