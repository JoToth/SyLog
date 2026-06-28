# Lifecycle

SyLog has explicit lifecycle operations because async logging must make data-loss
behavior visible.

## Normal startup

Configure the pipeline, add routes, then start it:

```cpp
sylog::Pipeline::instance().configure(sylog::Config::asynchronous(16384));
sylog::Pipeline::instance().add_route(std::move(route));
sylog::Pipeline::instance().start();
```

A logger call made before `start()` is still accepted. In async configuration it
can be queued until the crossbar starts, subject to the configured per-route
capacity and delivery policy; in synchronous configuration it is dispatched on
the caller thread.

## `start()`

Starts the configured pipeline and route workers.

```cpp
sylog::Pipeline::instance().start();
```

Routes and filters should normally be configured before `start()`. Calling
`start()` repeatedly is harmless; only the first call starts workers.

## `close()`

Gracefully stops the pipeline:

1. stop accepting new async events;
2. drain queued events;
3. flush sinks;
4. recreate the internal crossbar so the singleton can be configured again in
   tests or short-lived tools.

```cpp
sylog::Pipeline::instance().close();
```

Use this for normal application shutdown.

## `request_stop()`

Requests fast stop and wakes blocked workers/producers. This is the escape hatch
for tests, signal handlers, and failure paths where bounded shutdown time matters
more than draining all events.

```cpp
sylog::Pipeline::instance().request_stop();
```

Do not use `request_stop()` as the normal shutdown path for applications that
must preserve queued log events.

## `flush()`

Flushes configured sinks. In async mode, `flush()` flushes sink objects; it is
not a drain barrier for events that are still queued behind worker threads. Use
`close()` when queued async events must be drained before shutdown.

```cpp
sylog::Pipeline::instance().flush();
```

`flush()` does not imply `close()` and does not reconfigure the pipeline.

## Reconfiguration

For tests and tools that run multiple independent scenarios in one process, use
this pattern:

```cpp
sylog::Pipeline::instance().close();
sylog::Pipeline::instance().clear_routes();
sylog::Pipeline::instance().clear_global_filters();
sylog::Pipeline::instance().configure(sylog::Config::synchronous());

// add routes, then start again
```

## Recommended pattern

```cpp
sylog::Pipeline::instance().start();

// application work

sylog::Pipeline::instance().close();
```

Use `request_stop()` only for forced termination paths.
