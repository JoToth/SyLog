#include <cpptest.h>

#include "sylog/sylog.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

struct CountingSink final : sylog::ISink {
  std::atomic<std::size_t> writes{0};
  void write(std::string_view) override { writes.fetch_add(1, std::memory_order_acq_rel); }
  void flush() override {}
};

class PipelineStressSuite final : public Test::Suite {
public:
  PipelineStressSuite() {
    TEST_ADD(PipelineStressSuite::many_producers_drain_on_close);
    TEST_ADD(PipelineStressSuite::request_stop_returns_promptly_under_load);
  }

private:
  void many_producers_drain_on_close() {
    using namespace sylog;

    Pipeline::instance().request_stop();
    Pipeline::instance().clear_routes();
    Pipeline::instance().clear_global_filters();
    Pipeline::instance().configure(Config::asynchronous(256));

    auto sink = std::make_shared<CountingSink>();
    Route r;
    r.send_to(sink);
    r.render_with(std::make_shared<PatternRenderer>("{message}\n"));
    r.block_when_full();
    Pipeline::instance().add_route(r);
    Pipeline::instance().start();

    constexpr int producers = 4;
    constexpr int per_producer = 1000;
    std::vector<std::thread> threads;
    threads.reserve(producers);

    for (int p = 0; p < producers; ++p) {
      threads.emplace_back([p] {
        sylog::Logger log("stress.block");
        for (int i = 0; i < per_producer; ++i) log.info("p{}:{}", p, i);
      });
    }

    for (auto& t : threads) t.join();
    Pipeline::instance().close();

    TEST_ASSERT_MSG(sink->writes.load(std::memory_order_acquire) ==
                        static_cast<std::size_t>(producers * per_producer),
                    "close() should drain all block-policy events");
  }

  void request_stop_returns_promptly_under_load() {
    using namespace sylog;

    Pipeline::instance().request_stop();
    Pipeline::instance().clear_routes();
    Pipeline::instance().clear_global_filters();
    Pipeline::instance().configure(Config::asynchronous(32));

    auto sink = std::make_shared<CountingSink>();
    Route r;
    r.send_to(sink);
    r.render_with(std::make_shared<PatternRenderer>("{message}\n"));
    r.block_when_full();
    Pipeline::instance().add_route(r);
    Pipeline::instance().start();

    std::atomic<bool> run{true};
    std::vector<std::thread> threads;
    for (int p = 0; p < 4; ++p) {
      threads.emplace_back([&run, p] {
        sylog::Logger log("stress.stop");
        int i = 0;
        while (run.load(std::memory_order_acquire)) log.info("p{}:{}", p, i++);
      });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::jthread watchdog([&](std::stop_token st) {
      std::this_thread::sleep_for(std::chrono::seconds(5));
      if (!st.stop_requested()) {
        run.store(false, std::memory_order_release);
        Pipeline::instance().request_stop();
      }
    });

    const auto start = std::chrono::steady_clock::now();
    run.store(false, std::memory_order_release);
    Pipeline::instance().request_stop();
    for (auto& t : threads) t.join();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    watchdog.request_stop();

    TEST_ASSERT_MSG(elapsed < std::chrono::seconds(2), "request_stop() should wake blocked producers promptly");
  }
};

} // namespace

std::unique_ptr<Test::Suite> make_pipeline_stress_suite() {
  return std::make_unique<PipelineStressSuite>();
}
