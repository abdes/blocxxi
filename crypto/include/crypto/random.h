//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <crypto/blocxxi_crypto_export.h>

#include <cstddef> // for std::size_t
#include <cstdint> // for uint8_t

namespace blocxxi::crypto::random {

BLOCXXI_CRYPTO_API void GenerateBlock(std::uint8_t *output, std::size_t size);

} // namespace blocxxi::crypto::random
