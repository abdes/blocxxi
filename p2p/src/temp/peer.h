//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <iosfwd>

#include <p2p/include/p2p/kademlia/node.h>
#include "endpoint.h"

namespace blocxxi {
namespace p2p {
namespace kademlia {
namespace detail {

///
struct Peer final {
  Node::IdType id_;
  ResolvedEndpoint endpoint_;
};

/**
 *
 */
std::ostream &
operator<<
    (std::ostream &out, Peer const &p);

/**
 *
 */
inline bool
operator==
    (const Peer &a, const Peer &b) {
  return a.id_ == b.id_ && a.endpoint_ == b.endpoint_;
}

/**
 *
 */
inline bool
operator!=
    (Peer const &a, Peer const &b) { return !(a == b); }

} // namespace detail
} // namespace kademlia
}
}
