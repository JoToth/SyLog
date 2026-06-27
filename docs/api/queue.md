# Delivery and backpressure

Backpressure is configured per route with delivery policy classes.

```cpp
route.drop_when_full();
route.block_when_full();
```

A slow sink has its own crossbar lane, so it does not block unrelated routes unless those routes are explicitly configured to block.
