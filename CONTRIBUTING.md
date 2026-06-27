# Contributing

Thanks for helping improve SyLog.

## Development loop

```sh
cmake -S . -B build -DSYLOG_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Run stress tests explicitly when touching async lifecycle, routing, or backpressure:

```sh
ctest --test-dir build -L stress --output-on-failure
```

## Style

- C++20 only.
- Keep logging-domain logic in SyLog.
- Keep concurrency/runtime ownership in Workers.
- Prefer explicit lifecycle APIs over implicit destructor shutdown.
- Tests must be deterministic: no unbounded sleeps or infinite waits.

## Pull requests

A PR should include:

- concise description of the behavior change
- tests for new behavior
- benchmark notes if performance-sensitive code changes
- documentation updates when public behavior changes
