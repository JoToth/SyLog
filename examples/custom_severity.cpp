#include "sylog/sylog.hpp"

#include <cstdint>
#include <memory>
#include <utility>
#include <string_view>

class audit : public sylog::severity_base {
public:
  static constexpr std::string_view name() noexcept { return "AUDIT"; }
  static constexpr std::uint16_t priority() noexcept { return 450; }
};

int main() {
  namespace sl = sylog;

  auto console = std::make_shared<sl::ConsoleSink>();

  sl::Pipeline::instance().configure(sl::Config::synchronous());

  sl::Route route;
  route
      .send_to(console)
      .render_with(std::make_shared<sl::PatternRenderer>("[{severity}] {endpoint}: {message}\n"))
      .filtered_by(sl::SeverityFilter::at_least<sl::severity::info>())
      .block_when_full();

  sl::Pipeline::instance().add_route(std::move(route));
  sl::Pipeline::instance().start();

  sl::Logger log("example.audit");
  log.log<audit>("user {} changed account settings", 42);

  sl::Pipeline::instance().close();
}
