#include "sylog/sylog.hpp"

#include <memory>
#include <utility>

int main() {
  namespace sl = sylog;

  auto console = std::make_shared<sl::ConsoleSink>();

  sl::Pipeline::instance().configure(sl::Config::asynchronous(1024));

  sl::Route route;
  route
      .send_to(console)
      .render_with(std::make_shared<sl::PatternRenderer>("{time} [{severity}] {message}\n"))
      .block_when_full();

  sl::Pipeline::instance().add_route(std::move(route));
  sl::Pipeline::instance().start();

  sl::Logger log("example.async");
  for (int i = 0; i < 10; ++i) {
    log.info("workers-backed event {}", i);
  }

  // Graceful lifecycle: stop accepting new async events and drain queued ones.
  sl::Pipeline::instance().close();
}
