#include "sylog/sink.hpp"

#include <cstdio>
#include <mutex>

namespace sylog {

struct ConsoleSink::Impl { std::mutex m; };

ConsoleSink::ConsoleSink(bool stderr_) : use_stderr(stderr_), impl_(std::make_unique<Impl>()) {}

ConsoleSink::~ConsoleSink() = default;

void ConsoleSink::write(std::string_view bytes) {
  std::lock_guard lk(impl_->m);
  FILE* f = use_stderr ? stderr : stdout;
  std::fwrite(bytes.data(), 1, bytes.size(), f);
}

void ConsoleSink::flush() {
  std::lock_guard lk(impl_->m);
  std::fflush(use_stderr ? stderr : stdout);
}

} // namespace sylog
