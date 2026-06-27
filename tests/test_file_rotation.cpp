#include <cpptest.h>

#include "sylog/sylog.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

static std::string read_all(const fs::path& p) {
  std::ifstream ifs(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

namespace {

class FileRotationSuite final : public Test::Suite {
public:
  FileRotationSuite() {
    TEST_ADD(FileRotationSuite::rotating_file_sink_size_rotation_creates_backups);
    TEST_ADD(FileRotationSuite::rotating_file_sink_daily_rotation_smoke);
  }

private:
  void rotating_file_sink_size_rotation_creates_backups() {
    using namespace sylog;

    const fs::path dir = fs::temp_directory_path() / "sylog_tests";
    fs::create_directories(dir);
    const fs::path base = dir / "sylog_size.log";
    // cleanup old
    for (int i = 0; i < 10; ++i) {
      fs::remove(base);
      fs::remove(base.string() + "." + std::to_string(i));
    }

    Pipeline::instance().close();
    Pipeline::instance().clear_routes();
    Pipeline::instance().clear_global_filters();
    Pipeline::instance().configure(Config::synchronous().queue_capacity(1024));
    auto fsink = std::make_shared<RotatingFileSink>(base.string(), rotation::when_file_size_exceeds(200, 3));

    Route r;
    r.send_to(fsink);
    r.render_with(std::make_shared<PatternRenderer>("{message}\n"));
    Pipeline::instance().add_route(r);

    Pipeline::instance().start();

    Logger log("x");
    for (int i = 0; i < 50; ++i) log.info("line {}", i);

    Pipeline::instance().close();
    fsink->flush();

    // At least one rotation should have occurred.
    const bool any_backup = fs::exists(base.string() + ".1") || fs::exists(base.string() + ".2") || fs::exists(base.string() + ".3");
    TEST_ASSERT_MSG(any_backup, "expected rotated backup files to exist");

    // Base file should exist and be non-empty.
    TEST_ASSERT(fs::exists(base));
    TEST_ASSERT(fs::file_size(base) > 0);
  }

  void rotating_file_sink_daily_rotation_smoke() {
    using namespace sylog;

    const fs::path dir = fs::temp_directory_path() / "sylog_tests";
    fs::create_directories(dir);
    const fs::path base = dir / "sylog_daily.log";
    fs::remove(base);
    fs::remove(base.string() + ".1");
    fs::remove(base.string() + ".2");

    Pipeline::instance().close();
    Pipeline::instance().clear_routes();
    Pipeline::instance().clear_global_filters();
    Pipeline::instance().configure(Config::synchronous().queue_capacity(1024));
    auto fsink = std::make_shared<RotatingFileSink>(base.string(), rotation::when_new_day_begins(2));

    Route r;
    r.send_to(fsink);
    r.render_with(std::make_shared<PatternRenderer>("{message}\n"));
    Pipeline::instance().add_route(r);

    Pipeline::instance().start();
    Logger log("x");
    log.info("today");
    Pipeline::instance().close();
    fsink->flush();

    // Forcing a day change without time injection is not reliable in unit tests.
    // Smoke test: daily mode still writes.
    TEST_ASSERT(fs::exists(base));
    const auto content = read_all(base);
    TEST_ASSERT(content.find("today") != std::string::npos);
  }
};

} // namespace

std::unique_ptr<Test::Suite> make_file_rotation_suite() {
  return std::make_unique<FileRotationSuite>();
}
