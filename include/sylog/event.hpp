#pragma once

#include <chrono>
#include <cstdint>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace sylog {

using FieldValue = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

class Field {
public:
  Field() = default;
  Field(std::string key, FieldValue value) : key_(std::move(key)), value_(std::move(value)) {}

  std::string_view key() const noexcept { return key_; }
  const FieldValue& value() const noexcept { return value_; }

private:
  std::string key_;
  FieldValue value_;
};

class Event {
public:
  using clock = std::chrono::system_clock;

  Event() = default;

  clock::time_point timestamp() const noexcept { return ts_; }
  std::string_view severity_name() const noexcept { return severity_name_; }
  std::uint16_t severity_priority() const noexcept { return severity_priority_; }
  std::string_view endpoint() const noexcept { return endpoint_; }
  std::string_view message() const noexcept { return message_; }
  std::source_location source() const noexcept { return loc_; }
  std::thread::id thread_id() const noexcept { return tid_; }
  const std::vector<Field>& fields() const noexcept { return fields_; }

  void set_timestamp(clock::time_point v) noexcept { ts_ = v; }
  void set_severity(std::string_view name, std::uint16_t priority) noexcept {
    severity_name_ = name;
    severity_priority_ = priority;
  }
  void set_endpoint(std::string endpoint) { endpoint_ = std::move(endpoint); }
  void set_message(std::string message) { message_ = std::move(message); }
  void set_source(std::source_location loc) noexcept { loc_ = loc; }
  void set_thread_id(std::thread::id tid) noexcept { tid_ = tid; }
  void set_fields(std::span<const Field> fields) { fields_.assign(fields.begin(), fields.end()); }

private:
  clock::time_point ts_{};
  std::string_view severity_name_{"INFO"};
  std::uint16_t severity_priority_{300};
  std::string endpoint_;
  std::string message_;
  std::source_location loc_{};
  std::thread::id tid_{};
  std::vector<Field> fields_;
};

} // namespace sylog
