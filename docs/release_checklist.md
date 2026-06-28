# Release checklist

Use this checklist before tagging a public release.

## Source tree

1. Confirm the repository license is unambiguous:
   - `LICENSE` contains BSD-3-Clause text;
   - README says BSD-3-Clause;
   - docs do not mention another license;
   - generated/package metadata does not mention another license.
2. Update `CHANGELOG.md` and remove stale `unreleased` wording for the tag.
3. Review public docs:
   - `README.md`;
   - `docs/getting_started.md`;
   - `docs/api/public_api.md`;
   - `docs/api/lifecycle.md`;
   - `docs/api/routing.md`;
   - `docs/api/sinks.md`;
   - `docs/api/renderers_filters.md`;
   - `docs/api/queue.md`;
   - `docs/api/examples.md`;
   - `docs/benchmarks/scenarios.md`;
   - `docs/architecture.md`;
   - `docs/severity.md`;
   - `docs/internals/*.md` for implementation notes that may affect release notes.
4. Confirm dependency refs in `deps.cmake` are pinned to intended tags/commits.
5. Confirm public examples still use only installed/public headers.

## Build and test

```sh
cmake -S . -B build -DSYLOG_BUILD_TESTS=ON -DSYLOG_BUILD_EXAMPLES=ON -DSYLOG_BUILD_BENCHMARKS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Stress tests:

```sh
ctest --test-dir build -L stress --output-on-failure
```

Benchmark smoke test:

```sh
./build/benchmarks/SyLogBenchmarks --scenario all --events 10000 --producers 4 --capacity 4096
```

Install smoke test:

```sh
cmake --install build --prefix install
```

Consumer smoke test after install:

```sh
cmake -S path/to/consumer -B consumer-build -DCMAKE_PREFIX_PATH="$PWD/install"
cmake --build consumer-build
```

## Documentation sanity checks

- Code snippets use `#include <sylog/sylog.hpp>` or explicitly list required
  public headers.
- No documentation describes closed enum configuration for severity, delivery, or
  rotation.
- Lifecycle docs distinguish graceful `close()` from forced `request_stop()` and
  do not describe `flush()` as a queue-drain barrier.
- Benchmark docs describe workload diagnostics, not portable performance claims.
- Documentation lists only currently implemented sinks. Future syslog or Windows
  Event Log sinks must be marked as roadmap items until implemented.

## Tag

```sh
git tag v0.1.0
git push origin v0.1.0
```
