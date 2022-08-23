//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <crypto/blocxxi_crypto_export.h>

#include <crypto/hash.h>

namespace blocxxi::crypto {

class KeyPair {
public:
  using PrivateKey = blocxxi::crypto::Hash256;
  using PublicKey = blocxxi::crypto::Hash512;

  BLOCXXI_CRYPTO_API KeyPair();

  explicit BLOCXXI_CRYPTO_API KeyPair(const std::string &secret_hex);

  explicit BLOCXXI_CRYPTO_API KeyPair(const PrivateKey &secret);

  BLOCXXI_CRYPTO_API KeyPair(const KeyPair &) = default;

  BLOCXXI_CRYPTO_API auto operator=(const KeyPair &rhs) -> KeyPair & = default;

  BLOCXXI_CRYPTO_API KeyPair(KeyPair &&) = default;

  BLOCXXI_CRYPTO_API auto operator=(KeyPair &&rhs) -> KeyPair & = default;

  BLOCXXI_CRYPTO_API ~KeyPair() {
    secret_.Clear();
  }

  [[nodiscard]] BLOCXXI_CRYPTO_API auto Secret() const -> const PrivateKey & {
    return secret_;
  }

  [[nodiscard]] BLOCXXI_CRYPTO_API auto Public() const -> const PublicKey & {
    return public_;
  }

private:
  PrivateKey secret_;
  PublicKey public_;
};

} // namespace blocxxi::crypto
