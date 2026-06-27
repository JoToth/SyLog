# Severity model

Severity is open and type-based. A severity is a class derived from `sylog::severity_base`.

It describes event meaning only: name, priority, and optional sync/flush flags. Delivery/backpressure belongs to routes, not severity.
