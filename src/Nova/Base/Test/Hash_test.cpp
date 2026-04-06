//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <array>
#include <string_view>
#include <vector>

#include <Nova/Base/Hash.h>
#include <Nova/Testing/GTest.h>

namespace {

//! Verify that HashCombine deterministically mixes values into the seed.
NOLINT_TEST(HashCombine_basic, Deterministic)
{
  // Arrange
  size_t seed1 = 0;
  size_t seed2 = 0;

  // Act
  nova::HashCombine(seed1, 42);
  nova::HashCombine(seed2, 42);

  // Assert
  EXPECT_EQ(seed1, seed2);
}

//! Verify that combining different values produces different seeds (very
//! likely).
NOLINT_TEST(HashCombine_basic, DifferentValuesChangeSeed)
{
  // Arrange
  size_t seed = 0;
  size_t seed_with_1 = seed;
  size_t seed_with_2 = seed;

  // Act
  nova::HashCombine(seed_with_1, 1);
  nova::HashCombine(seed_with_2, 2);

  // Assert
  EXPECT_NE(seed_with_1, seed_with_2);
}

// NOTE: Reference FNV-1a test vectors from python-fnvhash

constexpr std::array<std::pair<std::string_view, std::uint64_t>, 11>
  fnv1a_string_tests = { {
    { "", 0xcbf29ce484222325ULL },
    { "a", 0xaf63dc4c8601ec8cULL },
    { "b", 0xaf63df4c8601f1a5ULL },
    { "c", 0xaf63de4c8601eff2ULL },
    { "d", 0xaf63d94c8601e773ULL },
    { "e", 0xaf63d84c8601e5c0ULL },
    { "f", 0xaf63db4c8601ead9ULL },
    { "foobar", 0x85944171f73967e8ULL },
    { "hello", 0xa430d84680aabd0bULL },
    { "FNV1a", 0x439af329408e451bULL },
    { "The quick brown fox jumps over the lazy dog", 0xf3f9b7f5e7e47110ULL },
  } };

using Binary = std::vector<unsigned char>;
std::array<std::pair<Binary, std::uint64_t>, 9> fnv1a_binary_tests = { {
  // single-byte inputs
  { Binary { 0x00 }, 0xaf63bd4c8601b7dfULL },
  { Binary { 0xFF }, 0xaf64724c8602eb6eULL },
  // 4-byte hex patterns
  { Binary { 0xDE, 0xAD, 0xBE, 0xEF }, 0x277045760cdd0993ULL },
  { Binary { 0x01, 0x02, 0x03, 0x04 }, 0xbe7a5e775165785dULL },
  { Binary { 0x10, 0x20, 0x30, 0x40 }, 0x623637059e5851b5ULL },
  // 4-byte integer patterns
  { Binary { 0x00, 0x00, 0x00, 0x00 }, 0x4d25767f9dce13f5ULL }, // 0
  { Binary { 0xFF, 0xFF, 0xFF, 0xFF },
    0x994f76653e2a3951ULL }, // -1 or UINT32_MAX
  { Binary { 0x78, 0x56, 0x34, 0x12 }, 0xcccfd053e47c3365ULL }, // 0x12345678
  { Binary { 0x21, 0x43, 0x65, 0x87 }, 0x9c1c436b54765cbdULL }, // 0x87654321
} };

// Parameterized tests for FNV-1a using the prepared test vectors above.

// String inputs
class FNV1aStringTest : public ::testing::TestWithParam<
                          std::pair<std::string_view, std::uint64_t>> { };

NOLINT_TEST_P(FNV1aStringTest, MatchesReference)
{
  const auto param = GetParam();
  const std::string_view sv = param.first;
  const std::uint64_t expected = param.second;

  const auto h = nova::ComputeFNV1a64(sv.data(), sv.size());
  EXPECT_EQ(h, expected);
}

INSTANTIATE_TEST_SUITE_P(FNV1aStrings, FNV1aStringTest,
  // string values
  ::testing::ValuesIn(fnv1a_string_tests));

// Binary inputs (merged single-byte, hex patterns, and integer patterns)
using BinaryParam = std::pair<Binary, std::uint64_t>;
class FNV1aBinaryTest : public ::testing::TestWithParam<BinaryParam> { };

NOLINT_TEST_P(FNV1aBinaryTest, MatchesReference)
{
  const auto param = GetParam();
  const auto& vec = param.first;
  const std::uint64_t expected = param.second;

  const auto h = nova::ComputeFNV1a64(vec.data(), vec.size());
  EXPECT_EQ(h, expected);
}

INSTANTIATE_TEST_SUITE_P(FNV1aBinaries, FNV1aBinaryTest,
  // binary data values
  ::testing::ValuesIn(fnv1a_binary_tests));

//! Verify ComputeFNV1a64 with zero-length input returns the offset basis.
NOLINT_TEST(ComputeFNV1a64_basic, EmptyInputReturnsOffsetBasis)
{
  // Arrange
  const void* data = nullptr;

  // Act
  const auto h = nova::ComputeFNV1a64(data, 0);

  // Assert
  // FNV-1a offset basis for 64-bit.
  constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
  EXPECT_EQ(h, kOffsetBasis);
}

} // namespace
