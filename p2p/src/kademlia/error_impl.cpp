//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <string>

#include <p2p/kademlia/detail/error_impl.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {
namespace detail {

namespace {

/// Error category implementation for the kademlia system errors.
struct kademlia_error_category : std::error_category {
  char const *name() const noexcept override { return "kademlia"; }

  std::string message(int condition) const noexcept override {
    switch (condition) {
      case INITIAL_PEER_FAILED_TO_RESPOND:
        return "initial peer failed to respond";
      case UNASSOCIATED_MESSAGE_ID:
        return "unassociated message id";
      case INVALID_IPV4_ADDRESS:
        return "invalid IPv4 address";
      case INVALID_IPV6_ADDRESS:
        return "invalid IPv6 address";
      case VALUE_NOT_FOUND:
        return "value not found";
      case TIMER_MALFUNCTION:
        return "timer malfunction";
      default:
        return "unknown error";
    }
  }
};

}  // namespace

std::error_category const &error_category() {
  static const kademlia_error_category category_{};
  return category_;
}

std::error_code make_error_code(error_type code) {
  return std::error_code{static_cast<int>(code), error_category()};
}

}  // namespace detail
}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi
