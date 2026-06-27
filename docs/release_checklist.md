# Release checklist

Use this checklist before tagging a public release.

## Source tree

1. Confirm the repository license is unambiguous:
   - `LICENSE` contains BSD-3-Clause text;
   - README says BSD-3-Clause;
   - docs do not mention another license;
   - generated/package metadata does not mention another license.
2. Update `CHANGELOG.md`.
3. Review public docs:
   - `README.md`;
   - `docs/getting_started.md`;
   - `docs/api/public_api.md`;
   - `docs/api/lifecycle.md`;
   - `docs/api/routing.md`;
   - `docs/api/sinks.md`;
   - `docs/api/renderers_filters.md`;
   - `docs/benchmarks/scenarios.md`.
4. Decide whether dependency refs in `deps.cmake` remain on branches or are pinned to tags/commits.

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

## Tag

```sh
git tag v0.1.0
git push origin v0.1.0
```
