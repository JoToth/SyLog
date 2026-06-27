#pragma once

#include "sylog/logger.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace sylog::detail {

class WorkersEventQueue;

// SinkCrossbar fans routed events out into one lane per route/sink.
//
// The logger pipeline decides *which* routes match an event. The crossbar owns
// per-route queues and workers so a slow sink cannot stall unrelated sinks. Each
// lane applies its route filters again immediately before rendering/writing; this
// preserves dynamic filter semantics and keeps the endpoint cache an optimization,
// not a correctness dependency.
class SinkCrossbar final {
public:
  explicit SinkCrossbar(std::size_t queue_capacity);
  ~SinkCrossbar();

  SinkCrossbar(const SinkCrossbar&) = delete;
  SinkCrossbar& operator=(const SinkCrossbar&) = delete;

  void configure(std::vector<Route> routes, std::size_t queue_capacity);
  void start();
  void close();
  void request_stop();
  void flush();

  [[nodiscard]] bool is_running() const noexcept;
  [[nodiscard]] std::size_t lane_count() const;

  // Submit to every bit set in route_mask. Delivery behavior is evaluated per lane
  // from the route delivery strategy object.
  bool submit(Event e, std::uint64_t route_mask, std::atomic<bool>& accepting);

private:
  struct Lane;

  static void process_(const Route& route, const Event& e, std::string& buffer);
  static bool route_passes_(const Route& route, const Event& e);
  static void lane_loop_(Lane& lane, std::stop_token st);

  void stop_lanes_(bool drain);
  std::vector<std::shared_ptr<Lane>> snapshot_lanes_() const;

  mutable std::mutex m_;
  std::size_t queue_capacity_{1};
  bool running_{false};
  std::vector<std::shared_ptr<Lane>> lanes_;
};

} // namespace sylog::detail
