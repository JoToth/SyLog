#include <cpptest.h>

#include "sylog/sylog.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

struct MemSink final : sylog::ISink {
  std::mutex m;
  std::vector<std::string> lines;

  void write(std::string_view bytes) override {
    std::lock_guard lk(m);
    lines.emplace_back(bytes);
  }
  void flush() override {}
};

class AsyncSuite final : public Test::Suite {
public:
  AsyncSuite() {
    TEST_ADD(AsyncSuite::async_basic_routing_and_level_filter);
  }

private:
  void async_basic_routing_and_level_filter() {
    using namespace sylog;

    Pipeline::instance().close();
    Pipeline::instance().clear_routes();
    Pipeline::instance().clear_global_filters();
    Pipeline::instance().configure(Config::asynchronous(1u << 12));

    auto mem = std::make_shared<MemSink>();

    Route r;
    r.send_to(mem);
    r.render_with(std::make_shared<PatternRenderer>("[{severity}] {message}\n"));
    r.filtered_by(std::make_shared<SeverityFilter>(severity::info::priority()));
    Pipeline::instance().add_route(r);

    Pipeline::instance().start();

    Logger log("t");
    log.debug("skip");
    log.info("hello {}", 1);
    log.warn("warn {}", 2);

    Pipeline::instance().close();

    std::lock_guard lk(mem->m);
    TEST_ASSERT(mem->lines.size() >= 2);
    for (auto const& s : mem->lines) {
      TEST_ASSERT(s.find("skip") == std::string::npos);
    }
  }
};

} // namespace

std::unique_ptr<Test::Suite> make_async_suite() {
  return std::make_unique<AsyncSuite>();
}
