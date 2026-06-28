# Architecture

SyLog has four runtime layers:

```text
application code
  -> Logger endpoint facade
  -> Pipeline filters and route matching
  -> SinkCrossbar async route lanes
  -> Renderer + Sink write/flush
```

## Logger layer

`sylog::Logger` stores an endpoint string. Each enabled log call creates an
`Event` with severity, timestamp, endpoint, source location, thread id, message,
and optional structured fields.

Before formatting, the logger asks the pipeline for the cached minimum enabled
severity for that endpoint. Disabled severities return early and avoid
`std::format` cost.

## Pipeline layer

`Pipeline` owns:

- global filters;
- routes;
- endpoint cache;
- lifecycle state;
- the async sink crossbar.

Route matching produces a route mask. Synchronous dispatch writes matching routes
on the calling thread. Async dispatch sends the event and route mask to the
crossbar.

The endpoint cache is an optimization only. Route filters are checked again
inside the crossbar lane before rendering and writing. This keeps routing correct
when filters are more specific than the cached endpoint/severity summary.

If no route matches an event, SyLog drops the event except for `ERROR` and more
severe events, which are written to the emergency fallback on `stderr`.

## Crossbar layer

The crossbar owns one lane per route. Each lane contains:

- a bounded Workers-backed queue;
- one `std::jthread` worker;
- the route's sink;
- the route's renderer;
- the route's filters;
- the route delivery policy.

This prevents a slow route from becoming a global serialization point. Capacity
is per lane, not one shared global queue.

The current route mask is 64 bits. Only the first 64 routes are considered by the
dispatch path in `0.1.x`.

## Sink/write layer

A route worker pops events, checks filters, renders into a reusable string
buffer, writes the bytes to the sink, and flushes when lifecycle requires it.

If a route has no renderer, SyLog writes the event message and appends a newline
when needed. If a route has no sink, the route produces no output.

## Lifecycle

`close()` is graceful and drain-oriented. `request_stop()` is immediate and
wake-oriented. That split is part of the public API because async logging must
make shutdown data-loss semantics explicit.

## Dependency boundary

SyLog depends on Workers for queue/worker behavior and argtree for command-line
tools. Those dependencies are not part of the application-facing logging API.
Public headers live under `include/sylog/`; internal backend code lives under
`src/`.

## Dependency testing boundary

The Workers dependency owns queue and worker-runtime correctness. SyLog tests
only the integration contract: events submitted through SyLog are routed,
rendered, delivered, drained, or stopped according to SyLog configuration.
Low-level queue and scheduler tests belong in Workers, not SyLog.
