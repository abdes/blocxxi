//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <gsl/gsl> // for gsl::span<std::uint8_t>

#include <cstdint> // for std::uint8_t
#include <vector>  // for std::vector<std::uint8_t>

namespace blocxxi::p2p::kademlia {

/// Represents an output buffer to which headers and message bodies can be
/// serialized. Whatever type is used, it needs to be able to expand as needed.
using Buffer = std::vector<std::uint8_t>;

/// Represents a read-only view over the input buffer used to deserialize
/// headers and message bodies.
using BufferReader = gsl::span<std::uint8_t>;

} // namespace blocxxi::p2p::kademlia
