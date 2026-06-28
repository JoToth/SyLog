# Sinks

Sinks receive rendered bytes. A sink derives from `sylog::ISink` and implements
`write()` and `flush()`. The built-in sinks in `0.1.0` are console and rotating
file sinks. Platform-specific sinks such as syslog or Windows Event Log are
future extension points, not current built-ins.

## Console

```cpp
#include <sylog/sylog.hpp>

#include <memory>

auto stdout_console = std::make_shared<sylog::ConsoleSink>();
auto stderr_console = std::make_shared<sylog::ConsoleSink>(true);
```

## Rotating file

```cpp
auto file = std::make_shared<sylog::RotatingFileSink>(
    "app.log",
    sylog::rotation::when_file_size_exceeds(10 * 1024 * 1024, 5));
```

`RotatingFileSink` opens the active file in append mode. On rotation, SyLog keeps
numbered archives (`app.log.1`, `app.log.2`, ...). `ok()` reports whether the
underlying stream is usable.

Rotation modes are behavior classes derived from `rotation::mode_base`.

Built-in helpers:

```cpp
sylog::rotation::never_rotate();
sylog::rotation::when_file_size_exceeds(10 * 1024 * 1024, 5);
sylog::rotation::when_new_day_begins(5);
sylog::rotation::when_file_size_exceeds_or_new_day_begins(10 * 1024 * 1024, 5);
```

## Custom sink

A sink derives from `sylog::ISink` and implements `write()` and `flush()`.

```cpp
#include <sylog/sylog.hpp>

#include <string_view>

class NullSink : public sylog::ISink {
public:
  void write(std::string_view) override {}
  void flush() override {}
};
```

Sink implementations should make their own thread-safety decision. In async mode
a route owns one worker lane, but the same sink object may be shared by multiple
routes. In synchronous mode, producer threads may call sink methods directly.
