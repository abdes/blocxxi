//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/Crypto/api_export.h>

#include <optional>
#include <span>
#include <string>
#include <vector>

#include <Blocxxi/Crypto/keypair.h>

namespace blocxxi::crypto {

using SignatureBytes = std::vector<std::uint8_t>;

[[nodiscard]] BLOCXXI_CRYPTO_API auto PublicKeyToHex(
  KeyPair::PublicKey const& public_key) -> std::string;

[[nodiscard]] BLOCXXI_CRYPTO_API auto PublicKeyFromHex(
  std::string const& public_key_hex) -> std::optional<KeyPair::PublicKey>;

[[nodiscard]] BLOCXXI_CRYPTO_API auto SignMessage(
  KeyPair const& key_pair, std::span<std::uint8_t const> message) -> SignatureBytes;

[[nodiscard]] BLOCXXI_CRYPTO_API auto SignMessageHex(
  KeyPair const& key_pair, std::span<std::uint8_t const> message) -> std::string;

[[nodiscard]] BLOCXXI_CRYPTO_API auto VerifyMessage(
  KeyPair::PublicKey const& public_key, std::span<std::uint8_t const> message,
  std::span<std::uint8_t const> signature) -> bool;

[[nodiscard]] BLOCXXI_CRYPTO_API auto VerifyMessageHex(
  KeyPair::PublicKey const& public_key, std::span<std::uint8_t const> message,
  std::string const& signature_hex) -> bool;

} // namespace blocxxi::crypto
