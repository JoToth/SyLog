# Examples

The `examples/` directory contains small, buildable programs that show the public API.

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
