# Public API

SyLog's public API is intentionally type/policy based.

There is no public `Level` enum, no public `QueueStrategy` enum, and no public `Rotation::Mode` enum. Built-in behaviors are normal classes with methods, and user code can define new classes that derive from the appropriate base class.

## Severity

Severity is an event classification type.

```cpp
class audit : public sylog::severity_base {
public:
  static constexpr std::string_view name() noexcept { return "AUDIT"; }
  static constexpr std::uint16_t priority() noexcept { return 450; }
};

log.log<audit>("user logged in");
```

Built-ins:

- `sylog::severity::trace`
- `sylog::severity::debug`
- `sylog::severity::info`
- `sylog::severity::warn` / `warning`
- `sylog::severity::error`
- `sylog::severity::fatal`

## Delivery policy

Delivery is route configuration, not severity metadata.

```cpp
route.drop_when_full();
route.block_when_full();
route.deliver_with<my_delivery_strategy>();
```

Custom delivery policies derive from `sylog::delivery::strategy_base`.

## Rotation policy

Rotation is a sink behavior object.

```cpp
auto sink = std::make_shared<sylog::RotatingFileSink>(
    "app.log",
    sylog::rotation::when_file_size_exceeds(10 * 1024 * 1024, 5));
```

Custom rotation policies derive from `sylog::rotation::mode_base`.

## Routes

```cpp
sylog::Route route;
route.send_to(console)
     .render_with(renderer)
     .filtered_by(sylog::SeverityFilter::at_least<sylog::severity::info>())
     .block_when_full();
```

## Logger

```cpp
sylog::Logger log("service.http");
log.info("started");
log.log<my_severity>("custom event");
```
