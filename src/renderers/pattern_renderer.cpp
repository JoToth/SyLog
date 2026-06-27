#include "sylog/renderer.hpp"
#include "sylog/severity.hpp"

#include <cstdio>
#include <ctime>
#include <format>
#include <thread>

namespace sylog {

PatternRenderer::PatternRenderer(std::string pat) : pattern_(std::move(pat)) {}

std::string PatternRenderer::format_time(std::chrono::system_clock::time_point tp) {
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec);
  return std::string(buf);
}

void PatternRenderer::render(const Event& e, std::string& out) const {
  out = pattern_;

  auto replace_all = [&](std::string_view key, std::string_view val) {
    std::size_t pos = 0;
    while ((pos = out.find(key, pos)) != std::string::npos) {
      out.replace(pos, key.size(), val);
      pos += val.size();
    }
  };

  const auto time_s  = format_time(e.timestamp());
  const auto severity_s = std::string(e.severity_name());
  const auto severity_priority_s = std::format("{}", e.severity_priority());
  const auto file_s  = std::string(e.source().file_name());
  const auto line_s  = std::format("{}", e.source().line());
  const auto func_s  = std::string(e.source().function_name());
  const auto tid_s   = std::format("{}", std::hash<std::thread::id>{}(e.thread_id()));

  replace_all("{time}", time_s);
  replace_all("{severity}", severity_s);
  replace_all("{severity_priority}", severity_priority_s);
  replace_all("{endpoint}", e.endpoint());
  replace_all("{message}", e.message());
  replace_all("{file}", file_s);
  replace_all("{line}", line_s);
  replace_all("{func}", func_s);
  replace_all("{tid}", tid_s);

  if (!out.empty() && out.back() != '\n') out.push_back('\n');
}

AnsiConsoleRenderer::AnsiConsoleRenderer(std::string pat) : inner_(std::move(pat)) {}

namespace detail {
static std::string_view ansi_for(std::uint16_t priority) {
  if (priority >= severity::fatal::priority()) return "\x1b[35m";
  if (priority >= severity::error::priority()) return "\x1b[31m";
  if (priority >= severity::warn::priority()) return "\x1b[33m";
  if (priority >= severity::info::priority()) return "\x1b[32m";
  if (priority >= severity::debug::priority()) return "\x1b[36m";
  return "\x1b[90m";
}
} // namespace detail

void AnsiConsoleRenderer::render(const Event& e, std::string& out) const {
  inner_.render(e, out);
  const auto sev = std::string(e.severity_name());
  const auto pos = out.find(sev);
  if (pos == std::string::npos) return;
  const std::string colored = std::string(detail::ansi_for(e.severity_priority())) + sev + "\x1b[0m";
  out.replace(pos, sev.size(), colored);
}

} // namespace sylog
