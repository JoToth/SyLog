#pragma once

#include "sylog/event.hpp"

#include <Workers/queue/mpmc_queue.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <stop_token>

namespace sylog::detail {

// Bounded Workers-backed event queue.
//
// Workers owns the concurrent queue and stop-token-aware consumer wait. SyLog
// only layers logging-specific bounded backpressure on top so route policies can
// choose drop or block without reimplementing queue correctness.
class WorkersEventQueue final {
public:
  explicit WorkersEventQueue(std::size_t capacity);

  WorkersEventQueue(const WorkersEventQueue&) = delete;
  WorkersEventQueue& operator=(const WorkersEventQueue&) = delete;

  [[nodiscard]] std::size_t capacity() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

  // Drop strategy: reserve once and fail immediately if the bounded capacity is full.
  bool try_push(Event&& e);

  // Block strategy: wait for capacity while accepting remains true.
  bool push_blocking(Event&& e, std::atomic<bool>& accepting);

  // Non-blocking drain path.
  bool try_pop(Event& out);

  // Worker path: delegates blocking wait to Workers::Queue::BlockingQueue.
  bool pop_blocking(Event& out, std::stop_token st);

  // Graceful close for the underlying Workers queue. Already-queued events can
  // still be drained with try_pop().
  void close();

  // Wake both Workers consumers and SyLog producers waiting for capacity.
  void notify_all();

private:
  bool reserve_slot_();
  void release_slot_();

  std::size_t capacity_{1};
  Workers::Queue::BlockingQueue<Event> queue_;
  std::atomic<std::size_t> size_{0};
  std::mutex space_m_;
  std::condition_variable cv_space_;
};

} // namespace sylog::detail
