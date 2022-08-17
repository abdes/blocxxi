//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <common/compilers.h>

#include <crypto/hash.h>

#if defined(ASAP_MSVC_VERSION) && !defined(ASAP_CLANG_VERSION)
#include <iso646.h>
#endif

// Disable compiler and linter warnings originating from the unit test framework
// and for which we cannot do anything. Additionally, every TEST or TEST_X macro
// usage must be preceded by a '// NOLINTNEXTLINE'.
ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_CLANG_VERSION)
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wunused-result"
#endif
#if defined(ASAP_GNUC_VERSION)
#pragma GCC diagnostic ignored "-Wunused-result"
#endif
#if defined(ASAP_MSVC_VERSION)
#pragma warning(disable : 4834)
#endif

#include <gtest/gtest.h>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
namespace blocxxi::crypto {

// TODO(Abdessattar): Implement unit tests for blocxxi::crypto::Hash

// NOLINTNEXTLINE
TEST(HashTest, DefaultConstructorSetsAllToZero) {
  const auto h = Hash<96>();
  ASSERT_TRUE(h.IsAllZero());
}

// NOLINTNEXTLINE
TEST(HashTest, MinIsZero) {
  const auto h1 = Hash<256>::Min();
  ASSERT_TRUE(h1.IsAllZero());
  const auto h2 = Hash<192>::Min();
  ASSERT_TRUE(h2.IsAllZero());
}

// NOLINTNEXTLINE
TEST(HashTest, MaxIsAllSetToOne) {
  auto h1 = Hash<128>::Max();
  for (const auto &b : h1) {
    ASSERT_EQ(0xFF, b);
  }
  auto h2 = Hash<512>::Max();
  for (const auto &b : h2) {
    ASSERT_EQ(0xFF, b);
  }
}

// NOLINTNEXTLINE
TEST(HashTest, AtThrowsOutOfRange) {
  const auto &h = Hash<32>();
  ASSERT_EQ(32 / 8, h.Size());
  for (auto i = 0U; i < Hash<32>::Size(); ++i) {
    // NOLINTNEXTLINE
    ASSERT_NO_THROW(h.At(i));
  }
  // NOLINTNEXTLINE
  ASSERT_THROW(h.At(h.Size()), std::out_of_range);
  // NOLINTNEXTLINE
  ASSERT_THROW(h.At(h.Size() + 3), std::out_of_range);
}

// NOLINTNEXTLINE
TEST(HashTest, CountLeadingZeroBits) {
  auto h = Hash<64>();
  ASSERT_EQ(64, h.LeadingZeroBits());
  h = Hash<64>::Max();
  auto expected = 0;
  for (auto &pb : h) {
    for (auto i = 1; i <= 8; i++) {
      constexpr std::uint8_t bit_set = 0xff;
      pb = bit_set >> i;
      expected += 1;
      ASSERT_EQ(expected, h.LeadingZeroBits());
    }
  }
}

// NOLINTNEXTLINE
TEST(HashTest, AssignContentFromSpan) {
  Hash<32> h;
  ASSERT_EQ(4, h.Size());
  std::uint8_t source[] = {1, 2, 3, 4, 5, 6};
  // Assign with source span works and has no effect
  h.Assign(gsl::make_span<uint8_t>(&source[0], &source[0]), h.begin());
  ASSERT_TRUE(h.IsAllZero());

  // Assign with smaller size than available works and only changes the assigned
  // elements
  h.Clear();
  h.Assign(gsl::make_span<uint8_t>(&source[0], &source[2]), h.begin());
  ASSERT_TRUE(std::equal(&source[0], &source[2], h.begin(), h.begin() + 2));
  std::for_each(
      h.begin() + 2, h.end(), [](auto unchanged) { ASSERT_EQ(0, unchanged); });

  // Assign full hash works and only changes all elements
  h.Clear();
  h.Assign(gsl::make_span<uint8_t>(&source[0], &source[4]), h.begin());
  ASSERT_TRUE(std::equal(&source[0], &source[4], h.begin(), h.end()));

  // Assign with buffer size bigger than hash size will assert fail
  h.Clear();
  // TODO(Abdessattar): add this back when assertions are implemented
#if defined(ASAP_USE_ASSERTS)
  ASSERT_DEATH(
      h.Assign(gsl::make_span<uint8_t>(source), h.begin()), "A precondition.*");
#else
  // NOLINTNEXTLINE
  ASSERT_NO_FATAL_FAILURE(h.Assign(gsl::make_span<uint8_t>(source), h.begin()));
#endif
}

// NOLINTNEXTLINE
TEST(HashTest, ClearSetsAllToZero) {
  Hash256 h = Hash256::RandomHash();
  h.Clear();
  ASSERT_TRUE(h.IsAllZero());
}

// NOLINTNEXTLINE
TEST(HashTest, ConstructFromSpan) {
  std::uint8_t source[]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

  // Prefect case source size = hash size
  Hash<64> h1(gsl::make_span(std::begin(source), std::begin(source) + 8));
  ASSERT_TRUE(std::equal(&source[0], &source[8], h1.begin(), h1.end()));

  // source size < hash size : padding with zeros
  Hash<64> h2(gsl::make_span(std::begin(source), std::begin(source) + 4));
  auto pos = h2.begin();
  for (auto i = 0U; i < 4; ++i) {
    ASSERT_EQ(0, *pos);
    ++pos;
  }
  ASSERT_TRUE(std::equal(&source[0], &source[4], pos, h2.end()));

  // TODO(Abdessattar): make this a separate death test
  // source size > hash size : fatal assertion
#if defined(ASAP_USE_ASSERTS)
  ASSERT_DEATH(Hash<64> h3(gsl::make_span(source)), "A precondition.*");
#else
  ASSERT_NO_FATAL_FAILURE(Hash<64> h3(gsl::make_span(source)));
  // result is undefined
#endif
}

// NOLINTNEXTLINE
TEST(HashTest, AccessorsAndIterators) {
  std::uint8_t source[]{1, 2, 3, 4};
  Hash<32> h(gsl::make_span(source));

  ASSERT_EQ(1, h.Front());
  ASSERT_EQ(4, h.Back());

  const auto *check = std::cbegin(source);
  for (auto test_char : h) {
    ASSERT_EQ(*check, test_char);
    ++check;
  }

  auto rcheck = std::crbegin(source);
  for (auto it = h.crbegin(); it != h.crend(); ++it) {
    ASSERT_EQ(*rcheck, *it);
    ++rcheck;
  }
}

// NOLINTNEXTLINE
TEST(HashTest, Swap) {
  Hash<32> min = Hash<32>::Min();
  Hash<32> max = Hash<32>::Max();
  ASSERT_TRUE(min == Hash<32>::Min());
  ASSERT_TRUE(max == Hash<32>::Max());
  min.swap(max);
  ASSERT_TRUE(min == Hash<32>::Max());
  ASSERT_TRUE(max == Hash<32>::Min());

  std::swap(min, max);
  ASSERT_TRUE(min == Hash<32>::Min());
  ASSERT_TRUE(max == Hash<32>::Max());
}

// NOLINTNEXTLINE
TEST(HashTest, LessThenComparison) {
  std::array<uint8_t, 4> small{1, 2, 3, 4};
  constexpr std::size_t c_hash_size = 32;
  const Hash<c_hash_size> hsmall(gsl::make_span(small));
  ASSERT_GE(hsmall, hsmall);
  ASSERT_LE(hsmall, hsmall);
  ASSERT_EQ(hsmall, hsmall);

  constexpr std::array<std::array<uint8_t, 4>, 4> greater{
      {{{2, 2, 3, 4}}, {{1, 3, 3, 4}}, {{1, 2, 4, 4}}, {{1, 2, 3, 5}}}};
  for (const auto &val : greater) {
    Hash<c_hash_size> hval(gsl::make_span(val));
    ASSERT_GT(hval, hsmall);
    ASSERT_LT(hsmall, hval);
    ASSERT_GE(hval, hsmall);
    ASSERT_NE(hval, hsmall);
  }
}

// NOLINTNEXTLINE
TEST(HashTest, BitWiseXor) {
  constexpr std::uint8_t h1[]{1, 2, 3, 4, 5, 6, 7, 8};
  constexpr std::uint8_t h2[]{7, 0, 6, 6, 150, 65, 23, 12};

  constexpr std::uint8_t x[]{1 xor 7, 2 xor 0, 3 xor 6, 4 xor 6, 5 xor 150,
      6 xor 65, 7 xor 23, 8 xor 12};

  const auto res = Hash<64>(h1) xor Hash<64>(h2);
  ASSERT_EQ(res, Hash<64>(x));
  ASSERT_EQ(Hash<64>(h1) xor Hash<64>(h2), Hash<64>(h2) xor Hash<64>(h1));
}

// NOLINTNEXTLINE
TEST(HashTest, ToHex) {
  std::uint8_t hash_bytes[]{1, 2, 3, 4, 5, 6, 7, 8};
  const Hash<64> hash(gsl::make_span(hash_bytes));
  ASSERT_EQ("0102030405060708", hash.ToHex());
}

// NOLINTNEXTLINE
TEST(HashTest, FromHex) {
  const std::string hex("0102030405060708");
  const auto hash = Hash<64>::FromHex(hex);
  std::uint8_t hash_bytes[]{1, 2, 3, 4, 5, 6, 7, 8};
  const Hash<64> hash_ref(gsl::make_span(hash_bytes));
  ASSERT_EQ(hash_ref, hash);
}

// NOLINTNEXTLINE
TEST(HashTest, ToBitSet) {
  std::uint8_t hash_bytes[]{1, 2, 3, 4, 5, 6, 7, 8};
  const Hash<64> hash(gsl::make_span(hash_bytes));
  ASSERT_EQ("0000000100000010000000110000010000000101000001100000011100001000",
      hash.ToBitSet().to_string());
}

} // namespace blocxxi::crypto
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
ASAP_DIAGNOSTIC_POP
