#pragma once

#include "sylog/event.hpp"
#include "sylog/export.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <string_view>

namespace sylog {

class SYLOG_API IRenderer {
public:
  virtual ~IRenderer() = default;
  virtual void render(const Event& e, std::string& out) const = 0;
};

// Tokens: {time} {severity} {severity_priority} {endpoint} {message} {file} {line} {func} {tid}
class SYLOG_API PatternRenderer : public IRenderer {
public:
  explicit PatternRenderer(std::string pat);
  void render(const Event& e, std::string& out) const override;

  std::string_view pattern() const noexcept { return pattern_; }
  static std::string format_time(std::chrono::system_clock::time_point tp);

private:
  std::string pattern_;
};

class SYLOG_API AnsiConsoleRenderer : public IRenderer {
public:
  explicit AnsiConsoleRenderer(std::string pat);
  void render(const Event& e, std::string& out) const override;

private:
  PatternRenderer inner_;
};

class SYLOG_API JsonRenderer : public IRenderer {
public:
  JsonRenderer() = default;

  JsonRenderer& include_source(bool value) noexcept { include_source_ = value; return *this; }
  JsonRenderer& include_thread(bool value) noexcept { include_thread_ = value; return *this; }
  JsonRenderer& include_fields(bool value) noexcept { include_fields_ = value; return *this; }

  bool includes_source() const noexcept { return include_source_; }
  bool includes_thread() const noexcept { return include_thread_; }
  bool includes_fields() const noexcept { return include_fields_; }

  void render(const Event& e, std::string& out) const override;

private:
  bool include_source_{true};
  bool include_thread_{true};
  bool include_fields_{true};
};

} // namespace sylog
