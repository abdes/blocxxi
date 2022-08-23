//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <common/compilers.h>

#include <crypto/config.h>
#include <crypto/hash.h>

ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_GNUC_VERSION)
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#endif
#if defined(ASAP_CLANG_VERSION) &&                                             \
    ASAP_HAS_WARNING("-Wreserved-macro-identifier")
#pragma clang diagnostic ignored "-Wreserved-macro-identifier"
#endif
#if defined(ASAP_CLANG_VERSION) && ASAP_HAS_WARNING("-Wreserved-identifier")
#pragma clang diagnostic ignored "-Wreserved-identifier"
#endif
#include <boost/endian/conversion.hpp>
ASAP_DIAGNOSTIC_POP

namespace blocxxi::crypto::detail {

auto HostToNetwork(std::uint32_t number) -> std::uint32_t {
  return boost::endian::native_to_big(number);
}

auto NetworkToHost(std::uint32_t number) -> std::uint32_t {
  return boost::endian::big_to_native(number);
}

///@{
/// Find first set functions.
/// For details on the algorithms and options refer to
/// https://en.wikipedia.org/wiki/Find_first_set
///
/// These functions expect the range to be in big-endian byte order.

inline auto CountLeadingZeroBits_SW(gsl::span<std::uint32_t const> buf)
    -> size_t {
  auto const num = buf.size();
  std::uint32_t const *ptr = buf.data();

  for (size_t i = 0; i < num; i++) {
    if (ptr[i] == 0) {
      continue;
    }
    std::uint32_t v = NetworkToHost(ptr[i]);

    // http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogObvious
    constexpr size_t MultiplyDeBruijnBitPosition[32] = {0, 9, 1, 10, 13, 21, 2,
        29, 11, 14, 16, 18, 22, 25, 3, 30, 8, 12, 20, 28, 15, 17, 24, 7, 19, 27,
        23, 6, 26, 5, 4, 31};

    v |= v >> 1; // first round down to one less than a power of 2
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

inline auto CountLeadingZeroBits_HW(gsl::span<std::uint32_t const> buf)
    -> size_t {
  auto const num = buf.size();
  std::uint32_t const *ptr = buf.data();

  for (size_t i = 0; i < num; i++) {
    if (ptr[i] == 0) {
      continue;
    }
    std::uint32_t v = NetworkToHost(ptr[i]);

#if BLOCXXI_HAS_BUILTIN_CLZ
    return i * 32 + static_cast<size_t>(__builtin_clz(v));
#elif BLOCXXI_HAS_BITSCAN_REVERSE
    unsigned long pos;
    _BitScanReverse(&pos, v);
    return i * 32 + 31 - pos;
#endif
  }

  return num * 32;
}

auto CountLeadingZeroBits(gsl::span<std::uint32_t const> buf) -> size_t {
#if BLOCXXI_HAS_BUILTIN_CLZ || BLOCXXI_HAS_BITSCAN_REVERSE
  return CountLeadingZeroBits_HW(buf);
#else
  return CountLeadingZeroBits_SW(buf);
#endif
}

///@}

} // namespace blocxxi::crypto::detail
