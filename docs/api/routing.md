# Routing

A SyLog route connects filtering, rendering, delivery policy, and sink output.

```cpp
#include <sylog/sylog.hpp>

#include <memory>
#include <utility>

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

## Matching

A route is eligible for an event when all route filters pass. Multiple routes may
match the same event; each matching route receives the event independently.

`SeverityFilter::at_least<T>()` passes events whose severity priority is greater
than or equal to `T::priority()`.

`EndpointFilter` supports prefix-based allow and deny lists. Deny rules are
checked first. If no allow rules are configured, every non-denied endpoint is
allowed.

```cpp
auto endpoint_filter = std::make_shared<sylog::EndpointFilter>();
endpoint_filter->allow("service.http").deny("service.http.noisy");

route.filtered_by(endpoint_filter);
```

`EndpointFilter` uses simple prefix matching. The prefix `service.http` matches
`service.http`, `service.http.api`, and `service.http.noisy`.

## Global filters

Global filters are evaluated after route-mask lookup and before dispatch. Use
them for process-wide decisions that should affect every route.

```cpp
sylog::Pipeline::instance().add_global_filter(
    sylog::SeverityFilter::at_least<sylog::severity::info>());
```

## Delivery policy

Backpressure is configured per route:

```cpp
route.drop_when_full();   // favor producer latency; events may be lost
route.block_when_full();  // favor event preservation; producers may wait
```

In async mode, each route has its own crossbar lane. A slow sink therefore does
not globally serialize unrelated routes, although a route configured with
`block_when_full()` may still block producers for that route.

## Fallback behavior

If no route matches an event, SyLog normally drops it. `ERROR` and more severe
events are written to the emergency fallback on `stderr` so severe failures are
not silently lost when routing is empty or misconfigured.

## Route count

The current route mask is a 64-bit mask, so only the first 64 routes are
considered by the dispatcher. Treat this as a current implementation limit for
`0.1.x`.
