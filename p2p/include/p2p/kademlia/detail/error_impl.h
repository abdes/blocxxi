//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include "p2p/blocxxi_p2p_export.h"
#include <boost/system/error_code.hpp>

#include <p2p/blocxxi_p2p_api.h>
#include <p2p/kademlia/error.h>

namespace blocxxi::p2p::kademlia::detail {

BLOCXXI_P2P_API auto error_category() -> std::error_category const &;

BLOCXXI_P2P_API auto make_error_code(error_type code) -> std::error_code;

/**
 *
 */
inline auto BoostToStdError(boost::system::error_code const &failure)
    -> std::error_code {
  if (failure.category() == boost::system::generic_category()) {
    return std::error_code{failure.value(), std::generic_category()};
  }
  if (failure.category() == boost::system::system_category()) {
    return std::error_code{failure.value(), std::system_category()};
  }
  return make_error_code(UNKNOWN_ERROR);
}

} // namespace blocxxi::p2p::kademlia::detail
