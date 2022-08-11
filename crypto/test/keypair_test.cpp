//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <gtest/gtest.h>

#include <crypto/keypair.h>

namespace blocxxi::crypto {

// NOLINTNEXTLINE
TEST(KeyPairTest, NewKeyPair) {
  auto kpair = KeyPair();

  std::cout << kpair.Secret() << std::endl;
  std::cout << kpair.Public() << std::endl;

  kpair = KeyPair(
      "40C756697E6F60AC839FE53DD403F0B254D49A26243A196300CD4D515EE28062");
  std::cout << kpair.Secret() << std::endl;
  // 1D26A437CB485CF1789FBBD8188D588DE8CE4FBD2F97629AB0319794B676519BD5587FC71CC14D30731C0C0CDAAF8DD04C088FEF475254B9731DC77094AB2B9B
  std::cout << kpair.Public() << std::endl;
}

} // namespace blocxxi::crypto
