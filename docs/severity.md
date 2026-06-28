# Severity model

Severity is open and type-based. A severity is a class derived from
`sylog::severity_base`.

It describes event meaning only: name, priority, and optional sync/flush flags.
Delivery and backpressure belong to routes, not severity.

Built-in severities use increasing priority values:

| Type | Priority |
| --- | ---: |
| `sylog::severity::trace` | `100` |
| `sylog::severity::debug` | `200` |
| `sylog::severity::info` | `300` |
| `sylog::severity::warn` / `warning` | `400` |
| `sylog::severity::error` | `500` |
| `sylog::severity::fatal` | `600` |

`fatal` is special: it returns `true` from `force_sync()` and `flush_after()`.
Custom severities inherit `false` for both flags unless they override them.
