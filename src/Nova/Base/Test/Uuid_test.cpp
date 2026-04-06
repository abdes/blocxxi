//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Nova/Testing/GTest.h>

#include <Nova/Base/Uuid.h>

namespace {

class UuidV7Test : public testing::Test {
protected:
  static auto CanonicalV7String() -> std::string_view
  {
    return "018f8f8f-1234-7abc-8def-0123456789ab";
  }

  static auto CanonicalV7UppercaseString() -> std::string_view
  {
    return "018F8F8F-1234-7ABC-8DEF-0123456789AB";
  }

  static auto ParseV7(std::string_view text) -> nova::Result<nova::Uuid>
  {
    return nova::Uuid::FromString(text);
  }

  static auto ByteAt(const nova::Uuid& uuid, const std::size_t index) -> uint8_t
  {
    auto it = nova::as_bytes(uuid).begin();
    std::ranges::advance(it, static_cast<std::ptrdiff_t>(index));
    return std::to_integer<uint8_t>(*it);
  }

  static auto VariantBits(const nova::Uuid& uuid) -> uint8_t
  {
    constexpr auto kVariantByteIndex = std::size_t { 8U };
    constexpr auto kVariantMask = uint8_t { 0xC0U };
    return static_cast<uint8_t>(ByteAt(uuid, kVariantByteIndex) & kVariantMask);
  }
};

static_assert(sizeof(nova::Uuid) == nova::Uuid::kSize);
static_assert(std::is_trivially_copyable_v<nova::Uuid>);
static_assert(std::is_standard_layout_v<nova::Uuid>);

//! Validates that default construction yields nil/zero UUID state.
NOLINT_TEST_F(UuidV7Test, DefaultConstructorCreatesNilUuid)
{
  using nova::Uuid;

  // Arrange
  const auto uuid = Uuid {};

  // Act
  const auto bytes = nova::as_bytes(uuid);

  // Assert
  EXPECT_TRUE(uuid.IsNil());
  EXPECT_EQ(uuid.Version(), 0U);
  for (const auto byte : bytes) {
    EXPECT_EQ(byte, std::byte { 0 });
  }
}

//! Validates canonical UUIDv7 bytes parse successfully.
NOLINT_TEST_F(UuidV7Test, FromBytesParsesCanonicalV7Bytes)
{
  using nova::Uuid;

  // Arrange
  constexpr auto input = std::array<uint8_t, Uuid::kSize> { 0x01U, 0x23U, 0x45U,
    0x67U, 0x89U, 0xABU, 0x7CU, 0xDEU, 0x8FU, 0xEDU, 0xBAU, 0x98U, 0x76U, 0x54U,
    0x32U, 0x10U };

  // Act
  const auto parsed = Uuid::FromBytes(input);
  ASSERT_TRUE(parsed.has_value());
  const auto bytes = nova::as_bytes(parsed.value());
  auto expected = std::array<std::byte, Uuid::kSize> {};
  std::ranges::transform(
    input, expected.begin(), [](const uint8_t value) -> std::byte {
      return static_cast<std::byte>(value);
    });

  // Assert
  EXPECT_TRUE(std::ranges::equal(bytes, expected));
}

//! Validates byte ingress rejects non-v7 payloads.
NOLINT_TEST_F(UuidV7Test, FromBytesRejectsNonV7Payload)
{
  using nova::Uuid;

  // Arrange
  constexpr auto input = std::array<uint8_t, Uuid::kSize> { 0x01U, 0x23U, 0x45U,
    0x67U, 0x89U, 0xABU, 0x6CU, 0xDEU, 0x8FU, 0xEDU, 0xBAU, 0x98U, 0x76U, 0x54U,
    0x32U, 0x10U };

  // Act
  const auto parsed = Uuid::FromBytes(input);

  // Assert
  EXPECT_FALSE(parsed.has_value());
}

//! Validates generated UUIDs always set RFC 9562 version 7 and RFC variant
//! bits.
NOLINT_TEST_F(UuidV7Test, GenerateCreatesValidV7Rfc4122Variant)
{
  using nova::Uuid;

  // Arrange
  constexpr auto kVariantRfc4122 = uint8_t { 0x80U };

  // Act
  const auto uuid = Uuid::Generate();

  // Assert
  EXPECT_FALSE(uuid.IsNil());
  EXPECT_EQ(uuid.Version(), 7U);
  EXPECT_EQ(VariantBits(uuid), kVariantRfc4122);
}

//! Validates canonical formatter shape and lowercase output contract.
NOLINT_TEST_F(UuidV7Test, ToStringProducesCanonicalLowercaseFormat)
{
  using nova::to_string;

  // Arrange
  const auto parsed = ParseV7(CanonicalV7String());
  ASSERT_TRUE(parsed.has_value());
  const auto uuid = parsed.value();

  // Act
  const auto text = to_string(uuid);
  const auto via_member = uuid.ToString();

  // Assert
  EXPECT_EQ(text, CanonicalV7String());
  EXPECT_EQ(via_member, CanonicalV7String());
  EXPECT_EQ(text.size(), std::size_t { 36U });
  EXPECT_EQ(text[8], '-');
  EXPECT_EQ(text[13], '-');
  EXPECT_EQ(text[18], '-');
  EXPECT_EQ(text[23], '-');
}

//! Validates canonical lowercase v7 string parsing.
NOLINT_TEST_F(UuidV7Test, FromStringParsesCanonicalLowercaseV7)
{
  using nova::Uuid;

  // Arrange
  const auto text = CanonicalV7String();

  // Act
  const auto parsed = Uuid::FromString(text);

  // Assert
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed.value().Version(), 7U);
  EXPECT_EQ(parsed.value().ToString(), text);
}

//! Validates parser rejects uppercase because canonical text is lowercase-only.
NOLINT_TEST_F(UuidV7Test, FromStringRejectsUppercaseText)
{
  using nova::Uuid;

  // Arrange
  const auto uppercase = CanonicalV7UppercaseString();

  // Act
  const auto parsed = Uuid::FromString(uppercase);

  // Assert
  EXPECT_FALSE(parsed.has_value());
}

//! Validates parser rejects malformed length inputs.
NOLINT_TEST_F(UuidV7Test, FromStringRejectsInvalidLength)
{
  using nova::Uuid;

  // Arrange
  constexpr auto kTooShort
    = std::string_view { "018f8f8f-1234-7abc-8def-0123456789a" };
  constexpr auto kTooLong
    = std::string_view { "018f8f8f-1234-7abc-8def-0123456789abc" };

  // Act
  const auto short_result = Uuid::FromString(kTooShort);
  const auto long_result = Uuid::FromString(kTooLong);

  // Assert
  EXPECT_FALSE(short_result.has_value());
  EXPECT_FALSE(long_result.has_value());
}

//! Validates parser rejects non-canonical hyphen layout.
NOLINT_TEST_F(UuidV7Test, FromStringRejectsInvalidHyphenLayout)
{
  using nova::Uuid;

  // Arrange
  constexpr auto kMissingHyphen
    = std::string_view { "018f8f8f1234-7abc-8def-0123456789ab" };
  constexpr auto kWrongHyphen
    = std::string_view { "018f8f8f-12347abc-8def-0123456789ab" };

  // Act
  const auto missing_hyphen = Uuid::FromString(kMissingHyphen);
  const auto wrong_hyphen = Uuid::FromString(kWrongHyphen);

  // Assert
  EXPECT_FALSE(missing_hyphen.has_value());
  EXPECT_FALSE(wrong_hyphen.has_value());
}

//! Validates parser rejects non-hex characters.
NOLINT_TEST_F(UuidV7Test, FromStringRejectsInvalidHexCharacters)
{
  using nova::Uuid;

  // Arrange
  constexpr auto kInvalidFirstNibble
    = std::string_view { "g18f8f8f-1234-7abc-8def-0123456789ab" };
  constexpr auto kInvalidLastNibble
    = std::string_view { "018f8f8f-1234-7abc-8def-0123456789ag" };

  // Act
  const auto first_result = Uuid::FromString(kInvalidFirstNibble);
  const auto last_result = Uuid::FromString(kInvalidLastNibble);

  // Assert
  EXPECT_FALSE(first_result.has_value());
  EXPECT_FALSE(last_result.has_value());
}

//! Validates parser is v7-only and rejects non-v7 UUID versions.
NOLINT_TEST_F(UuidV7Test, FromStringRejectsNonV7Version)
{
  using nova::Uuid;

  // Arrange
  constexpr auto kVersionNibbleCharIndex = std::size_t { 14U };
  constexpr auto kNonV7VersionChar = '6';
  auto non_v7 = std::string(CanonicalV7String());
  non_v7.at(kVersionNibbleCharIndex) = kNonV7VersionChar;

  // Act
  const auto result = Uuid::FromString(non_v7);

  // Assert
  EXPECT_FALSE(result.has_value());
}

//! Validates parser rejects non-RFC-4122 variant encoding.
NOLINT_TEST_F(UuidV7Test, FromStringRejectsNonRfcVariant)
{
  using nova::Uuid;

  // Arrange
  constexpr auto kVariantNibbleCharIndex = std::size_t { 19U };
  constexpr auto kNonRfcVariantChar = 'c';
  auto non_rfc_variant = std::string(CanonicalV7String());
  non_rfc_variant.at(kVariantNibbleCharIndex) = kNonRfcVariantChar;

  // Act
  const auto result = Uuid::FromString(non_rfc_variant);

  // Assert
  EXPECT_FALSE(result.has_value());
}

//! Validates v7-only parser rejects nil UUID text.
NOLINT_TEST_F(UuidV7Test, FromStringRejectsNilUuid)
{
  using nova::Uuid;

  // Arrange
  constexpr auto kNilText
    = std::string_view { "00000000-0000-0000-0000-000000000000" };

  // Act
  const auto result = Uuid::FromString(kNilText);

  // Assert
  EXPECT_FALSE(result.has_value());
}

//! Validates parse failures consistently return invalid_argument error code.
NOLINT_TEST_F(UuidV7Test, FromStringInvalidInputsReturnInvalidArgumentErrorCode)
{
  using nova::Uuid;

  // Arrange
  constexpr auto kInvalidInputs = std::array<std::string_view, 6> {
    "018f8f8f-1234-7abc-8def-0123456789a",
    "018f8f8f-1234-7abc-8def-0123456789abc",
    "018f8f8f1234-7abc-8def-0123456789ab",
    "018f8f8f-12347abc-8def-0123456789ab",
    "018f8f8f-1234-6abc-8def-0123456789ab",
    "018f8f8f-1234-7abc-cdef-0123456789ab",
  };
  const auto expected_error = std::make_error_code(std::errc::invalid_argument);

  // Act / Assert
  for (const auto input : kInvalidInputs) {
    const auto result = Uuid::FromString(input);
    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(result.error(), expected_error);
  }
}

//! Validates parser rejects malformed strings with extra separators.
NOLINT_TEST_F(UuidV7Test, FromStringRejectsExtraSeparators)
{
  using nova::Uuid;

  // Arrange
  constexpr auto kExtraSeparators = std::array<std::string_view, 4> {
    "018f8f8f--1234-7abc-8def-0123456789ab",
    "-018f8f8f-1234-7abc-8def-0123456789ab",
    "018f8f8f-1234-7abc-8def-0123456789ab-",
    "018f8f8f-1234-7abc-8def-0123-456789ab",
  };

  // Act / Assert
  for (const auto input : kExtraSeparators) {
    const auto result = Uuid::FromString(input);
    EXPECT_FALSE(result.has_value());
  }
}

//! Validates generated UUIDs are strictly monotonic within a thread.
NOLINT_TEST_F(UuidV7Test, GenerateIsStrictlyMonotonicInSingleThread)
{
  using nova::Uuid;

  // Arrange
  constexpr auto kSampleCount = std::size_t { 8192U };
  auto generated = std::vector<Uuid> {};
  generated.reserve(kSampleCount);

  // Act
  for (auto i = std::size_t { 0U }; i < kSampleCount; ++i) {
    generated.push_back(Uuid::Generate());
  }

  // Assert
  for (auto i = std::size_t { 1U }; i < generated.size(); ++i) {
    EXPECT_EQ(
      generated.at(i - 1U) <=> generated.at(i), std::strong_ordering::less);
  }
}

//! Validates concurrent generation maintains uniqueness and valid v7 metadata.
NOLINT_TEST_F(UuidV7Test, GenerateIsUniqueAcrossThreads)
{
  using nova::Uuid;

  // Arrange
  constexpr auto kThreadCount = std::size_t { 4U };
  constexpr auto kUuidsPerThread = std::size_t { 2048U };
  constexpr auto kExpectedTotal = kThreadCount * kUuidsPerThread;

  auto all = std::vector<Uuid> {};
  all.reserve(kExpectedTotal);
  auto all_mutex = std::mutex {};
  auto workers = std::vector<std::thread> {};
  workers.reserve(kThreadCount);

  // Act
  for (auto worker_index = std::size_t { 0U }; worker_index < kThreadCount;
    ++worker_index) {
    workers.emplace_back([&all, &all_mutex]() -> void {
      auto local = std::vector<Uuid> {};
      local.reserve(kUuidsPerThread);
      for (auto i = std::size_t { 0U }; i < kUuidsPerThread; ++i) {
        local.push_back(Uuid::Generate());
      }

      auto lock = std::scoped_lock(all_mutex);
      all.insert(all.end(), local.begin(), local.end());
    });
  }

  for (auto& worker : workers) {
    worker.join();
  }

  // Assert
  ASSERT_EQ(all.size(), kExpectedTotal);
  auto unique = std::unordered_set<Uuid> {};
  unique.reserve(kExpectedTotal);
  for (const auto& uuid : all) {
    EXPECT_EQ(uuid.Version(), 7U);
    unique.insert(uuid);
  }
  EXPECT_EQ(unique.size(), kExpectedTotal);
}

//! Validates generated UUIDs round-trip through canonical text parser.
NOLINT_TEST_F(UuidV7Test, GenerateRoundTripsThroughCanonicalText)
{
  using nova::Uuid;

  // Arrange
  const auto generated = Uuid::Generate();

  // Act
  const auto parsed = Uuid::FromString(generated.ToString());

  // Assert
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed.value(), generated);
}

//! Validates standard map ordering and hash-map interoperability.
NOLINT_TEST_F(UuidV7Test, OrderedAndUnorderedContainersWork)
{
  using nova::Uuid;

  // Arrange
  const auto u1 = Uuid::Generate();
  const auto u2 = Uuid::Generate();
  const auto u3 = Uuid::Generate();
  constexpr auto kOrderedPayload1 = 1;
  constexpr auto kOrderedPayload2 = 2;
  constexpr auto kOrderedPayload3 = 3;
  constexpr auto kFirstPayload = 10;
  constexpr auto kSecondPayload = 20;

  // Act
  auto ordered = std::map<Uuid, int> {};
  ordered.emplace(u2, kOrderedPayload2);
  ordered.emplace(u1, kOrderedPayload1);
  ordered.emplace(u3, kOrderedPayload3);

  auto unordered = std::unordered_map<Uuid, int> {};
  unordered.emplace(u1, kFirstPayload);
  unordered.emplace(u2, kSecondPayload);

  // Assert
  ASSERT_EQ(ordered.size(), std::size_t { 3U });
  EXPECT_EQ(ordered.begin()->first, u1);
  EXPECT_EQ(unordered.at(u1), kFirstPayload);
  EXPECT_EQ(unordered.at(u2), kSecondPayload);
}

//! Validates hash and formatter behavior for equal UUID values.
NOLINT_TEST_F(UuidV7Test, HashAndFormatterAreConsistent)
{
  using nova::Uuid;

  // Arrange
  const auto parsed = ParseV7(CanonicalV7String());
  const auto same_parsed = ParseV7(CanonicalV7String());
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(same_parsed.has_value());
  const auto uuid = parsed.value();
  const auto same_uuid = same_parsed.value();

  // Act
  const auto hash_value = std::hash<Uuid> {}(uuid);
  const auto same_hash_value = std::hash<Uuid> {}(same_uuid);
  const auto formatted = fmt::format("{}", uuid);

  // Assert
  EXPECT_EQ(hash_value, same_hash_value);
  EXPECT_EQ(formatted, CanonicalV7String());
}

//! Validates type traits and ABI contract for engine serialization safety.
NOLINT_TEST_F(UuidV7Test, TypeTraitsAndAbiContractHold)
{
  using nova::Uuid;

  // Arrange

  // Act
  constexpr auto size = sizeof(Uuid);
  constexpr auto trivially_copyable = std::is_trivially_copyable_v<Uuid>;
  constexpr auto standard_layout = std::is_standard_layout_v<Uuid>;

  // Assert
  EXPECT_EQ(size, Uuid::kSize);
  EXPECT_TRUE(trivially_copyable);
  EXPECT_TRUE(standard_layout);
}

} // namespace
