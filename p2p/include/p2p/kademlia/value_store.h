//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_P2P_KADEMLIA_VALUE_STORE_H_
#define BLOCXXI_P2P_KADEMLIA_VALUE_STORE_H_

#include <cstdint>
#include <functional>
#include <unordered_map>

#include <boost/functional/hash.hpp>

namespace blocxxi {
namespace p2p {
namespace kademlia {

template <typename TContainer>
struct ValueStoreKeyHasher {
  using ArgumentType = TContainer;
  using ResultType = std::size_t;

  ResultType operator()(ArgumentType const &key) const {
    return boost::hash_range(key.begin(), key.end());
  }
};

// TODO: Republish key-value pairs after once per hour
///
template <typename TKey, typename TValue>
using ValueStore =
    std::unordered_map<TKey, TValue, ValueStoreKeyHasher<TKey> >;

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

#endif  // BLOCXXI_P2P_KADEMLIA_VALUE_STORE_H_
