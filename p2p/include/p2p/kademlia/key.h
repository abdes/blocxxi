//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <crypto/hash.h>

namespace blocxxi::p2p::kademlia {

/// Hash 160 key type used for value keys and  response correlation id.
using KeyType = blocxxi::crypto::Hash160;

} // namespace blocxxi::p2p::kademlia
