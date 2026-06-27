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

class FiltersSuite final : public Test::Suite {
public:
  FiltersSuite() {
    TEST_ADD(FiltersSuite::endpoint_filter_allow_and_deny_prefixes);
    TEST_ADD(FiltersSuite::global_filters_apply_to_all_routes);
  }

private:
  void endpoint_filter_allow_and_deny_prefixes() {
    using namespace sylog;

    Pipeline::instance().close();
    Pipeline::instance().clear_routes();
    Pipeline::instance().clear_global_filters();
    Pipeline::instance().configure(Config::synchronous().queue_capacity(1024));

    auto mem = std::make_shared<MemSink>();

    auto epf = std::make_shared<EndpointFilter>();
    epf->allow("net.").allow("ui.");
    epf->deny("net.secret");

    Route r;
    r.send_to(mem);
    r.render_with(std::make_shared<PatternRenderer>("{endpoint}:{message}\n"));
    r.filtered_by(epf);
    Pipeline::instance().add_route(r);

    Pipeline::instance().start();

    Logger net("net.main");
    Logger ui("ui.window");
    Logger secret("net.secret.module");
    Logger other("db.core");

    net.info("A");
    ui.info("B");
    secret.info("C");
    other.info("D");

    Pipeline::instance().close();

    std::lock_guard lk(mem->m);
    std::string all;
    for (auto const& l : mem->lines) all += l;

    TEST_ASSERT(all.find("net.main:A") != std::string::npos);
    TEST_ASSERT(all.find("ui.window:B") != std::string::npos);
    TEST_ASSERT(all.find("net.secret.module:C") == std::string::npos);
    TEST_ASSERT(all.find("db.core:D") == std::string::npos);
  }

  void global_filters_apply_to_all_routes() {
    using namespace sylog;

    Pipeline::instance().close();
    Pipeline::instance().clear_routes();
    Pipeline::instance().clear_global_filters();
    Pipeline::instance().configure(Config::synchronous().queue_capacity(1024));

    auto mem = std::make_shared<MemSink>();

    Route r;
    r.send_to(mem);
    r.render_with(std::make_shared<PatternRenderer>("{severity}:{message}\n"));
    Pipeline::instance().add_route(r);

    // Global: allow only warn+
    Pipeline::instance().add_global_filter(std::make_shared<SeverityFilter>(severity::warn::priority()));

    Pipeline::instance().start();

    Logger log("x");
    log.info("i");
    log.warn("w");
    log.error("e");

    Pipeline::instance().close();

    std::lock_guard lk(mem->m);
    std::string all;
    for (auto const& l : mem->lines) all += l;

    TEST_ASSERT(all.find("INFO:i") == std::string::npos);
    TEST_ASSERT(all.find("WARN:w") != std::string::npos);
    TEST_ASSERT(all.find("ERROR:e") != std::string::npos);
  }
};

} // namespace

std::unique_ptr<Test::Suite> make_filters_suite() {
  return std::make_unique<FiltersSuite>();
}
