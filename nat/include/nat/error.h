//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

/*!
 * \file
 *
 * \brief Platform independent error codes for the nat module.
 */

#pragma once

#include <nat/blocxxi_nat_export.h>

#include <system_error>

namespace blocxxi::nat {

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
BLOCXXI_NAT_API auto make_error_condition(error_type condition)
    -> std::error_condition;

} // namespace blocxxi::nat

template <>
struct std::is_error_condition_enum<blocxxi::nat::error_type> : true_type {};
