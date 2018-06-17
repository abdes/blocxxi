//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_P2P_KADEMLIA_KEY_H_
#define BLOCXXI_P2P_KADEMLIA_KEY_H_

#include <crypto/hash.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

/// Hash 160 key type used for value keys and  response correlation id.
using KeyType = blocxxi::crypto::Hash160;

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

#endif  // BLOCXXI_P2P_KADEMLIA_KEY_H_
