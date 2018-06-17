//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_P2P_KADEMLIA_PARAMETERS_H_
#define BLOCXXI_P2P_KADEMLIA_PARAMETERS_H_

#include <chrono>   // for timeout value
#include <cstddef>  // for std::size_t

namespace blocxxi {
namespace p2p {
namespace kademlia {

constexpr unsigned int DEPTH_B = 5;
constexpr unsigned int CONCURRENCY_K = 8;
constexpr unsigned int PARALLELISM_ALPHA = 3;
constexpr unsigned int KEYSIZE_BITS = 160;
constexpr unsigned int REDUNDANT_SAVE_COUNT = 3;

constexpr unsigned int NODE_FAILED_COMMS_BEFORE_STALE = 2;
constexpr auto NODE_INACTIVE_TIME_BEFORE_QUESTIONABLE = std::chrono::minutes(15);

constexpr auto PERIODIC_REFRESH_TIMER = std::chrono::seconds(6);
constexpr auto BUCKET_INACTIVE_TIME_BEFORE_REFRESH = std::chrono::seconds(1200);
constexpr auto REQUEST_TIMEOUT = std::chrono::seconds(2);

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

#endif  // BLOCXXI_P2P_KADEMLIA_PARAMETERS_H_