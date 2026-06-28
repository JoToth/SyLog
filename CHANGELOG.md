# Changelog

All notable changes to SyLog are tracked here.

## 0.1.0 - unreleased

### Added
- Type-based severity model with user-defined severities.
- Workers-backed asynchronous pipeline.
- Crossbar sink routing with isolated route lanes.
- Delivery policies for dropping or blocking when a route queue is full.
- Pattern, ANSI console, and JSON renderers using severity terminology.
- Structured fields through `Logger::log_fields()` and JSON output.
- Rotating file sink with behavior-bearing rotation modes.
- Scenario-based benchmarks and bounded stress tests.
- CMake install/export package metadata.
- BSD-3-Clause license and GitHub-ready repository metadata.

### Documented
- Public extension points for severity, delivery, rotation, filters, renderers,
  and sinks.
- Graceful `close()` versus forced `request_stop()` lifecycle behavior.
- Benchmark scenarios and interpretation boundaries.
