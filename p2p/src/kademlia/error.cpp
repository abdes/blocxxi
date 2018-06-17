//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <p2p/kademlia/detail/error_impl.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

std::error_condition make_error_condition(error_type code) {
  return std::error_condition{static_cast<int>(code), detail::error_category()};
}

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi
