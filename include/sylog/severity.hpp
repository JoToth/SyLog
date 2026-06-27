#pragma once

#include <concepts>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace sylog {

class severity_base {
protected:
  constexpr severity_base() = default;

public:
  static constexpr std::string_view name() noexcept { return "CUSTOM"; }
  static constexpr std::uint16_t priority() noexcept { return 0; }
  static constexpr bool force_sync() noexcept { return false; }
  static constexpr bool flush_after() noexcept { return false; }
};

template <class T>
concept Severity = std::is_base_of_v<severity_base, T> && requires {
  { T::name() } -> std::convertible_to<std::string_view>;
  { T::priority() } -> std::convertible_to<std::uint16_t>;
  { T::force_sync() } -> std::convertible_to<bool>;
  { T::flush_after() } -> std::convertible_to<bool>;
};

namespace severity {

class trace : public severity_base {
public:
  static constexpr std::string_view name() noexcept { return "TRACE"; }
  static constexpr std::uint16_t priority() noexcept { return 100; }
};

class debug : public severity_base {
public:
  static constexpr std::string_view name() noexcept { return "DEBUG"; }
  static constexpr std::uint16_t priority() noexcept { return 200; }
};

class info : public severity_base {
public:
  static constexpr std::string_view name() noexcept { return "INFO"; }
  static constexpr std::uint16_t priority() noexcept { return 300; }
};

class warn : public severity_base {
public:
  static constexpr std::string_view name() noexcept { return "WARN"; }
  static constexpr std::uint16_t priority() noexcept { return 400; }
};

using warning = warn;

class error : public severity_base {
public:
  static constexpr std::string_view name() noexcept { return "ERROR"; }
  static constexpr std::uint16_t priority() noexcept { return 500; }
};

class fatal : public severity_base {
public:
  static constexpr std::string_view name() noexcept { return "FATAL"; }
  static constexpr std::uint16_t priority() noexcept { return 600; }
  static constexpr bool force_sync() noexcept { return true; }
  static constexpr bool flush_after() noexcept { return true; }
};

} // namespace severity

} // namespace sylog
