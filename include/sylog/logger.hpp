#pragma once

#include "sylog/delivery.hpp"
#include "sylog/event.hpp"
#include "sylog/export.hpp"
#include "sylog/renderer.hpp"
#include "sylog/severity.hpp"
#include "sylog/sink.hpp"

#include <atomic>
#include <format>
#include <memory>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <stop_token>
#include <unordered_map>
#include <vector>

namespace sylog {

class SYLOG_API IFilter {
public:
  virtual ~IFilter() = default;
  virtual bool pass(const Event& e) const = 0;
};

class SYLOG_API SeverityFilter : public IFilter {
public:
  explicit SeverityFilter(std::uint16_t minimum_priority);

  template <Severity Minimum>
  static std::shared_ptr<SeverityFilter> at_least() {
    return std::make_shared<SeverityFilter>(Minimum::priority());
  }

  std::uint16_t minimum_priority() const noexcept { return minimum_priority_; }
  bool pass(const Event& e) const override;

private:
  std::uint16_t minimum_priority_{0};
};

class SYLOG_API EndpointFilter : public IFilter {
public:
  EndpointFilter& allow(std::string prefix);
  EndpointFilter& deny(std::string prefix);

  bool pass(const Event& e) const override;
  static bool matches_prefix(std::string_view ep, std::string_view prefix);

private:
  std::vector<std::string> allow_prefixes_;
  std::vector<std::string> deny_prefixes_;
};

class Route {
public:
  Route() = default;

  Route& send_to(std::shared_ptr<ISink> s) { sink_ = std::move(s); return *this; }
  Route& render_with(std::shared_ptr<IRenderer> r) { renderer_ = std::move(r); return *this; }
  Route& filtered_by(std::shared_ptr<IFilter> f) { filters_.push_back(std::move(f)); return *this; }

  template <delivery::Strategy Strategy>
  Route& deliver_with(Strategy strategy = {}) {
    delivery_strategy_ = std::make_shared<Strategy>(std::move(strategy));
    return *this;
  }

  Route& drop_when_full() { delivery_strategy_ = delivery::dropping_when_full(); return *this; }
  Route& block_when_full() { delivery_strategy_ = delivery::blocking_when_full(); return *this; }

  std::shared_ptr<ISink> sink_ref() const noexcept { return sink_; }
  std::shared_ptr<IRenderer> renderer_ref() const noexcept { return renderer_; }
  const std::vector<std::shared_ptr<IFilter>>& filter_list() const noexcept { return filters_; }
  std::shared_ptr<delivery::strategy_base> delivery() const noexcept { return delivery_strategy_; }

private:
  std::shared_ptr<ISink> sink_;
  std::shared_ptr<IRenderer> renderer_;
  std::vector<std::shared_ptr<IFilter>> filters_;
  std::shared_ptr<delivery::strategy_base> delivery_strategy_{delivery::dropping_when_full()};
};

class Config {
public:
  Config() = default;

  static Config asynchronous(std::size_t queue_capacity = (1u << 16)) {
    Config cfg;
    cfg.async_by_default(true).queue_capacity(queue_capacity);
    return cfg;
  }

  static Config synchronous() {
    Config cfg;
    cfg.async_by_default(false);
    return cfg;
  }

  Config& async_by_default(bool value) noexcept { async_by_default_ = value; return *this; }
  Config& queue_capacity(std::size_t value) noexcept { queue_capacity_ = value; return *this; }
  Config& fatal_sync(bool value) noexcept { fatal_sync_ = value; return *this; }

  bool async_by_default() const noexcept { return async_by_default_; }
  std::size_t queue_capacity() const noexcept { return queue_capacity_; }
  bool fatal_sync() const noexcept { return fatal_sync_; }

private:
  bool async_by_default_{true};
  std::size_t queue_capacity_{1u << 16};
  bool fatal_sync_{true};
};

class SYLOG_API Pipeline {
public:
  static Pipeline& instance();

  void configure(Config cfg);
  void add_global_filter(std::shared_ptr<IFilter> f);
  void clear_global_filters();
  void add_route(Route r);
  void clear_routes();
  void start();
  void close();
  void request_stop();
  void flush();
  bool is_running() const noexcept;

  std::uint16_t cached_min_priority(std::string_view endpoint);
  void submit(Event e, bool force_sync);

private:
  Pipeline();
  ~Pipeline();
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  class CacheEntry {
  public:
    std::uint16_t min_priority{65535};
    std::uint64_t route_mask{0};
  };

  void dispatch_sync_(const Event& e, std::uint64_t route_mask);
  void worker_loop_(std::stop_token st);
  CacheEntry compute_cache_entry_(std::string_view endpoint);
  CacheEntry cached_route_mask_(std::string_view endpoint);
  void invalidate_cache_();
  static void emergency_fallback_write_(std::string_view bytes) noexcept;

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

class SYLOG_API Logger {
public:
  explicit Logger(std::string endpoint);

  std::string_view endpoint() const noexcept { return endpoint_; }

  template <Severity Sev>
  bool enabled() const {
    return Sev::priority() >= Pipeline::instance().cached_min_priority(endpoint_);
  }

  template <Severity Sev, class... Args>
  void log(std::format_string<Args...> fmt, Args&&... args) {
    log_at<Sev>(std::source_location::current(), fmt, std::forward<Args>(args)...);
  }

  template <Severity Sev, class... Args>
  void log_at(std::source_location loc,
              std::format_string<Args...> fmt,
              Args&&... args) {
    if (!enabled<Sev>()) return;
    Event e = make_event_<Sev>(loc, std::format(fmt, std::forward<Args>(args)...));
    Pipeline::instance().submit(std::move(e), Sev::force_sync());
    if constexpr (Sev::flush_after()) Pipeline::instance().flush();
  }

  template <Severity Sev, class... Args>
  void log_fields(std::span<const Field> fields,
                  std::format_string<Args...> fmt,
                  Args&&... args) {
    log_fields_at<Sev>(fields, std::source_location::current(), fmt, std::forward<Args>(args)...);
  }

  template <Severity Sev, class... Args>
  void log_fields_at(std::span<const Field> fields,
                     std::source_location loc,
                     std::format_string<Args...> fmt,
                     Args&&... args) {
    if (!enabled<Sev>()) return;
    Event e = make_event_<Sev>(loc, std::format(fmt, std::forward<Args>(args)...));
    e.set_fields(fields);
    Pipeline::instance().submit(std::move(e), Sev::force_sync());
    if constexpr (Sev::flush_after()) Pipeline::instance().flush();
  }

  template <class... Args> void trace(std::format_string<Args...> f, Args&&... a) { log<severity::trace>(f, std::forward<Args>(a)...); }
  template <class... Args> void debug(std::format_string<Args...> f, Args&&... a) { log<severity::debug>(f, std::forward<Args>(a)...); }
  template <class... Args> void info(std::format_string<Args...> f, Args&&... a) { log<severity::info>(f, std::forward<Args>(a)...); }
  template <class... Args> void warn(std::format_string<Args...> f, Args&&... a) { log<severity::warn>(f, std::forward<Args>(a)...); }
  template <class... Args> void error(std::format_string<Args...> f, Args&&... a) { log<severity::error>(f, std::forward<Args>(a)...); }
  template <class... Args> void fatal(std::format_string<Args...> f, Args&&... a) { log<severity::fatal>(f, std::forward<Args>(a)...); }

  template <class... Args> void trace_at(std::source_location loc, std::format_string<Args...> f, Args&&... a) { log_at<severity::trace>(loc, f, std::forward<Args>(a)...); }
  template <class... Args> void debug_at(std::source_location loc, std::format_string<Args...> f, Args&&... a) { log_at<severity::debug>(loc, f, std::forward<Args>(a)...); }
  template <class... Args> void info_at(std::source_location loc, std::format_string<Args...> f, Args&&... a) { log_at<severity::info>(loc, f, std::forward<Args>(a)...); }
  template <class... Args> void warn_at(std::source_location loc, std::format_string<Args...> f, Args&&... a) { log_at<severity::warn>(loc, f, std::forward<Args>(a)...); }
  template <class... Args> void error_at(std::source_location loc, std::format_string<Args...> f, Args&&... a) { log_at<severity::error>(loc, f, std::forward<Args>(a)...); }
  template <class... Args> void fatal_at(std::source_location loc, std::format_string<Args...> f, Args&&... a) { log_at<severity::fatal>(loc, f, std::forward<Args>(a)...); }

private:
  template <Severity Sev>
  Event make_event_(std::source_location loc, std::string message) const {
    Event e;
    e.set_timestamp(std::chrono::system_clock::now());
    e.set_severity(Sev::name(), Sev::priority());
    e.set_endpoint(endpoint_);
    e.set_source(loc);
    e.set_thread_id(std::this_thread::get_id());
    e.set_message(std::move(message));
    return e;
  }

  std::string endpoint_;
};
} // namespace sylog
