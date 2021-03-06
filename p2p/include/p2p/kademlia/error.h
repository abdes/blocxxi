//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <system_error>

namespace blocxxi {
namespace p2p {
namespace kademlia {

/// This enum list all library specific errors.
enum error_type {
  /// An unknown error.
  UNKNOWN_ERROR = 1,
  /// The session failed to contact a valid peer uppon creation.
  INITIAL_PEER_FAILED_TO_RESPOND,
  /// An unexpected response has been received.
  UNASSOCIATED_MESSAGE_ID,
  /// The provided IPv4 address is invalid.
  INVALID_IPV4_ADDRESS,
  /// The provided IPv6 address is invalid.
  INVALID_IPV6_ADDRESS,
  /// The value associated with the requested key has not been found.
  VALUE_NOT_FOUND,
  /// The internal timer failed to tick.
  TIMER_MALFUNCTION
};

/**
 *  @brief Create a library error condition.
 *
 *  @return The created error condition.
 */
std::error_condition make_error_condition(error_type condition);

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

namespace std {

template <>
struct is_error_condition_enum<blocxxi::p2p::kademlia::error_type> : true_type {
};

}  // namespace std
