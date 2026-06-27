#pragma once

#include "sylog/export.hpp"

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace sylog {

class SYLOG_API ISink {
public:
  virtual ~ISink() = default;
  virtual void write(std::string_view bytes) = 0;
  virtual void flush() = 0;
};

class SYLOG_API ConsoleSink : public ISink {
public:
  explicit ConsoleSink(bool stderr_ = false);
  ~ConsoleSink();

  void write(std::string_view bytes) override;
  void flush() override;

private:
  bool use_stderr{false};
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

namespace rotation {

class context {
public:
  context(std::size_t current_size,
          std::size_t incoming_bytes,
          std::string_view opened_day,
          std::string_view today) noexcept
      : current_size_(current_size),
        incoming_bytes_(incoming_bytes),
        opened_day_(opened_day),
        today_(today) {}

  std::size_t current_size() const noexcept { return current_size_; }
  std::size_t incoming_bytes() const noexcept { return incoming_bytes_; }
  std::string_view opened_day() const noexcept { return opened_day_; }
  std::string_view today() const noexcept { return today_; }

private:
  std::size_t current_size_{0};
  std::size_t incoming_bytes_{0};
  std::string_view opened_day_;
  std::string_view today_;
};

class SYLOG_API mode_base {
protected:
  mode_base() = default;

public:
  virtual ~mode_base() = default;
  virtual bool should_rotate(const context& ctx) const = 0;
  virtual int max_archived_files() const noexcept { return 5; }
};

class SYLOG_API never : public mode_base {
public:
  bool should_rotate(const context&) const override { return false; }
};

class SYLOG_API file_size_exceeds : public mode_base {
public:
  explicit file_size_exceeds(std::size_t max_bytes, int keep_files = 5)
      : max_bytes_(max_bytes), keep_files_(keep_files) {}

  bool should_rotate(const context& ctx) const override {
    return (ctx.current_size() + ctx.incoming_bytes()) > max_bytes_;
  }

  int max_archived_files() const noexcept override { return keep_files_; }
  std::size_t max_bytes() const noexcept { return max_bytes_; }

private:
  std::size_t max_bytes_;
  int keep_files_;
};

class SYLOG_API new_day_begins : public mode_base {
public:
  explicit new_day_begins(int keep_files = 5) : keep_files_(keep_files) {}

  bool should_rotate(const context& ctx) const override {
    return !ctx.opened_day().empty() && !ctx.today().empty() && ctx.opened_day() != ctx.today();
  }

  int max_archived_files() const noexcept override { return keep_files_; }

private:
  int keep_files_;
};

class SYLOG_API file_size_exceeds_or_new_day_begins : public mode_base {
public:
  file_size_exceeds_or_new_day_begins(std::size_t max_bytes, int keep_files = 5)
      : by_size_(max_bytes, keep_files), by_day_(keep_files) {}

  bool should_rotate(const context& ctx) const override {
    return by_size_.should_rotate(ctx) || by_day_.should_rotate(ctx);
  }

  int max_archived_files() const noexcept override { return by_size_.max_archived_files(); }

private:
  file_size_exceeds by_size_;
  new_day_begins by_day_;
};

inline std::shared_ptr<mode_base> never_rotate() { return std::make_shared<never>(); }
inline std::shared_ptr<mode_base> when_file_size_exceeds(std::size_t bytes, int keep_files = 5) {
  return std::make_shared<file_size_exceeds>(bytes, keep_files);
}
inline std::shared_ptr<mode_base> when_new_day_begins(int keep_files = 5) {
  return std::make_shared<new_day_begins>(keep_files);
}
inline std::shared_ptr<mode_base> when_file_size_exceeds_or_new_day_begins(std::size_t bytes, int keep_files = 5) {
  return std::make_shared<file_size_exceeds_or_new_day_begins>(bytes, keep_files);
}

} // namespace rotation

class SYLOG_API RotatingFileSink : public ISink {
public:
  explicit RotatingFileSink(std::string path,
                            std::shared_ptr<rotation::mode_base> rotation = rotation::never_rotate());
  ~RotatingFileSink();

  void write(std::string_view bytes) override;
  void flush() override;
  bool ok() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace sylog
