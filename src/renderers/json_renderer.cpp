#include "sylog/renderer.hpp"

#include <format>
#include <type_traits>

namespace sylog {
namespace detail {

static void json_escape_append(std::string& out, std::string_view s) {
  out.push_back('"');
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          out += std::format("\\u{:04x}", static_cast<unsigned>(static_cast<unsigned char>(c)));
        } else {
          out.push_back(c);
        }
        break;
    }
  }
  out.push_back('"');
}

static void json_value_append(std::string& out, const FieldValue& v) {
  std::visit(
      [&](auto&& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          out += "null";
        } else if constexpr (std::is_same_v<T, bool>) {
          out += (val ? "true" : "false");
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          out += std::format("{}", val);
        } else if constexpr (std::is_same_v<T, double>) {
          out += std::format("{}", val);
        } else if constexpr (std::is_same_v<T, std::string>) {
          json_escape_append(out, val);
        }
      },
      v);
}

} // namespace detail

void JsonRenderer::render(const Event& e, std::string& out) const {
  out.clear();
  out.reserve(512);

  out.push_back('{');

  out += "\"time\":";
  detail::json_escape_append(out, PatternRenderer::format_time(e.timestamp()));

  out += ",\"level\":";
  detail::json_escape_append(out, e.severity_name());

  out += ",\"endpoint\":";
  detail::json_escape_append(out, e.endpoint());

  out += ",\"message\":";
  detail::json_escape_append(out, e.message());

  if (includes_thread()) {
    out += ",\"tid\":";
    out += std::format("{}", std::hash<std::thread::id>{}(e.thread_id()));
  }

  if (includes_source()) {
    out += ",\"source\":{\"file\":";
    detail::json_escape_append(out, e.source().file_name());
    out += ",\"line\":";
    out += std::format("{}", e.source().line());
    out += ",\"func\":";
    detail::json_escape_append(out, e.source().function_name());
    out += "}";
  }

  if (includes_fields() && !e.fields().empty()) {
    out += ",\"fields\":{";
    for (std::size_t i = 0; i < e.fields().size(); ++i) {
      if (i) out.push_back(',');
      detail::json_escape_append(out, e.fields()[i].key());
      out.push_back(':');
      detail::json_value_append(out, e.fields()[i].value());
    }
    out += "}";
  }

  out.push_back('}');
  out.push_back('\n');
}

} // namespace sylog
