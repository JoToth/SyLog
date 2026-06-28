# Getting started

This guide shows the public, pre-1.0 SyLog API. SyLog uses type/policy classes
instead of closed enum configuration.

## 1. Add one console route

```cpp
#include <sylog/sylog.hpp>

#include <memory>
#include <utility>

int main() {
  namespace sl = sylog;

  sl::Pipeline::instance().configure(sl::Config::synchronous());

  sl::Route route;
  route
      .send_to(std::make_shared<sl::ConsoleSink>())
      .render_with(std::make_shared<sl::PatternRenderer>("[{severity}] {endpoint}: {message}\n"))
      .filtered_by(sl::SeverityFilter::at_least<sl::severity::info>())
      .block_when_full();

  sl::Pipeline::instance().add_route(std::move(route));
  sl::Pipeline::instance().start();

  sl::Logger log("app.main");
  log.info("application started");
  log.warn("configuration file {} was not found", "local.toml");

  sl::Pipeline::instance().close();
}
```

`Pipeline` is a singleton. In tests and short-lived tools, call `close()` before
reconfiguring it for another scenario. Normal applications should configure
routes before `start()` and call `close()` during shutdown.

## 2. Use async delivery

Async mode uses the Workers backend. `close()` drains queued events.
`request_stop()` wakes blocked producers/workers and stops quickly; it is not a
normal graceful shutdown path. The queue capacity below is per route lane, not a
single global queue.

```cpp
sl::Pipeline::instance().configure(sl::Config::asynchronous(16384));
```

Choose route backpressure explicitly:

```cpp
route.block_when_full(); // preserve events; producers may wait
route.drop_when_full();  // preserve producer latency; events may be dropped
```

## 3. Add a file route

```cpp
auto file = std::make_shared<sl::RotatingFileSink>(
    "app.log",
    sl::rotation::when_file_size_exceeds(10 * 1024 * 1024, 5));

sl::Route file_route;
file_route
    .send_to(file)
    .render_with(std::make_shared<sl::JsonRenderer>())
    .filtered_by(sl::SeverityFilter::at_least<sl::severity::debug>())
    .block_when_full();

sl::Pipeline::instance().add_route(std::move(file_route));
```

`RotatingFileSink` appends to the active file and keeps numbered archives such as
`app.log.1`, `app.log.2`, and so on according to the selected rotation policy.
The current built-in sinks are console and rotating file sinks; platform sinks
such as syslog or Windows Event Log are not part of `0.1.0`.

## 4. Define a custom severity

Severity is open. A custom severity is a normal class derived from
`severity_base`.

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
sylog::Logger log("security.audit");
log.log<audit>("user {} changed permissions", user_id);
```

Priority is numeric and monotonic: higher values are more severe. For example,
`SeverityFilter::at_least<sylog::severity::warn>()` accepts `WARN`, `ERROR`,
`FATAL`, and custom severities whose priority is at least `400`.

## 5. Add structured fields

`Logger::log_fields()` attaches key/value fields to the event. The built-in JSON
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

## 6. Build examples

```sh
cmake -S . -B build -DSYLOG_BUILD_EXAMPLES=ON
cmake --build build --target SyLogExampleBasic
cmake --build build --target SyLogExampleAsyncWorkers
cmake --build build --target SyLogExampleCustomSeverity
cmake --build build --target SyLogExampleRouting
```
