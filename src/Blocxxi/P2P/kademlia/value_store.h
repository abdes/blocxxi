//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <cstddef>
#include <functional>
#include <unordered_map>

#include <Nova/Base/Hash.h>

namespace blocxxi::p2p::kademlia {

template <typename TContainer> struct ValueStoreKeyHasher {
  using ArgumentType = TContainer;
  using ResultType = std::size_t;

  auto operator()(ArgumentType const& key) const -> ResultType
  {
    auto seed = std::size_t { 0 };
    for (auto const& value : key) {
      nova::HashCombine(seed, value);
    }
    return seed;
  }
};

// TODO(Abdessattar): Republish key-value pairs after once per hour
template <typename TKey, typename TValue>
using ValueStore = std::unordered_map<TKey, TValue, ValueStoreKeyHasher<TKey>>;

} // namespace blocxxi::p2p::kademlia
