#include "async/workers_event_queue.hpp"

#include <algorithm>
#include <utility>

namespace sylog::detail {

WorkersEventQueue::WorkersEventQueue(std::size_t capacity)
    : capacity_(std::max<std::size_t>(capacity, 1u)) {}

std::size_t WorkersEventQueue::capacity() const noexcept { return capacity_; }

std::size_t WorkersEventQueue::size() const noexcept {
  return size_.load(std::memory_order_acquire);
}

bool WorkersEventQueue::empty() const noexcept { return size() == 0; }

bool WorkersEventQueue::try_push(Event&& e) {
  if (!reserve_slot_()) return false;
  if (!queue_.push(std::move(e))) {
    release_slot_();
    return false;
  }
  return true;
}

bool WorkersEventQueue::push_blocking(Event&& e, std::atomic<bool>& accepting) {
  while (accepting.load(std::memory_order_acquire)) {
    if (reserve_slot_()) {
      if (queue_.push(std::move(e))) return true;
      release_slot_();
      return false;
    }

    std::unique_lock lk(space_m_);
    cv_space_.wait(lk, [&] {
      return !accepting.load(std::memory_order_acquire) ||
             size_.load(std::memory_order_acquire) < capacity_;
    });
  }

  return false;
}

bool WorkersEventQueue::try_pop(Event& out) {
  if (!queue_.try_pop(out)) return false;
  release_slot_();
  return true;
}

bool WorkersEventQueue::pop_blocking(Event& out, std::stop_token st) {
  const auto result = queue_.pop(st, out);
  if (result != Workers::Queue::PopResult::ok) return false;
  release_slot_();
  return true;
}

void WorkersEventQueue::close() {
  queue_.close();
  notify_all();
}

void WorkersEventQueue::notify_all() {
  queue_.notify_all();
  cv_space_.notify_all();
}

bool WorkersEventQueue::reserve_slot_() {
  auto n = size_.load(std::memory_order_acquire);
  while (n < capacity_) {
    if (size_.compare_exchange_weak(
            n, n + 1,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      return true;
    }
  }
  return false;
}

void WorkersEventQueue::release_slot_() {
  size_.fetch_sub(1, std::memory_order_acq_rel);
  cv_space_.notify_one();
}

} // namespace sylog::detail
