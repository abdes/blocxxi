//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <string>

#include <nat/error.h>

namespace blocxxi::nat {

namespace detail {

namespace {

/// Error category implementation for the NAT system errors.
struct nat_error_category : std::error_category {
  [[nodiscard]] auto name() const noexcept -> char const * override {
    return "nat";
  }

  [[nodiscard]] auto message(int condition) const noexcept
      -> std::string override {
    switch (condition) {
    case UNKNOWN_ERROR:
      return "unknown error";
    case DISCOVERY_NOT_DONE:
      return "UPNP discovery not done or did not produce any usable results";
    case UPNP_COMMAND_ERROR:
      return "UPNP command issued to IGD returned an error";
    default:
      return "unknown error code";
    }
  }
};

} // namespace

inline auto error_category() -> std::error_category const & {
  static const nat_error_category category_{};
  return category_;
}

inline auto make_error_code(error_type code) -> std::error_code {
  return std::error_code{static_cast<int>(code), error_category()};
}

} // namespace detail

auto make_error_condition(error_type condition) -> std::error_condition {
  return std::error_condition{
      static_cast<int>(condition), detail::error_category()};
}

} // namespace blocxxi::nat
