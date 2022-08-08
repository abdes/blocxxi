//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <chrono>  // for timeout value
#include <cstddef> // for std::size_t

namespace blocxxi::p2p::kademlia {

constexpr unsigned int DEPTH_B = 5;
constexpr unsigned int CONCURRENCY_K = 8;
constexpr unsigned int PARALLELISM_ALPHA = 3;
constexpr unsigned int KEYSIZE_BITS = 160;
constexpr unsigned int REDUNDANT_SAVE_COUNT = 3;

constexpr unsigned int NODE_FAILED_COMMS_BEFORE_STALE = 2;
constexpr auto NODE_INACTIVE_TIME_BEFORE_QUESTIONABLE =
    std::chrono::minutes(15);

constexpr auto PERIODIC_REFRESH_TIMER = std::chrono::seconds(6);
constexpr auto BUCKET_INACTIVE_TIME_BEFORE_REFRESH = std::chrono::seconds(1200);
constexpr auto REQUEST_TIMEOUT = std::chrono::seconds(2);

} // namespace blocxxi::p2p::kademlia
