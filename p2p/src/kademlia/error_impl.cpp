//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <p2p/kademlia/detail/error_impl.h>

#include <string>

namespace blocxxi::p2p::kademlia::detail {

namespace {

/// Error category implementation for the kademlia system errors.
struct kademlia_error_category : std::error_category {
  [[nodiscard]] auto name() const noexcept -> char const * override {
    return "kademlia";
  }

  [[nodiscard]] auto message(int condition) const noexcept
      -> std::string override {
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

} // namespace

auto error_category() -> std::error_category const & {
  static const kademlia_error_category category_{};
  return category_;
}

auto make_error_code(error_type code) -> std::error_code {
  return std::error_code{static_cast<int>(code), error_category()};
}

} // namespace blocxxi::p2p::kademlia::detail
