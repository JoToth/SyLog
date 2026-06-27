#include <cpptest.h>

#include "sylog/sylog.hpp"
#include "test_helpers.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

struct MemSink final : sylog::ISink {
  mutable std::mutex m;
  std::vector<std::string> lines;

  void write(std::string_view bytes) override {
    std::lock_guard lk(m);
    lines.emplace_back(bytes);
  }

  void flush() override {}

  std::size_t size() const {
    std::lock_guard lk(m);
    return lines.size();
  }
};

struct SlowSink final : sylog::ISink {
  explicit SlowSink(std::chrono::milliseconds delay_) : delay(delay_) {}

  std::chrono::milliseconds delay;
  mutable std::mutex m;
  std::size_t writes{0};

  void write(std::string_view) override {
    std::this_thread::sleep_for(delay);
    std::lock_guard lk(m);
    ++writes;
  }

  void flush() override {}

  std::size_t size() const {
    std::lock_guard lk(m);
    return writes;
  }
};

void set_delivery(sylog::Route& r, std::shared_ptr<sylog::delivery::strategy_base> strategy) {
  if (strategy && strategy->name() == "block_when_full") r.block_when_full();
  else r.drop_when_full();
}

class CrossbarSinkSuite final : public Test::Suite {
public:
  CrossbarSinkSuite() {
    TEST_ADD(CrossbarSinkSuite::fanout_delivers_to_multiple_routes);
    TEST_ADD(CrossbarSinkSuite::slow_dropping_sink_does_not_starve_fast_sink);
  }

private:
  void reset_pipeline(std::size_t capacity) {
    using namespace sylog;
    Pipeline::instance().request_stop();
    Pipeline::instance().clear_routes();
    Pipeline::instance().clear_global_filters();
    Pipeline::instance().configure(Config::asynchronous(capacity));
  }

  void fanout_delivers_to_multiple_routes() {
    using namespace sylog;

    reset_pipeline(128);

    auto a = std::make_shared<MemSink>();
    auto b = std::make_shared<MemSink>();

    Route ra;
    ra.send_to(a);
    ra.render_with(std::make_shared<PatternRenderer>("A:{message}\n"));
    set_delivery(ra, delivery::blocking_when_full());

    Route rb;
    rb.send_to(b);
    rb.render_with(std::make_shared<PatternRenderer>("B:{message}\n"));
    set_delivery(rb, delivery::blocking_when_full());

    Pipeline::instance().add_route(ra);
    Pipeline::instance().add_route(rb);
    Pipeline::instance().start();

    Logger log("crossbar.fanout");
    for (int i = 0; i < 50; ++i) log.info("event {}", i);

    Pipeline::instance().close();

    TEST_ASSERT(a->size() == 50);
    TEST_ASSERT(b->size() == 50);
  }

  void slow_dropping_sink_does_not_starve_fast_sink() {
    using namespace sylog;

    reset_pipeline(4);

    auto slow = std::make_shared<SlowSink>(std::chrono::milliseconds(25));
    auto fast = std::make_shared<MemSink>();

    Route slow_route;
    slow_route.send_to(slow);
    slow_route.render_with(std::make_shared<PatternRenderer>("S:{message}\n"));
    set_delivery(slow_route, delivery::dropping_when_full());

    Route fast_route;
    fast_route.send_to(fast);
    fast_route.render_with(std::make_shared<PatternRenderer>("F:{message}\n"));
    set_delivery(fast_route, delivery::blocking_when_full());

    Pipeline::instance().add_route(slow_route);
    Pipeline::instance().add_route(fast_route);
    Pipeline::instance().start();

    Logger log("crossbar.isolation");
    for (int i = 0; i < 100; ++i) log.info("event {}", i);

    SYLOG_CHECK_EVENTUALLY(fast->size() >= 80,
                           std::chrono::seconds(2),
                           "fast sink should progress independently of the slow dropping sink");

    Pipeline::instance().request_stop();

    TEST_ASSERT(fast->size() >= 80);
    TEST_ASSERT(slow->size() < fast->size());
  }
};

} // namespace

std::unique_ptr<Test::Suite> make_crossbar_sink_suite() {
  return std::make_unique<CrossbarSinkSuite>();
}
