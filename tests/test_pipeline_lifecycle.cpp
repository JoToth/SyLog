#include <cpptest.h>

#include "sylog/sylog.hpp"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

struct CountingSink final : sylog::ISink {
  std::mutex m;
  std::condition_variable cv;
  std::vector<std::string> lines;

  void write(std::string_view bytes) override {
    {
      std::lock_guard lk(m);
      lines.emplace_back(bytes);
    }
    cv.notify_all();
  }

  bool wait_size(std::size_t n, std::chrono::milliseconds d) {
    std::unique_lock lk(m);
    return cv.wait_for(lk, d, [&] { return lines.size() >= n; });
  }

  std::size_t size() const {
    auto* self = const_cast<CountingSink*>(this);
    std::lock_guard lk(self->m);
    return lines.size();
  }

  void flush() override {}
};

class PipelineLifecycleSuite : public Test::Suite {
public:
  PipelineLifecycleSuite() {
    TEST_ADD(PipelineLifecycleSuite::close_drains_queued_events);
    TEST_ADD(PipelineLifecycleSuite::request_stop_wakes_and_allows_restart);
  }

private:
  static std::shared_ptr<CountingSink> configure_counting_pipeline(std::size_t cap) {
    using namespace sylog;
    Pipeline::instance().request_stop();
    Pipeline::instance().clear_routes();
    Pipeline::instance().clear_global_filters();
    Pipeline::instance().configure(Config::asynchronous(cap));

    auto sink = std::make_shared<CountingSink>();
    Route r;
    r.send_to(sink);
    r.render_with(std::make_shared<PatternRenderer>("{message}\n"));
            r.block_when_full();
    Pipeline::instance().add_route(r);
    return sink;
  }

  void close_drains_queued_events() {
    using namespace sylog;
    auto sink = configure_counting_pipeline(8);
    Logger log("life.close");

    log.info("a");
    log.info("b");
    log.info("c");

    Pipeline::instance().start();
    Pipeline::instance().close();

    TEST_ASSERT_MSG(sink->size() == 3, "close() should drain queued events before join");
  }

  void request_stop_wakes_and_allows_restart() {
    using namespace sylog;
    auto sink = configure_counting_pipeline(4);
    Logger log("life.stop");

    Pipeline::instance().start();
    log.info("before");
    TEST_ASSERT_MSG(sink->wait_size(1, 2s), "worker did not process initial event");

    Pipeline::instance().request_stop();
    TEST_ASSERT_MSG(!Pipeline::instance().is_running(), "request_stop() should mark pipeline stopped");

    Pipeline::instance().start();
    log.info("after");
    TEST_ASSERT_MSG(sink->wait_size(2, 2s), "pipeline did not restart after request_stop()");
    Pipeline::instance().close();
  }
};

} // namespace

std::unique_ptr<Test::Suite> make_pipeline_lifecycle_suite() {
  return std::make_unique<PipelineLifecycleSuite>();
}
