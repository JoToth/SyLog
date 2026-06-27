#include "sylog/sylog.hpp"

#include <memory>
#include <utility>

int main() {
  namespace sl = sylog;

  auto console = std::make_shared<sl::ConsoleSink>();
  auto file = std::make_shared<sl::RotatingFileSink>(
      "sylog-routing-example.log",
      sl::rotation::when_file_size_exceeds(1024 * 1024, 3));

  sl::Pipeline::instance().configure(sl::Config::asynchronous(4096));

  sl::Route console_route;
  console_route
      .send_to(console)
      .render_with(std::make_shared<sl::PatternRenderer>("[{severity}] {message}\n"))
      .filtered_by(sl::SeverityFilter::at_least<sl::severity::info>())
      .drop_when_full();

  sl::Route file_route;
  file_route
      .send_to(file)
      .render_with(std::make_shared<sl::JsonRenderer>())
      .filtered_by(sl::SeverityFilter::at_least<sl::severity::trace>())
      .block_when_full();

  sl::Pipeline::instance().add_route(std::move(console_route));
  sl::Pipeline::instance().add_route(std::move(file_route));
  sl::Pipeline::instance().start();

  sl::Logger log("example.routing");
  log.debug("debug goes only to the file route");
  log.info("info goes to console and file");
  log.error("error goes to console and file");

  sl::Pipeline::instance().close();
}
