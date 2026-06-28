# Async pipeline internals

This note describes the implementation boundary behind the public lifecycle API.
It is not an application-facing API contract.

## Shape

`sylog::Pipeline` owns the configured routes and a `sylog::detail::SinkCrossbar`.
The crossbar owns one lane per route. Each lane contains its own bounded queue,
worker thread, renderer, sink, filters, and delivery policy.

```text
Logger::log()
  -> Pipeline::submit()
  -> route mask lookup
  -> SinkCrossbar lane queues
  -> route worker
  -> filters
  -> renderer
  -> sink.write()/sink.flush()
```

## Backpressure

Delivery policy is evaluated per route. `drop_when_full()` allows producers to
continue when the lane queue is full. `block_when_full()` waits until capacity is
available or the pipeline stops accepting events.

## Shutdown

`close()` stops accepting new async events, drains queued work, flushes sinks, and
then recreates the crossbar so tests and short-lived tools can reconfigure the
singleton.

`request_stop()` stops accepting new async events and wakes blocked workers for
fast termination. It is intended for forced shutdown paths where bounded latency
matters more than draining every queued event.

## Dependency boundary

Queue and worker-runtime correctness belongs to Workers. SyLog tests the
integration contract: route selection, rendering, delivery policy behavior,
draining, forced stop, and sink isolation.
