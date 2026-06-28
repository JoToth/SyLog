# Crossbar sink design

The sink crossbar isolates routes from each other in async mode. A matching event
can be submitted to multiple route lanes, but each lane owns its own queue and
worker.

## Goals

- keep slow sinks from becoming a single global serialization point;
- allow different routes to choose different backpressure policies;
- preserve explicit lifecycle semantics for drain vs forced stop;
- keep Workers-specific details outside the public SyLog API.

## Lane contents

A lane contains:

- a bounded event queue;
- the route sink;
- the route renderer;
- route filters;
- the route delivery policy;
- one worker thread.

The worker checks filters before rendering. This keeps endpoint and severity
filtering correct even when the pipeline endpoint cache is stale or conservative.

## Route mask

The pipeline computes a route mask for an event endpoint. The mask is an
optimization that limits which lanes receive the event. It is not the final
authority: route filters are still evaluated inside each selected lane.

## Limits

The current route mask is `std::uint64_t`, so the optimized path can represent up
to 64 routes. Release notes should mention this if SyLog is published before a
larger route-selection representation is introduced.
