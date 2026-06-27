#include "sylog/sink.hpp"

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <system_error>

namespace sylog {

struct RotatingFileSink::Impl {
  std::mutex m;
  std::string path;
  std::shared_ptr<rotation::mode_base> rotation{rotation::never_rotate()};

  std::string opened_day;
  std::size_t current_size{0};
  std::ofstream ofs;

  static std::size_t file_size_or_zero(const std::string& p) {
    std::error_code ec;
    auto s = std::filesystem::file_size(p, ec);
    return ec ? 0 : static_cast<std::size_t>(s);
  }

  static std::string today_ymd() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return std::format("{:04d}{:02d}{:02d}", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
  }

  void open_if_needed() {
    opened_day = today_ymd();
    ofs.open(path, std::ios::out | std::ios::app);
    current_size = file_size_or_zero(path);
  }

  void rotate_files() {
    const int keep = rotation ? rotation->max_archived_files() : 0;
    if (keep <= 0) return;

    std::error_code ec;
    for (int i = keep - 1; i >= 1; --i) {
      std::filesystem::path from = path + "." + std::to_string(i);
      std::filesystem::path to = path + "." + std::to_string(i + 1);
      if (std::filesystem::exists(from, ec)) std::filesystem::rename(from, to, ec);
    }

    std::filesystem::path to = path + ".1";
    if (std::filesystem::exists(path, ec)) std::filesystem::rename(path, to, ec);
  }

  void rotate_if_needed(std::size_t incoming_bytes) {
    if (!rotation) return;
    const auto today = today_ymd();
    rotation::context ctx{current_size, incoming_bytes, opened_day, today};
    if (!rotation->should_rotate(ctx)) return;

    ofs.close();
    rotate_files();
    opened_day = today;
    ofs.open(path, std::ios::out | std::ios::trunc);
    current_size = 0;
  }
};

RotatingFileSink::RotatingFileSink(std::string path, std::shared_ptr<rotation::mode_base> rotation)
    : impl_(std::make_unique<Impl>()) {
  impl_->path = std::move(path);
  impl_->rotation = std::move(rotation);
  if (!impl_->rotation) impl_->rotation = rotation::never_rotate();
  impl_->open_if_needed();
}

RotatingFileSink::~RotatingFileSink() = default;

bool RotatingFileSink::ok() const { return static_cast<bool>(impl_->ofs); }

void RotatingFileSink::write(std::string_view bytes) {
  std::lock_guard lk(impl_->m);
  if (!impl_->ofs.is_open()) impl_->open_if_needed();
  if (!impl_->ofs) return;

  impl_->rotate_if_needed(bytes.size());
  impl_->ofs.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  impl_->current_size += bytes.size();
}

void RotatingFileSink::flush() {
  std::lock_guard lk(impl_->m);
  if (impl_->ofs) impl_->ofs.flush();
}

} // namespace sylog
