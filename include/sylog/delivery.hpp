#pragma once

#include "sylog/event.hpp"
#include "sylog/export.hpp"

#include <atomic>
#include <memory>
#include <string_view>
#include <type_traits>

namespace sylog::detail { class WorkersEventQueue; }

namespace sylog::delivery {

class SYLOG_API strategy_base {
protected:
  constexpr strategy_base() = default;

public:
  virtual ~strategy_base() = default;
  virtual std::string_view name() const noexcept = 0;
  virtual bool submit(detail::WorkersEventQueue& queue, Event&& event, std::atomic<bool>& accepting) const = 0;
};

class SYLOG_API drop_when_full : public strategy_base {
public:
  std::string_view name() const noexcept override;
  bool submit(detail::WorkersEventQueue& queue, Event&& event, std::atomic<bool>& accepting) const override;
};

class SYLOG_API block_when_full : public strategy_base {
public:
  std::string_view name() const noexcept override;
  bool submit(detail::WorkersEventQueue& queue, Event&& event, std::atomic<bool>& accepting) const override;
};

template <class T>
concept Strategy = std::is_base_of_v<strategy_base, T>;

inline std::shared_ptr<strategy_base> dropping_when_full() {
  return std::make_shared<drop_when_full>();
}

inline std::shared_ptr<strategy_base> blocking_when_full() {
  return std::make_shared<block_when_full>();
}

} // namespace sylog::delivery
