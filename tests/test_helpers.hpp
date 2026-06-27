#pragma once
#include <cpptest.h>

#include <chrono>
#include <thread>

// CppTest has TEST_ASSERT / TEST_ASSERT_MSG. We'll use those.
// If you already have your own CHECK/CHECK_MSG wrappers, you can drop these.
#ifndef CHECK
#define CHECK(expr) TEST_ASSERT((expr))
#endif

#ifndef CHECK_MSG
#define CHECK_MSG(expr, msg) TEST_ASSERT_MSG((expr), (msg))
#endif

// Repeatedly evaluates `expr` until it becomes true or timeout expires.
// This is a "bounded wait" helper for concurrency tests.
//
// NOTE: Not for performance/timing assertions. Use generous timeouts (e.g. 1–5 seconds).
#define SYLOG_CHECK_EVENTUALLY(expr, timeout_duration, msg)                          \
  do {                                                                               \
    const auto __deadline = std::chrono::steady_clock::now() + (timeout_duration);  \
    bool __ok = false;                                                              \
    while (std::chrono::steady_clock::now() < __deadline) {                         \
      if ((expr)) { __ok = true; break; }                                            \
      std::this_thread::yield();                                                     \
    }                                                                                \
    CHECK_MSG(__ok, (msg));                                                         \
  } while (0)

// Like EVENTUALLY, but expects the condition to remain false for the whole window.
// Useful for "should still be blocked" checks.
#define SYLOG_CHECK_STAYS_FALSE(expr, duration, msg)                                 \
  do {                                                                               \
    const auto __deadline = std::chrono::steady_clock::now() + (duration);          \
    bool __violated = false;                                                        \
    while (std::chrono::steady_clock::now() < __deadline) {                         \
      if ((expr)) { __violated = true; break; }                                      \
      std::this_thread::yield();                                                     \
    }                                                                                \
    CHECK_MSG(!__violated, (msg));                                                  \
  } while (0)

