#include "async/sink_crossbar.hpp"

#include "async/workers_event_queue.hpp"

#include <algorithm>
#include <utility>

namespace sylog::detail {

struct SinkCrossbar::Lane {
  Route route;
  std::shared_ptr<WorkersEventQueue> queue;
  std::jthread worker;
  std::atomic<bool> drain_on_stop{true};
};

SinkCrossbar::SinkCrossbar(std::size_t queue_capacity)
    : queue_capacity_(std::max<std::size_t>(queue_capacity, 1u)) {}

SinkCrossbar::~SinkCrossbar() { request_stop(); }

void SinkCrossbar::configure(std::vector<Route> routes, std::size_t queue_capacity) {
  const bool restart = is_running();
  if (restart) stop_lanes_(/*drain=*/true);

  std::lock_guard lk(m_);
  queue_capacity_ = std::max<std::size_t>(queue_capacity, 1u);
  lanes_.clear();
  lanes_.reserve(routes.size());

  for (auto& route : routes) {
    auto lane = std::make_shared<Lane>();
    lane->route = std::move(route);
    lane->queue = std::make_shared<WorkersEventQueue>(queue_capacity_);
    lanes_.push_back(std::move(lane));
  }

  if (restart) {
    running_ = true;
    for (auto& lane : lanes_) {
      lane->drain_on_stop.store(true, std::memory_order_release);
      lane->worker = std::jthread([lane](std::stop_token st) { lane_loop_(*lane, st); });
    }
  }
}

void SinkCrossbar::start() {
  std::lock_guard lk(m_);
  if (running_) return;
  running_ = true;

  for (auto& lane : lanes_) {
    lane->drain_on_stop.store(true, std::memory_order_release);
    if (!lane->queue) lane->queue = std::make_shared<WorkersEventQueue>(queue_capacity_);
    lane->worker = std::jthread([lane](std::stop_token st) { lane_loop_(*lane, st); });
  }
}

void SinkCrossbar::close() { stop_lanes_(/*drain=*/true); }

void SinkCrossbar::request_stop() { stop_lanes_(/*drain=*/false); }

void SinkCrossbar::flush() {
  auto lanes = snapshot_lanes_();
  for (auto& lane : lanes) {
    if (lane && lane->route.sink_ref()) lane->route.sink_ref()->flush();
  }
}

bool SinkCrossbar::is_running() const noexcept {
  std::lock_guard lk(m_);
  return running_;
}

std::size_t SinkCrossbar::lane_count() const {
  std::lock_guard lk(m_);
  return lanes_.size();
}

bool SinkCrossbar::submit(Event e, std::uint64_t route_mask, std::atomic<bool>& accepting) {
  auto lanes = snapshot_lanes_();
  bool delivered_any = false;

  for (std::size_t i = 0; i < lanes.size() && i < 64; ++i) {
    if (((route_mask >> i) & 1ull) == 0) continue;
    auto& lane = lanes[i];
    if (!lane || !lane->queue) continue;

    Event copy = e;
    const auto strategy = lane->route.delivery();
    const bool ok = strategy
        ? strategy->submit(*lane->queue, std::move(copy), accepting)
        : lane->queue->try_push(std::move(copy));
    delivered_any = delivered_any || ok;
  }

  return delivered_any;
}

bool SinkCrossbar::route_passes_(const Route& route, const Event& e) {
  for (auto const& f : route.filter_list()) {
    if (f && !f->pass(e)) return false;
  }
  return true;
}

void SinkCrossbar::process_(const Route& route, const Event& e, std::string& buffer) {
  if (!route.sink_ref()) return;
  if (!route_passes_(route, e)) return;

  buffer.clear();
  if (route.renderer_ref()) {
    route.renderer_ref()->render(e, buffer);
  } else {
    buffer = e.message();
    if (!buffer.empty() && buffer.back() != '\n') buffer.push_back('\n');
  }

  route.sink_ref()->write(buffer);
}

void SinkCrossbar::lane_loop_(Lane& lane, std::stop_token st) {
  if (!lane.queue) return;

  std::string buffer;
  buffer.reserve(512);

  Event e;
  while (lane.queue->pop_blocking(e, st)) {
    process_(lane.route, e, buffer);
  }

  // If this worker was gracefully closed, drain any already queued events.
  // Immediate request_stop() flips the lane flag before waking the queue.
  if (!lane.drain_on_stop.load(std::memory_order_acquire)) return;
  while (lane.queue->try_pop(e)) {
    process_(lane.route, e, buffer);
  }
}

void SinkCrossbar::stop_lanes_(bool drain) {
  std::vector<std::shared_ptr<Lane>> lanes;
  {
    std::lock_guard lk(m_);
    if (!running_ && lanes_.empty()) return;
    running_ = false;
    lanes = lanes_;
    for (auto& lane : lanes) {
      if (lane) lane->drain_on_stop.store(drain, std::memory_order_release);
    }
  }

  for (auto& lane : lanes) {
    if (!lane) continue;
    if (lane->queue) lane->queue->close();
    if (lane->worker.joinable()) lane->worker.request_stop();
  }

  for (auto& lane : lanes) {
    if (lane && lane->worker.joinable()) lane->worker.join();
  }

  if (!drain) {
    std::lock_guard lk(m_);
    for (auto& lane : lanes_) {
      if (lane) lane->queue = std::make_shared<WorkersEventQueue>(queue_capacity_);
    }
  }
}

std::vector<std::shared_ptr<SinkCrossbar::Lane>> SinkCrossbar::snapshot_lanes_() const {
  std::lock_guard lk(m_);
  return lanes_;
}

} // namespace sylog::detail
