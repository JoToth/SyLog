# Dependency boundary

This internal note records how SyLog uses external dependencies.

## Workers

SyLog links `Workers::Workers` and adapts it through `sylog::detail::WorkersEventQueue`.
The adapter provides SyLog-specific semantics:

- per-route bounded capacity;
- route-local drop/block behavior;
- producer wakeups on capacity release;
- close/request-stop wakeups;
- blocking pop with `std::stop_token`.

Workers types are not exposed in public SyLog headers.

## argtree

`argtree::argtree` is used by command-line programs only:

- `SyLogBenchmarks`;
- `sylogctl`.

The library target does not require user code to include argtree headers.

## CPpTest

`cpptest::cpptest` is used only by tests.
