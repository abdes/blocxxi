//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_P2P_KADEMLIA_BUFFER_H_
#define BLOCXXI_P2P_KADEMLIA_BUFFER_H_

#include <cstdint>  // for std::uint8_t
#include <gsl/gsl>  // for gsl::span<std::uint8_t>
#include <vector>   // for std::vector<std::uint8_t>

namespace blocxxi {
namespace p2p {
namespace kademlia {

/// Represents an output buffer to which headers and message bodies can be
/// serialized. Whatever type is used, it needs to be able to expand as needed.
using Buffer = std::vector<std::uint8_t>;

/// Represents a read-only view over the input buffer used to deserialize
/// headers and message bodies.
using BufferReader = gsl::span<std::uint8_t>;

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

#endif  // BLOCXXI_P2P_KADEMLIA_BUFFER_H_
