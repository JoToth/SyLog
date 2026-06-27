#include "sylog/logger.hpp"

#include "async/sink_crossbar.hpp"

#include <chrono>
#include <cstdio>
#include <format>
#include <mutex>
#include <thread>
#include <stop_token>

namespace sylog {

SeverityFilter::SeverityFilter(std::uint16_t minimum_priority) : minimum_priority_(minimum_priority) {}

bool SeverityFilter::pass(const Event& e) const { return e.severity_priority() >= minimum_priority_; }

EndpointFilter& EndpointFilter::allow(std::string prefix) {
  allow_prefixes_.push_back(std::move(prefix));
  return *this;
}

EndpointFilter& EndpointFilter::deny(std::string prefix) {
  deny_prefixes_.push_back(std::move(prefix));
  return *this;
}

bool EndpointFilter::matches_prefix(std::string_view ep, std::string_view prefix) {
  if (prefix.empty()) return true;
  if (ep.size() < prefix.size()) return false;
  return ep.substr(0, prefix.size()) == prefix;
}

bool EndpointFilter::pass(const Event& e) const {
  for (auto const& d : deny_prefixes_) {
    if (matches_prefix(e.endpoint(), d)) return false;
  }
  if (allow_prefixes_.empty()) return true;
  for (auto const& a : allow_prefixes_) {
    if (matches_prefix(e.endpoint(), a)) return true;
  }
  return false;
}

struct Pipeline::Impl {
  Config cfg{};

  mutable std::mutex state_m;
  std::vector<std::shared_ptr<IFilter>> global_filters;
  std::vector<Route> routes;

  std::mutex worker_m;
  std::atomic<bool> running{false};
  std::atomic<bool> accepting{true};
  std::shared_ptr<detail::SinkCrossbar> crossbar;

  std::atomic<bool> dirty_cache{true};
  std::shared_mutex cache_m;
  std::unordered_map<std::string, CacheEntry> endpoint_cache;
  std::atomic<std::uint16_t> global_min_priority{severity::trace::priority()};
};

Pipeline& Pipeline::instance() {
  static Pipeline p;
  return p;
}

Pipeline::Pipeline() : impl_(std::make_unique<Impl>()) {
  impl_->crossbar = std::make_shared<detail::SinkCrossbar>(impl_->cfg.queue_capacity());
}

Pipeline::~Pipeline() { close(); }

bool Pipeline::is_running() const noexcept { return impl_->running.load(std::memory_order_acquire); }

void Pipeline::configure(Config cfg) {
  {
    std::lock_guard lk(impl_->state_m);
    impl_->cfg = cfg;
  }

  if (!impl_->running.load(std::memory_order_acquire)) {
    std::vector<Route> routes;
    {
      std::lock_guard lk(impl_->state_m);
      routes = impl_->routes;
    }
    std::lock_guard lk(impl_->worker_m);
    impl_->crossbar = std::make_shared<detail::SinkCrossbar>(impl_->cfg.queue_capacity());
    impl_->crossbar->configure(std::move(routes), impl_->cfg.queue_capacity());
    impl_->accepting.store(true, std::memory_order_release);
  }
}

void Pipeline::add_global_filter(std::shared_ptr<IFilter> f) {
  std::lock_guard lk(impl_->state_m);
  impl_->global_filters.push_back(std::move(f));
}

void Pipeline::clear_global_filters() {
  std::lock_guard lk(impl_->state_m);
  impl_->global_filters.clear();
}

void Pipeline::invalidate_cache_() { impl_->dirty_cache.store(true, std::memory_order_release); }

void Pipeline::add_route(Route r) {
  std::vector<Route> routes;
  {
    std::lock_guard lk(impl_->state_m);
    impl_->routes.push_back(std::move(r));
    routes = impl_->routes;
    invalidate_cache_();
  }

  std::lock_guard wk(impl_->worker_m);
  if (impl_->crossbar) impl_->crossbar->configure(std::move(routes), impl_->cfg.queue_capacity());
}

void Pipeline::clear_routes() {
  {
    std::lock_guard lk(impl_->state_m);
    impl_->routes.clear();
    invalidate_cache_();
  }

  std::lock_guard wk(impl_->worker_m);
  if (impl_->crossbar) impl_->crossbar->configure({}, impl_->cfg.queue_capacity());
}

void Pipeline::start() {
  std::lock_guard lk(impl_->worker_m);
  impl_->accepting.store(true, std::memory_order_release);
  if (impl_->running.exchange(true, std::memory_order_acq_rel)) return;
  if (!impl_->crossbar) impl_->crossbar = std::make_shared<detail::SinkCrossbar>(impl_->cfg.queue_capacity());
  impl_->crossbar->start();
}

void Pipeline::close() {
  impl_->accepting.store(false, std::memory_order_release);
  {
    std::lock_guard lk(impl_->worker_m);
    impl_->running.store(false, std::memory_order_release);
    if (impl_->crossbar) impl_->crossbar->close();
  }
  flush();

  std::vector<Route> routes;
  {
    std::lock_guard lk(impl_->state_m);
    routes = impl_->routes;
  }
  {
    std::lock_guard lk(impl_->worker_m);
    impl_->crossbar = std::make_shared<detail::SinkCrossbar>(impl_->cfg.queue_capacity());
    impl_->crossbar->configure(std::move(routes), impl_->cfg.queue_capacity());
  }
}

void Pipeline::request_stop() {
  impl_->accepting.store(false, std::memory_order_release);
  {
    std::lock_guard lk(impl_->worker_m);
    impl_->running.store(false, std::memory_order_release);
    if (impl_->crossbar) impl_->crossbar->request_stop();
  }
  flush();

  std::vector<Route> routes;
  {
    std::lock_guard lk(impl_->state_m);
    routes = impl_->routes;
  }
  {
    std::lock_guard lk(impl_->worker_m);
    impl_->crossbar = std::make_shared<detail::SinkCrossbar>(impl_->cfg.queue_capacity());
    impl_->crossbar->configure(std::move(routes), impl_->cfg.queue_capacity());
  }
}


void Pipeline::flush() {
  std::shared_ptr<detail::SinkCrossbar> crossbar;
  {
    std::lock_guard lk(impl_->worker_m);
    crossbar = impl_->crossbar;
  }
  if (crossbar) crossbar->flush();
}

Pipeline::CacheEntry Pipeline::compute_cache_entry_(std::string_view endpoint) {
  CacheEntry out;
  out.min_priority = 65535;
  out.route_mask = 0;

  std::vector<Route> routes;
  {
    std::lock_guard lk(impl_->state_m);
    routes = impl_->routes;
  }

  for (std::size_t i = 0; i < routes.size() && i < 64; ++i) {
    bool endpoint_ok = true;
    std::uint16_t min_priority = severity::trace::priority();

    for (auto const& f : routes[i].filter_list()) {
      if (!f) continue;
      if (auto* sf = dynamic_cast<SeverityFilter*>(f.get())) {
        min_priority = sf->minimum_priority();
      } else if (auto* ef = dynamic_cast<EndpointFilter*>(f.get())) {
        Event tmp;
        tmp.set_endpoint(std::string(endpoint));
        endpoint_ok = ef->pass(tmp);
      }
    }

    if (!endpoint_ok) continue;

    out.route_mask |= (1ull << i);
    if (min_priority < out.min_priority) out.min_priority = min_priority;
  }

  return out;
}

Pipeline::CacheEntry Pipeline::cached_route_mask_(std::string_view endpoint) {
  if (impl_->dirty_cache.load(std::memory_order_acquire)) {
    std::unique_lock wl(impl_->cache_m);
    if (impl_->dirty_cache.exchange(false, std::memory_order_acq_rel)) {
      impl_->endpoint_cache.clear();

      std::uint16_t min_priority = 65535;
      std::lock_guard lk(impl_->state_m);
      for (auto const& r : impl_->routes) {
        std::uint16_t route_min = severity::trace::priority();
        for (auto const& f : r.filter_list()) {
          if (auto* sf = dynamic_cast<SeverityFilter*>(f.get())) { route_min = sf->minimum_priority(); break; }
        }
        if (route_min < min_priority) min_priority = route_min;
      }
      if (impl_->routes.empty()) min_priority = 65535;
      impl_->global_min_priority.store(min_priority, std::memory_order_release);
    }
  }

  {
    std::shared_lock rl(impl_->cache_m);
    auto it = impl_->endpoint_cache.find(std::string(endpoint));
    if (it != impl_->endpoint_cache.end()) return it->second;
  }

  auto computed = compute_cache_entry_(endpoint);
  {
    std::unique_lock wl(impl_->cache_m);
    impl_->endpoint_cache.emplace(std::string(endpoint), computed);
  }
  return computed;
}

std::uint16_t Pipeline::cached_min_priority(std::string_view endpoint) {
  auto ce = cached_route_mask_(endpoint);
  if (ce.route_mask == 0) return 65535;
  return ce.min_priority;
}

void Pipeline::dispatch_sync_(const Event& e, std::uint64_t route_mask) {
  std::vector<Route> routes;
  {
    std::lock_guard lk(impl_->state_m);
    routes = impl_->routes;
  }

  std::string buffer;
  buffer.reserve(512);

  for (std::size_t i = 0; i < routes.size() && i < 64; ++i) {
    if (((route_mask >> i) & 1ull) == 0) continue;

    bool ok = true;
    for (auto const& f : routes[i].filter_list()) {
      if (f && !f->pass(e)) { ok = false; break; }
    }
    if (!ok) continue;

    buffer.clear();
    if (routes[i].renderer_ref()) routes[i].renderer_ref()->render(e, buffer);
    else {
      buffer = e.message();
      if (!buffer.empty() && buffer.back() != '\n') buffer.push_back('\n');
    }

    if (routes[i].sink_ref()) routes[i].sink_ref()->write(buffer);
  }
}

void Pipeline::emergency_fallback_write_(std::string_view bytes) noexcept {
  std::fwrite(bytes.data(), 1, bytes.size(), stderr);
  std::fflush(stderr);
}

void Pipeline::submit(Event e, bool force_sync) {
  auto ce = cached_route_mask_(e.endpoint());
  if (ce.route_mask == 0) {
    if (e.severity_priority() >= severity::error::priority()) {
      std::string tmp = std::format("[sylog:fallback] {} {}: {}\n", PatternRenderer::format_time(e.timestamp()), e.severity_name(), e.message());
      emergency_fallback_write_(tmp);
    }
    return;
  }

  {
    std::lock_guard lk(impl_->state_m);
    for (auto const& f : impl_->global_filters) {
      if (f && !f->pass(e)) return;
    }
  }

  const bool async_mode = impl_->cfg.async_by_default() && !force_sync && !(impl_->cfg.fatal_sync() && e.severity_priority() >= severity::fatal::priority());
  if (!async_mode) {
    dispatch_sync_(e, ce.route_mask);
    return;
  }

  std::shared_ptr<detail::SinkCrossbar> crossbar;
  {
    std::lock_guard lk(impl_->worker_m);
    crossbar = impl_->crossbar;
  }

  if (!impl_->accepting.load(std::memory_order_acquire) || !crossbar) {
    dispatch_sync_(e, ce.route_mask);
    return;
  }

  (void)crossbar->submit(std::move(e), ce.route_mask, impl_->accepting);
}

void Pipeline::worker_loop_(std::stop_token) {}

Logger::Logger(std::string endpoint) : endpoint_(std::move(endpoint)) {}

} // namespace sylog
