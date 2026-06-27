#include <cpptest.h>

#include "sylog/sylog.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace {


class AuditSeverity : public sylog::severity_base {
public:
  static constexpr std::string_view name() noexcept { return "AUDIT"; }
  static constexpr std::uint16_t priority() noexcept { return 450; }
};

struct MemSink final : sylog::ISink {
  std::mutex m;
  std::vector<std::string> lines;
  void write(std::string_view bytes) override {
    std::lock_guard lk(m);
    lines.emplace_back(bytes);
  }
  void flush() override {}
};

class RenderersSuite final : public Test::Suite {
public:
  RenderersSuite() {
    TEST_ADD(RenderersSuite::json_renderer_includes_fields_and_basic_keys);
    TEST_ADD(RenderersSuite::pattern_renderer_tokens_expand);
    TEST_ADD(RenderersSuite::custom_typed_severity_renders_name);
  }

private:
  void json_renderer_includes_fields_and_basic_keys() {
    using namespace sylog;

    Pipeline::instance().close();
    Pipeline::instance().clear_routes();
    Pipeline::instance().clear_global_filters();
    Pipeline::instance().configure(Config::synchronous().queue_capacity(1024));

    auto mem = std::make_shared<MemSink>();

    auto jr = std::make_shared<JsonRenderer>();
    jr->include_fields(true).include_source(true).include_thread(true);

    Route r;
    r.send_to(mem);
    r.render_with(jr);
    Pipeline::instance().add_route(r);

    Pipeline::instance().start();

    Logger log("svc.auth");
    Field f1{"user", std::string("alice")};
    Field f2{"ok", true};
    Field f3{"count", std::int64_t(7)};
    Field f4{"ratio", 0.5};
    const Field fields[] = {f1, f2, f3, f4};

    log.log_fields<severity::info>(fields, "login {}", 42);

    Pipeline::instance().close();

    std::lock_guard lk(mem->m);
    TEST_ASSERT(mem->lines.size() == 1);

    const std::string& s = mem->lines[0];
    // super-lightweight checks (no JSON parser dependency)
    TEST_ASSERT(s.find("\"level\"") != std::string::npos);
    TEST_ASSERT(s.find("\"endpoint\"") != std::string::npos);
    TEST_ASSERT(s.find("\"message\"") != std::string::npos);
    TEST_ASSERT(s.find("\"fields\"") != std::string::npos);

    TEST_ASSERT(s.find("\"user\"") != std::string::npos);
    TEST_ASSERT(s.find("alice") != std::string::npos);
    TEST_ASSERT(s.find("\"ok\"") != std::string::npos);
    TEST_ASSERT(s.find("true") != std::string::npos);
    TEST_ASSERT(s.find("\"count\"") != std::string::npos);
    TEST_ASSERT(s.find("7") != std::string::npos);
    TEST_ASSERT(s.find("\"ratio\"") != std::string::npos);
    TEST_ASSERT(s.find("0.5") != std::string::npos);
  }

  void pattern_renderer_tokens_expand() {
    using namespace sylog;

    Pipeline::instance().close();
    Pipeline::instance().clear_routes();
    Pipeline::instance().clear_global_filters();
    Pipeline::instance().configure(Config::synchronous().queue_capacity(1024));

    auto mem = std::make_shared<MemSink>();

    Route r;
    r.send_to(mem);
    r.render_with(std::make_shared<PatternRenderer>("{severity} {endpoint} {file}:{line} {func} {message}\n"));
    Pipeline::instance().add_route(r);

    Pipeline::instance().start();

    Logger log("core");
    log.warn("hello");

    Pipeline::instance().close();

    std::lock_guard lk(mem->m);
    TEST_ASSERT(mem->lines.size() == 1);
    auto const& s = mem->lines[0];
    TEST_ASSERT(s.find("WARN") != std::string::npos);
    TEST_ASSERT(s.find("core") != std::string::npos);
    TEST_ASSERT(s.find("hello") != std::string::npos);
    // file:line and func are present (best-effort)
    TEST_ASSERT(s.find(":") != std::string::npos);
  }

  void custom_typed_severity_renders_name() {
    using namespace sylog;

    Pipeline::instance().close();
    Pipeline::instance().clear_routes();
    Pipeline::instance().clear_global_filters();
    Pipeline::instance().configure(Config::synchronous().queue_capacity(1024));

    auto mem = std::make_shared<MemSink>();

    Route r;
    r.send_to(mem);
    r.render_with(std::make_shared<PatternRenderer>("{severity} {endpoint} {message}\n"));
    Pipeline::instance().add_route(r);

    Pipeline::instance().start();

    Logger log("audit");
    log.log<AuditSeverity>("user {} changed permissions", 7);

    Pipeline::instance().close();

    std::lock_guard lk(mem->m);
    TEST_ASSERT(mem->lines.size() == 1);
    auto const& line = mem->lines[0];
    TEST_ASSERT(line.find("AUDIT") != std::string::npos);
    TEST_ASSERT(line.find("audit") != std::string::npos);
    TEST_ASSERT(line.find("changed permissions") != std::string::npos);
  }
};

} // namespace

std::unique_ptr<Test::Suite> make_renderers_suite() {
  return std::make_unique<RenderersSuite>();
}
