//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <crypto/hash.h>

#include <boost/endian/conversion.hpp>

#include <common/platform.h>

namespace blocxxi {
namespace crypto {
namespace detail {

std::uint32_t HostToNetwork(std::uint32_t number) {
  return boost::endian::native_to_big(number);
}

std::uint32_t NetworkToHost(std::uint32_t number) {
  return boost::endian::big_to_native(number);
}

///@{
/// Find first set functions.
/// For details on the algorithms and options refer to
/// https://en.wikipedia.org/wiki/Find_first_set
///
/// These functions expect the range to be in big-endian byte order.

int CountLeadingZeroBits_SW(gsl::span<std::uint32_t const> buf) {
  auto const num = int(buf.size());
  std::uint32_t const* ptr = buf.data();

  for (int i = 0; i < num; i++) {
    if (ptr[i] == 0) continue;
    std::uint32_t v = NetworkToHost(ptr[i]);

    // http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogObvious
    static const int MultiplyDeBruijnBitPosition[32] = {
        0, 9,  1,  10, 13, 21, 2,  29, 11, 14, 16, 18, 22, 25, 3, 30,
        8, 12, 20, 28, 15, 17, 24, 7,  19, 27, 23, 6,  26, 5,  4, 31};

    v |= v >> 1;  // first round down to one less than a power of 2
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;

    // clang-format off
    return i * 32 + 31 - MultiplyDeBruijnBitPosition[static_cast<std::uint32_t>(v * 0x07C4ACDDU) >> 27];
    // clang-format on
  }

  return num * 32;
}

int CountLeadingZeroBits_HW(gsl::span<std::uint32_t const> buf) {
  auto const num = int(buf.size());
  std::uint32_t const* ptr = buf.data();

  for (int i = 0; i < num; i++) {
    if (ptr[i] == 0) continue;
    std::uint32_t v = NetworkToHost(ptr[i]);

#if BLOCXXI_HAS_BUILTIN_CLZ
    return i * 32 + __builtin_clz(v);
#elif defined _MSC_VER
    unsigned long pos;
    _BitScanReverse(&pos, v);
    return i * 32 + 31 - pos;
#else
    BLOCXXI_ASSERT_FAIL();
    return -1;
#endif
  }

  return num * 32;
}

int CountLeadingZeroBits(gsl::span<std::uint32_t const> buf) {
#if BLOCXXI_HAS_BUILTIN_CLZ || defined _MSC_VER
  return CountLeadingZeroBits_HW(buf);
#else
  return CountLeadingZeroBits_SW(buf);
#endif
}

///@}

}  // namespace detail
}  // namespace crypto
}  // namespace blocxxi