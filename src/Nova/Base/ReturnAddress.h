//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

// Helper for MSVC
#ifdef _MSC_VER
#  include <intrin.h>
#  pragma intrinsic(_ReturnAddress)
namespace nova::detail {
[[nodiscard]] inline auto GetReturnAddress() noexcept -> void*
{
  return _ReturnAddress();
}
} // namespace nova::detail
#elif defined(__GNUC__) || defined(__clang__)
namespace nova::detail {
[[nodiscard]] inline void* GetReturnAddress() noexcept
{
  return __builtin_return_address(0);
}
} // namespace nova::detail
#else
namespace nova::detail {
[[nodiscard]] inline void* GetReturnAddress() noexcept { return nullptr; }
} // namespace nova::detail
#endif

namespace nova {
// User-facing constexpr template function
template <typename /*T*/ = void>
[[nodiscard]] constexpr auto ReturnAddress() noexcept -> void*
{
#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
  return detail::GetReturnAddress();
#else
  return nullptr;
#endif
}

} // namespace nova
