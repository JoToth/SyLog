# Delivery and backpressure

Backpressure is configured per route with delivery policy classes. It matters
for asynchronous routes; synchronous dispatch calls matching sinks directly on
the producer thread.

```cpp
route.drop_when_full();
route.block_when_full();
```

A slow sink has its own crossbar lane, so it does not block unrelated routes
unless those routes are explicitly configured to block.

## Policies

- `drop_when_full()` favors producer latency. If a route queue is full, the event
  may be lost for that route.
- `block_when_full()` favors event preservation. If a route queue is full, the
  producer may wait until the route can accept another event or shutdown begins.

The queue capacity is configured on the pipeline:

```cpp
sylog::Pipeline::instance().configure(sylog::Config::asynchronous(16384));
```

Capacity is per route lane, not a single global queue. A route with
`block_when_full()` can stall a producer until that lane has capacity; a route
with `drop_when_full()` can lose events for that lane while other lanes continue
independently.
