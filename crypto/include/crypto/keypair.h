//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_CRYPTO_KEYPAIR_H_
#define BLOCXXI_CRYPTO_KEYPAIR_H_

#include <crypto/hash.h>

namespace blocxxi {
namespace crypto {

class KeyPair {
 public:
  using PrivateKey = blocxxi::crypto::Hash256;
  using PublicKey = blocxxi::crypto::Hash512;

  KeyPair();

  explicit KeyPair(const std::string &secret_hex);

  explicit KeyPair(const PrivateKey &secret);

  KeyPair(const KeyPair &) = default;

  KeyPair &operator=(const KeyPair &rhs) = default;

  KeyPair(KeyPair &&) = default;

  KeyPair &operator=(KeyPair &&rhs) = default;

  ~KeyPair() { secret_.Clear(); }

  const PrivateKey &Secret() const { return secret_; }

  const PublicKey &Public() const { return public_; }

 private:
  PrivateKey secret_;
  PublicKey public_;
};

}  // namespace crypto
}  // namespace blocxxi

#endif  // BLOCXXI_CRYPTO_KEYPAIR_H_