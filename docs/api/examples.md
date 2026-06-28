# Examples

The `examples/` directory contains small, buildable programs that show the public
API.

| Target | Source | Purpose |
|---|---|---|
| `SyLogExampleBasic` | `examples/basic.cpp` | synchronous console route |
| `SyLogExampleAsyncWorkers` | `examples/async_workers.cpp` | async Workers-backed route |
| `SyLogExampleCustomSeverity` | `examples/custom_severity.cpp` | user-defined severity class |
| `SyLogExampleRouting` | `examples/routing.cpp` | console + rotating file routes |

Build all examples:

```sh
cmake -S . -B build -DSYLOG_BUILD_EXAMPLES=ON
cmake --build build
```

Build one example:

```sh
cmake --build build --target SyLogExampleBasic
```

Examples are smoke tests for the public API surface. Keep them short and avoid
showing internal test-only helpers.
