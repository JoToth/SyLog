# Renderers and filters

Renderers convert a `sylog::Event` into bytes. Filters decide whether a route
accepts an event. Both are public extension points.

## Pattern renderer

`PatternRenderer` expands tokens into text. Supported tokens are:

- `{time}`
- `{severity}`
- `{severity_priority}`
- `{endpoint}`
- `{message}`
- `{file}`
- `{line}`
- `{func}`
- `{tid}`

```cpp
#include <sylog/sylog.hpp>

#include <memory>

auto renderer = std::make_shared<sylog::PatternRenderer>(
    "{time} [{severity}] {endpoint}: {message}\n");
```

Unknown text is copied literally. Add `\n` to the pattern when line-oriented
output is desired.

## ANSI console renderer

`AnsiConsoleRenderer` wraps pattern rendering and adds terminal-oriented severity
formatting for console output. Use it only for sinks where ANSI escape sequences
are acceptable.

```cpp
auto renderer = std::make_shared<sylog::AnsiConsoleRenderer>(
    "[{severity}] {message}\n");
```

## JSON renderer

`JsonRenderer` emits structured JSON and can include or omit source, thread, and
field data.

```cpp
auto renderer = std::make_shared<sylog::JsonRenderer>();
renderer->include_source(true)
        .include_thread(true)
        .include_fields(true);
```

All three flags default to `true` in the current implementation.

## Built-in filters

```cpp
route.filtered_by(sylog::SeverityFilter::at_least<sylog::severity::warn>());

auto endpoints = std::make_shared<sylog::EndpointFilter>();
endpoints->allow("service.").deny("service.debug.");
route.filtered_by(endpoints);
```

`EndpointFilter` evaluates deny prefixes first. If the allow list is empty, every
non-denied endpoint passes.

## Custom filters

A custom filter derives from `sylog::IFilter` and implements `pass()`.

```cpp
#include <sylog/sylog.hpp>

#include <memory>

class HealthcheckFilter : public sylog::IFilter {
public:
  bool pass(const sylog::Event& e) const override {
    return e.endpoint() != "service.http.healthcheck";
  }
};

route.filtered_by(std::make_shared<HealthcheckFilter>());
```

Filters should be cheap and non-blocking. They may run on producer threads in
synchronous dispatch and on route worker threads in async dispatch.

## Custom renderers

A custom renderer derives from `sylog::IRenderer` and writes bytes into the
provided output buffer.

```cpp
class MessageOnlyRenderer : public sylog::IRenderer {
public:
  void render(const sylog::Event& e, std::string& out) const override {
    out.assign(e.message());
    out.push_back('\n');
  }
};
```
