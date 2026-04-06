//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>

namespace nova {

//! Combines a hash seed with the hash of a value.
/*!
 Implements hash combination based on the Boost hash_combine function. Uses a
 good mixing function with the magic number 0x9e3779b9 (derived from the golden
 ratio) to distribute bits throughout the resulting hash.

 This function is useful for hashing multiple fields together, such as when
 implementing std::hash for composite types.

 @param seed The existing hash value to be combined with the new value's hash.
 @param v The value to be hashed and combined with the seed.
*/
template <class T> void HashCombine(size_t& seed, const T& v)
{
  // Magic constant for hash mixing, derived from the golden ratio.
  constexpr size_t golden_ratio = 0x9e3779b97f4a7c15ULL;
  // Number of bits to shift left in hash mixing.
  constexpr size_t shift_left = 6;
  // Number of bits to shift right in hash mixing.
  constexpr size_t shift_right = 2;

  std::hash<T> hasher;
  seed
    ^= hasher(v) + golden_ratio + (seed << shift_left) + (seed >> shift_right);
}

// Use FNV-1a helper for the final hash.
inline auto ComputeFNV1a64(std::span<const std::byte> bytes) -> std::uint64_t
{
  // NOLINTNEXTLINE(*-magic-numbers)
  std::uint64_t h = 14695981039346656037ULL; // offset basis for FNV-1a 64
  // NOLINTNEXTLINE(*-magic-numbers)
  constexpr std::uint64_t prime = 1099511628211ULL;
  for (const auto byte : bytes) {
    h ^= std::to_integer<std::uint64_t>(byte);
    h *= prime;
  }
  return h;
}

inline auto ComputeFNV1a64(const void* data, size_t size_bytes) -> std::uint64_t
{
  if (size_bytes == 0) {
    return ComputeFNV1a64(std::span<const std::byte> {});
  }

  return ComputeFNV1a64(std::span<const std::byte>(
    static_cast<const std::byte*>(data), size_bytes));
}

} // namespace nova
