//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_P2P_KADEMLIA_ERROR_IMPL_H_
#define BLOCXXI_P2P_KADEMLIA_ERROR_IMPL_H_

#include <boost/system/error_code.hpp>

#include <p2p/kademlia/error.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {
namespace detail {

std::error_category const &error_category();

std::error_code make_error_code(error_type code);

/**
 *
 */
inline std::error_code BoostToStdError(
    boost::system::error_code const &failure) {
  if (failure.category() == boost::system::generic_category())
    return std::error_code{failure.value(), std::generic_category()};
  else if (failure.category() == boost::system::system_category())
    return std::error_code{failure.value(), std::system_category()};
  else
    return make_error_code(UNKNOWN_ERROR);
}

}  // namespace detail
}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

#endif  // BLOCXXI_P2P_KADEMLIA_ERROR_IMPL_H_
