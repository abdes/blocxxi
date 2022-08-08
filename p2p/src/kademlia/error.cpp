//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <p2p/kademlia/detail/error_impl.h>

namespace blocxxi::p2p::kademlia {

auto make_error_condition(error_type condition) -> std::error_condition {
  return std::error_condition{
      static_cast<int>(condition), detail::error_category()};
}

} // namespace blocxxi::p2p::kademlia
