//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <crypto/random.h>

#include <osrng.h>  // for random number generation

namespace blocxxi {
namespace crypto {
namespace random {

void GenerateBlock(std::uint8_t *output, std::size_t size) {
  CryptoPP::AutoSeededRandomPool rng;
  rng.GenerateBlock(output, size);
}

}  // namespace random
}  // namespace crypto
}  // namespace blocxxi