//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <string>

#include <nat/error.h>

namespace blocxxi {
namespace nat {

namespace detail {

namespace {

/// Error category implementation for the NAT system errors.
struct nat_error_category : std::error_category {
  char const *name() const noexcept override { return "nat"; }

  std::string message(int condition) const noexcept override {
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

}  // namespace

std::error_category const &error_category() {
  static const nat_error_category category_{};
  return category_;
}

std::error_code make_error_code(error_type code) {
  return std::error_code{static_cast<int>(code), error_category()};
}

}  // namespace detail

std::error_condition make_error_condition(error_type code) {
  return std::error_condition{static_cast<int>(code), detail::error_category()};
}

}  // namespace nat
}  // namespace blocxxi
