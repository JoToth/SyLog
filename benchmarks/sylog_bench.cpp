#include "sylog/sylog.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <string>
#include <string_view>
#include <system_error>

#include <argtree/argtree.hpp>
#include <thread>
#include <vector>

namespace {

using clock_type = std::chrono::steady_clock;
using ns = std::chrono::nanoseconds;

struct CountingSink final : sylog::ISink {
  std::atomic<std::uint64_t> writes{0};
  std::atomic<std::uint64_t> bytes{0};

  void write(std::string_view s) override {
    writes.fetch_add(1, std::memory_order_relaxed);
    bytes.fetch_add(static_cast<std::uint64_t>(s.size()), std::memory_order_relaxed);
  }
  void flush() override {}
};

struct SlowCountingSink final : sylog::ISink {
  std::chrono::microseconds delay;
  std::atomic<std::uint64_t> writes{0};
  std::atomic<std::uint64_t> bytes{0};

  explicit SlowCountingSink(std::chrono::microseconds d) : delay(d) {}

  void write(std::string_view s) override {
    if (delay.count() > 0) std::this_thread::sleep_for(delay);
    writes.fetch_add(1, std::memory_order_relaxed);
    bytes.fetch_add(static_cast<std::uint64_t>(s.size()), std::memory_order_relaxed);
  }
  void flush() override {}
};

struct CountingFileSink final : sylog::ISink {
  std::mutex m;
  std::ofstream out;
  std::atomic<std::uint64_t> writes{0};
  std::atomic<std::uint64_t> bytes{0};

  explicit CountingFileSink(const std::filesystem::path& path) : out(path, std::ios::out | std::ios::trunc) {}

  void write(std::string_view s) override {
    std::lock_guard lk(m);
    out.write(s.data(), static_cast<std::streamsize>(s.size()));
    writes.fetch_add(1, std::memory_order_relaxed);
    bytes.fetch_add(static_cast<std::uint64_t>(s.size()), std::memory_order_relaxed);
  }
  void flush() override {
    std::lock_guard lk(m);
    out.flush();
  }
};

struct Options {
  std::string scenario{"all"};
  std::uint64_t events{100000};
  int producers{4};
  std::size_t queue_capacity{1u << 14};
  int slow_us{100};
  std::string file_path{"sylog_bench.log"};
};

struct Timings {
  std::mutex m;
  std::vector<std::uint64_t> enqueue_ns;

  void reserve(std::uint64_t n) {
    const auto limit = std::min<std::uint64_t>(n, 200000u);
    enqueue_ns.reserve(static_cast<std::size_t>(limit));
  }

  void record(ns d) {
    // Sampling keeps high-volume runs from becoming a vector benchmark.
    std::lock_guard lk(m);
    if (enqueue_ns.size() < enqueue_ns.capacity()) {
      enqueue_ns.push_back(static_cast<std::uint64_t>(d.count()));
    }
  }
};

struct ScenarioResult {
  std::string name;
  std::string sink;
  std::string renderer;
  std::string policy;
  int producers{};
  std::size_t queue_capacity{};
  std::uint64_t produced{};
  std::uint64_t consumed{};
  std::uint64_t dropped_or_filtered{};
  std::uint64_t bytes{};
  double enqueue_seconds{};
  double total_seconds{};
  double drain_seconds{};
  std::uint64_t p50_ns{};
  std::uint64_t p95_ns{};
  std::uint64_t p99_ns{};
};

std::uint64_t percentile(std::vector<std::uint64_t> values, double p) {
  if (values.empty()) return 0;
  std::sort(values.begin(), values.end());
  const auto idx = static_cast<std::size_t>(std::clamp(p, 0.0, 1.0) * static_cast<double>(values.size() - 1));
  return values[idx];
}


struct CliState {
  std::string_view scenario{"all"};
  std::uint64_t events{100000};
  int producers{4};
  std::size_t queue_capacity{1u << 14};
  int slow_us{100};
  std::string_view file_path{"sylog_bench.log"};
  std::string_view error{};
};

bool parse_u64(std::string_view s, std::uint64_t& out) noexcept {
  if (s.empty()) return false;
  const auto* first = s.data();
  const auto* last = s.data() + s.size();
  std::uint64_t value{};
  const auto [ptr, ec] = std::from_chars(first, last, value);
  if (ec != std::errc{} || ptr != last) return false;
  out = value;
  return true;
}

void set_error(CliState& st, std::string_view message) noexcept {
  if (st.error.empty()) st.error = message;
}

void set_scenario(CliState& st, std::string_view value) noexcept { st.scenario = value; }
void set_file(CliState& st, std::string_view value) noexcept { st.file_path = value; }

void set_events(CliState& st, std::string_view value) noexcept {
  std::uint64_t parsed{};
  if (!parse_u64(value, parsed)) return set_error(st, "--events expects an unsigned integer");
  st.events = std::max<std::uint64_t>(1, parsed);
}

void set_producers(CliState& st, std::string_view value) noexcept {
  std::uint64_t parsed{};
  if (!parse_u64(value, parsed)) return set_error(st, "--producers expects an unsigned integer");
  st.producers = static_cast<int>(std::max<std::uint64_t>(1, parsed));
}

void set_capacity(CliState& st, std::string_view value) noexcept {
  std::uint64_t parsed{};
  if (!parse_u64(value, parsed)) return set_error(st, "--capacity expects an unsigned integer");
  st.queue_capacity = static_cast<std::size_t>(std::max<std::uint64_t>(1, parsed));
}

void set_slow_us(CliState& st, std::string_view value) noexcept {
  std::uint64_t parsed{};
  if (!parse_u64(value, parsed)) return set_error(st, "--slow-us expects an unsigned integer");
  st.slow_us = static_cast<int>(parsed);
}

using BenchCli = argtree::cx::schema<CliState>;

const auto benchmark_cli = BenchCli::root(
    BenchCli::custom_option("--scenario", true, &set_scenario,
                            "Scenario: all|null|null-format|file|slow|burst"),
    BenchCli::custom_option("--events", true, &set_events,
                            "Total events to produce per scenario"),
    BenchCli::custom_option("--producers", true, &set_producers,
                            "Producer thread count"),
    BenchCli::custom_option("--capacity", true, &set_capacity,
                            "Async queue capacity"),
    BenchCli::custom_option("--slow-us", true, &set_slow_us,
                            "Slow sink delay per write, in microseconds"),
    BenchCli::custom_option("--file", true, &set_file,
                            "Output file path for the file scenario"));

void print_benchmark_notes(std::ostream& os) {
  os << "\nNotes:\n"
     << "  produced is the number of logger calls. consumed is sink writes observed after close().\n"
     << "  dropped_or_filtered is produced-consumed; in drop scenarios this approximates backpressure loss.\n"
     << "  enqueue p50/p95/p99 measures producer-side call latency samples, not raw worker throughput.\n";
}

struct ParsedOptions {
  Options options{};
  bool should_exit{false};
  int exit_code{0};
};

ParsedOptions parse_args(int argc, char** argv) {
  auto parsed = benchmark_cli.parse(argc, argv);

  switch (parsed.status()) {
    case argtree::cx::parse_status::ok:
      break;
    case argtree::cx::parse_status::help_current:
    case argtree::cx::parse_status::help_subtree:
      benchmark_cli.print_help(std::cout, parsed, argc > 0 ? std::string_view{argv[0]} : std::string_view{"SyLogBenchmarks"});
      print_benchmark_notes(std::cout);
      return {.should_exit = true, .exit_code = 0};
    case argtree::cx::parse_status::error:
      benchmark_cli.print_error(std::cerr, parsed, argc > 0 ? std::string_view{argv[0]} : std::string_view{"SyLogBenchmarks"});
      return {.should_exit = true, .exit_code = 2};
  }

  if (!parsed.state.error.empty()) {
    std::cerr << "parse error: " << parsed.state.error << "\n";
    return {.should_exit = true, .exit_code = 2};
  }

  Options opt;
  opt.scenario = std::string(parsed.state.scenario);
  opt.events = parsed.state.events;
  opt.producers = parsed.state.producers;
  opt.queue_capacity = parsed.state.queue_capacity;
  opt.slow_us = parsed.state.slow_us;
  opt.file_path = std::string(parsed.state.file_path);
  return {.options = std::move(opt), .should_exit = false, .exit_code = 0};
}

std::shared_ptr<sylog::IRenderer> renderer_for(std::string_view kind) {
  if (kind == "none") return nullptr;
  if (kind == "full-pattern") {
    return std::make_shared<sylog::PatternRenderer>("{time} {severity} {endpoint} {tid} {file}:{line} {message}\n");
  }
  return std::make_shared<sylog::PatternRenderer>("{message}\n");
}

template <class SinkPtr>
ScenarioResult run_scenario(const Options& opt,
                            std::string name,
                            SinkPtr sink,
                            std::string sink_name,
                            std::string renderer_name,
                            std::shared_ptr<sylog::delivery::strategy_base> policy,
                            bool burst_mode) {
  sylog::Pipeline::instance().request_stop();
  sylog::Pipeline::instance().clear_routes();
  sylog::Pipeline::instance().clear_global_filters();
  sylog::Pipeline::instance().configure(sylog::Config::asynchronous(opt.queue_capacity));

  sylog::Route route;
  route.send_to(sink);
  route.render_with(renderer_for(renderer_name));
  const bool blocking_policy = policy && policy->name() == "block_when_full";
  if (blocking_policy) route.block_when_full();
  else route.drop_when_full();
  sylog::Pipeline::instance().add_route(route);
  sylog::Pipeline::instance().start();

  Timings timings;
  timings.reserve(opt.events);
  std::atomic<std::uint64_t> produced{0};
  std::atomic<bool> go{false};
  std::vector<std::thread> threads;
  threads.reserve(static_cast<std::size_t>(opt.producers));

  const auto begin_total = clock_type::now();
  const auto begin_enqueue = clock_type::now();

  for (int p = 0; p < opt.producers; ++p) {
    threads.emplace_back([&, p] {
      sylog::Logger log("bench.worker" + std::to_string(p));
      while (!go.load(std::memory_order_acquire)) std::this_thread::yield();

      for (;;) {
        const auto idx = produced.fetch_add(1, std::memory_order_acq_rel);
        if (idx >= opt.events) break;

        const auto t0 = clock_type::now();
        if (renderer_name == "full-pattern") {
          log.info("event={} producer={} payload={} status={}", idx, p, "abcdefghijklmnopqrstuvwxyz", "ok");
        } else if (burst_mode) {
          log.info("burst event {}", idx);
        } else {
          log.info("message {}", idx);
        }
        const auto t1 = clock_type::now();
        timings.record(std::chrono::duration_cast<ns>(t1 - t0));
      }
    });
  }

  go.store(true, std::memory_order_release);
  for (auto& t : threads) t.join();
  const auto end_enqueue = clock_type::now();

  const auto begin_drain = clock_type::now();
  sylog::Pipeline::instance().close();
  const auto end_total = clock_type::now();

  const auto consumed = sink->writes.load(std::memory_order_acquire);
  const auto bytes = sink->bytes.load(std::memory_order_acquire);

  ScenarioResult result;
  result.name = std::move(name);
  result.sink = std::move(sink_name);
  result.renderer = std::string(renderer_name);
  result.policy = blocking_policy ? "block" : "drop";
  result.producers = opt.producers;
  result.queue_capacity = opt.queue_capacity;
  result.produced = opt.events;
  result.consumed = consumed;
  result.dropped_or_filtered = (opt.events >= consumed) ? (opt.events - consumed) : 0;
  result.bytes = bytes;
  result.enqueue_seconds = std::chrono::duration<double>(end_enqueue - begin_enqueue).count();
  result.total_seconds = std::chrono::duration<double>(end_total - begin_total).count();
  result.drain_seconds = std::chrono::duration<double>(end_total - begin_drain).count();
  result.p50_ns = percentile(timings.enqueue_ns, 0.50);
  result.p95_ns = percentile(timings.enqueue_ns, 0.95);
  result.p99_ns = percentile(timings.enqueue_ns, 0.99);
  return result;
}

void print_result(const ScenarioResult& r) {
  const auto enqueue_eps = static_cast<double>(r.produced) / std::max(0.000001, r.enqueue_seconds);
  const auto consumed_eps = static_cast<double>(r.consumed) / std::max(0.000001, r.total_seconds);

  std::cout << "\nscenario=" << r.name << "\n"
            << "  backend=Workers\n"
            << "  sink=" << r.sink << " renderer=" << r.renderer << " policy=" << r.policy << "\n"
            << "  producers=" << r.producers << " queue_capacity=" << r.queue_capacity << "\n"
            << "  produced=" << r.produced << " consumed=" << r.consumed
            << " dropped_or_filtered=" << r.dropped_or_filtered << " bytes=" << r.bytes << "\n"
            << std::fixed << std::setprecision(6)
            << "  enqueue_seconds=" << r.enqueue_seconds
            << " total_seconds=" << r.total_seconds
            << " drain_seconds=" << r.drain_seconds << "\n"
            << std::setprecision(2)
            << "  producer_call_rate_per_sec=" << enqueue_eps
            << " end_to_end_consumed_per_sec=" << consumed_eps << "\n"
            << "  enqueue_latency_ns p50=" << r.p50_ns
            << " p95=" << r.p95_ns
            << " p99=" << r.p99_ns << "\n";
}

std::vector<std::string> expand_scenarios(const std::string& requested) {
  if (requested == "all") return {"null", "null-format", "file", "slow", "burst"};
  return {requested};
}

} // namespace

int main(int argc, char** argv) {
  const auto parsed = parse_args(argc, argv);
  if (parsed.should_exit) return parsed.exit_code;
  const auto& opt = parsed.options;
  const auto scenarios = expand_scenarios(opt.scenario);

  for (const auto& scenario : scenarios) {
    if (scenario == "null") {
      print_result(run_scenario(opt, "enqueue-only-null", std::make_shared<CountingSink>(),
                                "null", "none", sylog::delivery::blocking_when_full(), false));
    } else if (scenario == "null-format") {
      print_result(run_scenario(opt, "formatting-null", std::make_shared<CountingSink>(),
                                "null", "full-pattern", sylog::delivery::blocking_when_full(), false));
    } else if (scenario == "file") {
      std::filesystem::remove(opt.file_path);
      print_result(run_scenario(opt, "file-sink", std::make_shared<CountingFileSink>(opt.file_path),
                                "file", "message", sylog::delivery::blocking_when_full(), false));
    } else if (scenario == "slow") {
      print_result(run_scenario(opt, "slow-sink-drop-pressure",
                                std::make_shared<SlowCountingSink>(std::chrono::microseconds(opt.slow_us)),
                                "slow", "message", sylog::delivery::dropping_when_full(), false));
    } else if (scenario == "burst") {
      print_result(run_scenario(opt, "burst-drain-block", std::make_shared<CountingSink>(),
                                "null", "message", sylog::delivery::blocking_when_full(), true));
    } else {
      std::cerr << "unknown scenario: " << scenario << "\n";
      benchmark_cli.print_help(std::cerr, benchmark_cli.parse(0, static_cast<char**>(nullptr)), argc > 0 ? std::string_view{argv[0]} : std::string_view{"SyLogBenchmarks"});
      return 2;
    }
  }

  return 0;
}
