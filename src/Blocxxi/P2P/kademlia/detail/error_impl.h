//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/P2P/api_export.h>
#include <Blocxxi/P2P/kademlia/error.h>

// #include <system_error>

namespace blocxxi::p2p::kademlia::detail {

BLOCXXI_P2P_API auto error_category() -> std::error_category const&;

BLOCXXI_P2P_API auto make_error_code(error_type code) -> std::error_code;

} // namespace blocxxi::p2p::kademlia::detail
