#include "sylog/delivery.hpp"

#include "async/workers_event_queue.hpp"

#include <utility>

namespace sylog::delivery {

std::string_view drop_when_full::name() const noexcept { return "drop_when_full"; }

bool drop_when_full::submit(detail::WorkersEventQueue& queue, Event&& event, std::atomic<bool>&) const {
  return queue.try_push(std::move(event));
}

std::string_view block_when_full::name() const noexcept { return "block_when_full"; }

bool block_when_full::submit(detail::WorkersEventQueue& queue, Event&& event, std::atomic<bool>& accepting) const {
  return queue.push_blocking(std::move(event), accepting);
}

} // namespace sylog::delivery
