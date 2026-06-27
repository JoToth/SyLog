# Sinks

## Console

```cpp
auto console = std::make_shared<sylog::ConsoleSink>();
```

## Rotating file

```cpp
auto file = std::make_shared<sylog::RotatingFileSink>(
    "app.log",
    sylog::rotation::when_file_size_exceeds(10 * 1024 * 1024, 5));
```

Rotation modes are behavior classes derived from `rotation::mode_base`.
