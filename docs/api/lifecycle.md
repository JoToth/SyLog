# Lifecycle

SyLog has explicit lifecycle operations because async logging must make data-loss behavior visible.

## `start()`

Starts the configured pipeline and route workers.

```cpp
sylog::Pipeline::instance().start();
```

Routes and filters should normally be configured before `start()`.

## `close()`

Gracefully stops the pipeline:

1. stop accepting new async events;
2. drain queued events;
3. flush sinks;
4. recreate the internal crossbar so the singleton can be configured again in tests or short-lived tools.

```cpp
sylog::Pipeline::instance().close();
```

Use this for normal application shutdown.

## `request_stop()`

Requests fast stop and wakes blocked workers. This is the escape hatch for tests, signal handlers, and failure paths where bounded shutdown time matters more than draining all events.

```cpp
sylog::Pipeline::instance().request_stop();
```

## `flush()`

Flushes configured sinks.

```cpp
sylog::Pipeline::instance().flush();
```

## Recommended pattern

```cpp
sylog::Pipeline::instance().start();

// application work

sylog::Pipeline::instance().close();
```

Use `request_stop()` only for forced termination paths.
