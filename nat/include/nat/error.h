//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_NAT_ERROR_H_
#define BLOCXXI_NAT_ERROR_H_

#include <system_error>

namespace blocxxi {
namespace nat {

/// All NAT module error codes.
enum error_type {
  /// An unknown error.
  UNKNOWN_ERROR = 1,
  DISCOVERY_NOT_DONE,
  UPNP_COMMAND_ERROR
};

/**
 *  @brief Create a library error condition.
 *
 *  @return The created error condition.
 */
std::error_condition make_error_condition(error_type condition);

}  // namespace nat
}  // namespace blocxxi

namespace std {

template <>
struct is_error_condition_enum<blocxxi::nat::error_type> : true_type {};

}  // namespace std

#endif  // BLOCXXI_NAT_ERROR_H_
