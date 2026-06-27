---
name: Bug report
about: Report a reproducible SyLog problem
labels: bug
---

## Summary

## Reproduction

```sh
cmake -S . -B build -DSYLOG_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Expected behavior

## Actual behavior

## Environment

- OS:
- Compiler:
- CMake:
- SyLog commit:
