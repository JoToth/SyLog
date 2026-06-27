#include "sylog/sylog.hpp"

#include <memory>
#include <utility>

int main() {
  namespace sl = sylog;

  auto console = std::make_shared<sl::ConsoleSink>();

  sl::Pipeline::instance().configure(sl::Config::synchronous());

  sl::Route route;
  route
      .send_to(console)
      .render_with(std::make_shared<sl::PatternRenderer>("[{severity}] {endpoint}: {message}\n"))
      .block_when_full();

  sl::Pipeline::instance().add_route(std::move(route));
  sl::Pipeline::instance().start();

  sl::Logger log("example.basic");
  log.info("hello from SyLog");
  log.warn("this warning is routed to the console");

  sl::Pipeline::instance().close();
}
