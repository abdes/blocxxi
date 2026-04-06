//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Base/Sha256.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Nova/Testing/GTest.h>

using ::testing::Each;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::SizeIs;

using nova::ComputeFileSha256;
using nova::ComputeSha256;
using nova::IsAllZero;
using nova::Sha256;
using nova::Sha256Digest;

namespace {

// ---------------------------------------------------------------------------
// Test Helpers
// ---------------------------------------------------------------------------

//! Convert a hex string to a Sha256Digest.
[[nodiscard]] auto HexToDigest(std::string_view hex) -> Sha256Digest
{
  Sha256Digest digest = {};
  for (size_t i = 0; i < 32 && i * 2 + 1 < hex.size(); ++i) {
    const auto hi = hex[i * 2];
    const auto lo = hex[i * 2 + 1];
    auto nibble = [](char c) -> uint8_t {
      if (c >= '0' && c <= '9') {
        return static_cast<uint8_t>(c - '0');
      }
      if (c >= 'a' && c <= 'f') {
        return static_cast<uint8_t>(c - 'a' + 10);
      }
      if (c >= 'A' && c <= 'F') {
        return static_cast<uint8_t>(c - 'A' + 10);
      }
      return 0;
    };
    digest[i] = static_cast<uint8_t>((nibble(hi) << 4u) | nibble(lo));
  }
  return digest;
}

//! Convert Sha256Digest to lowercase hex string for debugging.
[[nodiscard]] auto DigestToHex(const Sha256Digest& digest) -> std::string
{
  std::string result;
  result.reserve(64);
  for (uint8_t byte : digest) {
    constexpr char hex_chars[] = "0123456789abcdef";
    result.push_back(hex_chars[byte >> 4u]);
    result.push_back(hex_chars[byte & 0xFu]);
  }
  return result;
}

//! Convert string to span of bytes.
[[nodiscard]] auto ToBytes(std::string_view sv) -> std::span<const std::byte>
{
  return { reinterpret_cast<const std::byte*>(sv.data()), sv.size() };
}

// ---------------------------------------------------------------------------
// Reference Test Vectors
// ---------------------------------------------------------------------------
// NIST FIPS 180-4 and widely known test vectors

struct Sha256TestVector {
  std::string_view input;
  std::string_view expected_hex;
};

//! Standard SHA-256 test vectors from NIST and other authoritative sources.
constexpr std::array<Sha256TestVector, 8> kTestVectors = { {
  // Empty string
  { "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
  // "abc" - NIST short message
  { "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
  // "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
  // NIST 448-bit message
  { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
    "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
  // "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"
  // NIST 896-bit message
  { "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjk"
    "l"
    "mnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
    "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1" },
  // Single character
  { "a", "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb" },
  // Longer message
  { "The quick brown fox jumps over the lazy dog",
    "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592" },
  // With punctuation
  { "The quick brown fox jumps over the lazy dog.",
    "ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c" },
  // 63 bytes - one less than block size
  { "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde",
    "057ee79ece0b9a849552ab8d3c335fe9a5f1c46ef5f1d9b190c295728628299c" },
} };

// ---------------------------------------------------------------------------
// Basic SHA-256 Computation Tests
// ---------------------------------------------------------------------------

//! Verify that ComputeSha256 produces correct digest for empty input.
NOLINT_TEST(ComputeSha256_basic, EmptyInputProducesCorrectDigest)
{
  // Arrange
  const auto expected = HexToDigest(
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

  // Act
  const auto result = ComputeSha256({});

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify that ComputeSha256 produces correct digest for "abc".
NOLINT_TEST(ComputeSha256_basic, AbcInputProducesCorrectDigest)
{
  // Arrange
  const std::string input = "abc";
  const auto expected = HexToDigest(
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

  // Act
  const auto result = ComputeSha256(ToBytes(input));

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify ComputeSha256 is deterministic - same input produces same output.
NOLINT_TEST(ComputeSha256_basic, IsDeterministic)
{
  // Arrange
  const std::string input = "determinism test data";

  // Act
  const auto result1 = ComputeSha256(ToBytes(input));
  const auto result2 = ComputeSha256(ToBytes(input));

  // Assert
  EXPECT_THAT(result1, ElementsAreArray(result2));
}

//! Verify different inputs produce different digests (collision resistance).
NOLINT_TEST(ComputeSha256_basic, DifferentInputsProduceDifferentDigests)
{
  // Arrange
  const std::string input1 = "input one";
  const std::string input2 = "input two";

  // Act
  const auto result1 = ComputeSha256(ToBytes(input1));
  const auto result2 = ComputeSha256(ToBytes(input2));

  // Assert
  EXPECT_NE(DigestToHex(result1), DigestToHex(result2));
}

// ---------------------------------------------------------------------------
// Parameterized Tests for Standard Test Vectors
// ---------------------------------------------------------------------------

class Sha256VectorTest : public ::testing::TestWithParam<Sha256TestVector> { };

//! Verify ComputeSha256 matches reference vectors.
NOLINT_TEST_P(Sha256VectorTest, MatchesReferenceVector)
{
  // Arrange
  const auto& [input, expected_hex] = GetParam();
  const auto expected = HexToDigest(expected_hex);

  // Act
  const auto result = ComputeSha256(ToBytes(input));

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected))
    << "Input: \"" << input << "\"\n"
    << "Expected: " << expected_hex << "\n"
    << "Got: " << DigestToHex(result);
}

INSTANTIATE_TEST_SUITE_P(
  StandardVectors, Sha256VectorTest, ::testing::ValuesIn(kTestVectors));

// ---------------------------------------------------------------------------
// Streaming (Incremental Hashing) Tests
// ---------------------------------------------------------------------------

class Sha256StreamingTest : public ::testing::Test {
protected:
  Sha256 hasher_;
};

//! Verify streaming produces same result as one-shot for empty input.
NOLINT_TEST_F(Sha256StreamingTest, EmptyStreamMatchesOneShot)
{
  // Arrange
  const auto expected = ComputeSha256({});

  // Act
  const auto result = hasher_.Finalize();

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify streaming with single update matches one-shot.
NOLINT_TEST_F(Sha256StreamingTest, SingleUpdateMatchesOneShot)
{
  // Arrange
  const std::string input = "single update test";
  const auto expected = ComputeSha256(ToBytes(input));

  // Act
  hasher_.Update(ToBytes(input));
  const auto result = hasher_.Finalize();

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify streaming with multiple updates matches one-shot.
NOLINT_TEST_F(Sha256StreamingTest, MultipleUpdatesMatchOneShot)
{
  // Arrange
  const std::string part1 = "Hello, ";
  const std::string part2 = "World!";
  const std::string full = part1 + part2;
  const auto expected = ComputeSha256(ToBytes(full));

  // Act
  hasher_.Update(ToBytes(part1));
  hasher_.Update(ToBytes(part2));
  const auto result = hasher_.Finalize();

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify byte-by-byte streaming produces correct result.
NOLINT_TEST_F(Sha256StreamingTest, ByteByByteStreamingMatchesOneShot)
{
  // Arrange
  const std::string input = "abc";
  const auto expected = ComputeSha256(ToBytes(input));

  // Act
  for (char c : input) {
    const std::byte byte = static_cast<std::byte>(c);
    hasher_.Update({ &byte, 1 });
  }
  const auto result = hasher_.Finalize();

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify hasher can be reused after Finalize.
NOLINT_TEST_F(Sha256StreamingTest, CanReuseAfterFinalize)
{
  // Arrange
  const std::string input1 = "first hash";
  const std::string input2 = "second hash";
  const auto expected1 = ComputeSha256(ToBytes(input1));
  const auto expected2 = ComputeSha256(ToBytes(input2));

  // Act
  hasher_.Update(ToBytes(input1));
  const auto result1 = hasher_.Finalize();
  hasher_.Update(ToBytes(input2));
  const auto result2 = hasher_.Finalize();

  // Assert
  EXPECT_THAT(result1, ElementsAreArray(expected1));
  EXPECT_THAT(result2, ElementsAreArray(expected2));
}

//! Verify streaming with chunk sizes that span block boundaries.
NOLINT_TEST_F(Sha256StreamingTest, ChunksSpanningBlockBoundariesMatchOneShot)
{
  // Arrange - 100 bytes split across block boundary (block is 64 bytes)
  std::string full(100, 'x');
  const auto expected = ComputeSha256(ToBytes(full));

  // Act - Split at various points including mid-block
  hasher_.Update(ToBytes(full.substr(0, 30)));
  hasher_.Update(ToBytes(full.substr(30, 40))); // spans block boundary
  hasher_.Update(ToBytes(full.substr(70, 30)));
  const auto result = hasher_.Finalize();

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify empty updates don't affect the hash.
NOLINT_TEST_F(Sha256StreamingTest, EmptyUpdatesHaveNoEffect)
{
  // Arrange
  const std::string input = "test data";
  const auto expected = ComputeSha256(ToBytes(input));

  // Act
  hasher_.Update({});
  hasher_.Update(ToBytes(input));
  hasher_.Update({});
  hasher_.Update({});
  const auto result = hasher_.Finalize();

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

// ---------------------------------------------------------------------------
// Block Boundary Tests
// ---------------------------------------------------------------------------

class Sha256BlockBoundaryTest : public ::testing::Test {
protected:
  Sha256 hasher_;
};

//! Verify hashing exactly one block (64 bytes) works correctly.
NOLINT_TEST_F(Sha256BlockBoundaryTest, ExactlyOneBlock)
{
  // Arrange - Exactly 64 bytes
  std::string input(64, 'A');
  const auto expected = ComputeSha256(ToBytes(input));

  // Act
  hasher_.Update(ToBytes(input));
  const auto result = hasher_.Finalize();

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify hashing exactly two blocks (128 bytes) works correctly.
NOLINT_TEST_F(Sha256BlockBoundaryTest, ExactlyTwoBlocks)
{
  // Arrange - Exactly 128 bytes
  std::string input(128, 'B');
  const auto expected = ComputeSha256(ToBytes(input));

  // Act
  hasher_.Update(ToBytes(input));
  const auto result = hasher_.Finalize();

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify hashing 55 bytes (max payload before padding causes extra block).
NOLINT_TEST_F(Sha256BlockBoundaryTest, FiftyFiveBytes)
{
  // Arrange - 55 bytes: 55 + 1 (0x80) + 8 (length) = 64, fits in one padded
  // block
  std::string input(55, 'C');
  const auto expected = ComputeSha256(ToBytes(input));

  // Act
  hasher_.Update(ToBytes(input));
  const auto result = hasher_.Finalize();

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify hashing 56 bytes (padding forces extra block).
NOLINT_TEST_F(Sha256BlockBoundaryTest, FiftySixBytesForcesExtraBlock)
{
  // Arrange - 56 bytes: 56 + 1 + 8 = 65, requires two padded blocks
  std::string input(56, 'D');
  const auto expected = ComputeSha256(ToBytes(input));

  // Act
  hasher_.Update(ToBytes(input));
  const auto result = hasher_.Finalize();

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify hashing 63 bytes (one less than block).
NOLINT_TEST_F(Sha256BlockBoundaryTest, SixtyThreeBytes)
{
  // Arrange
  std::string input(63, 'E');
  const auto expected = ComputeSha256(ToBytes(input));

  // Act
  hasher_.Update(ToBytes(input));
  const auto result = hasher_.Finalize();

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify hashing 65 bytes (one more than block).
NOLINT_TEST_F(Sha256BlockBoundaryTest, SixtyFiveBytes)
{
  // Arrange
  std::string input(65, 'F');
  const auto expected = ComputeSha256(ToBytes(input));

  // Act
  hasher_.Update(ToBytes(input));
  const auto result = hasher_.Finalize();

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

// ---------------------------------------------------------------------------
// Large Data Tests
// ---------------------------------------------------------------------------

class Sha256LargeDataTest : public ::testing::Test { };

//! Verify hashing 1 MB of data produces consistent results.
NOLINT_TEST_F(Sha256LargeDataTest, OneMegabyteProducesConsistentResult)
{
  // Arrange
  constexpr size_t kSize = 1024 * 1024;
  std::vector<std::byte> data(kSize);
  for (size_t i = 0; i < kSize; ++i) {
    data[i] = static_cast<std::byte>(i & 0xFF);
  }

  // Act
  const auto result1 = ComputeSha256(data);
  const auto result2 = ComputeSha256(data);

  // Assert
  EXPECT_THAT(result1, ElementsAreArray(result2));
  // Ensure it's not empty or trivial
  EXPECT_FALSE(IsAllZero(result1));
}

//! Verify streaming 1 MB in various chunk sizes matches one-shot.
NOLINT_TEST_F(Sha256LargeDataTest, StreamingLargeDataMatchesOneShot)
{
  // Arrange
  constexpr size_t kSize = 256 * 1024; // 256 KB
  std::vector<std::byte> data(kSize);
  std::mt19937 rng(42); // Fixed seed for reproducibility
  for (auto& b : data) {
    b = static_cast<std::byte>(rng() & 0xFF);
  }
  const auto expected = ComputeSha256(data);

  // Act - Stream in 4KB chunks
  Sha256 hasher;
  constexpr size_t kChunkSize = 4096;
  for (size_t offset = 0; offset < data.size(); offset += kChunkSize) {
    const size_t chunk_len = std::min(kChunkSize, data.size() - offset);
    hasher.Update({ data.data() + offset, chunk_len });
  }
  const auto result = hasher.Finalize();

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

// ---------------------------------------------------------------------------
// IsAllZero Tests
// ---------------------------------------------------------------------------

class IsAllZeroTest : public ::testing::Test { };

//! Verify IsAllZero returns true for zero-filled digest.
NOLINT_TEST_F(IsAllZeroTest, ReturnsTrueForZeroDigest)
{
  // Arrange
  Sha256Digest zero_digest = {};

  // Act & Assert
  EXPECT_TRUE(IsAllZero(zero_digest));
}

//! Verify IsAllZero returns false for non-zero digest.
NOLINT_TEST_F(IsAllZeroTest, ReturnsFalseForNonZeroDigest)
{
  // Arrange
  Sha256Digest digest = {};
  digest[15] = 1; // Single non-zero byte in the middle

  // Act & Assert
  EXPECT_FALSE(IsAllZero(digest));
}

//! Verify IsAllZero returns false for actual hash result.
NOLINT_TEST_F(IsAllZeroTest, ReturnsFalseForActualHash)
{
  // Arrange
  const std::string input = "test";
  const auto digest = ComputeSha256(ToBytes(input));

  // Act & Assert
  EXPECT_FALSE(IsAllZero(digest));
}

//! Verify IsAllZero correctly detects non-zero in first byte.
NOLINT_TEST_F(IsAllZeroTest, DetectsNonZeroFirstByte)
{
  // Arrange
  Sha256Digest digest = {};
  digest[0] = 0x01;

  // Act & Assert
  EXPECT_FALSE(IsAllZero(digest));
}

//! Verify IsAllZero correctly detects non-zero in last byte.
NOLINT_TEST_F(IsAllZeroTest, DetectsNonZeroLastByte)
{
  // Arrange
  Sha256Digest digest = {};
  digest[31] = 0xFF;

  // Act & Assert
  EXPECT_FALSE(IsAllZero(digest));
}

// ---------------------------------------------------------------------------
// File Hashing Tests
// ---------------------------------------------------------------------------

class Sha256FileTest : public ::testing::Test {
protected:
  std::filesystem::path temp_file_;

  void SetUp() override
  {
    temp_file_
      = std::filesystem::temp_directory_path() / "sha256_test_file.tmp";
  }

  void TearDown() override
  {
    std::error_code ec;
    std::filesystem::remove(temp_file_, ec);
  }

  void WriteFile(const std::vector<std::byte>& data)
  {
    std::ofstream ofs(temp_file_, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(data.data()),
      static_cast<std::streamsize>(data.size()));
  }

  void WriteFile(std::string_view content)
  {
    std::ofstream ofs(temp_file_, std::ios::binary);
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
  }
};

//! Verify ComputeFileSha256 produces correct hash for small file.
NOLINT_TEST_F(Sha256FileTest, SmallFileProducesCorrectHash)
{
  // Arrange
  const std::string content = "Hello, World!";
  WriteFile(content);
  const auto expected = ComputeSha256(ToBytes(content));

  // Act
  const auto result = ComputeFileSha256(temp_file_);

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify ComputeFileSha256 produces correct hash for empty file.
NOLINT_TEST_F(Sha256FileTest, EmptyFileProducesCorrectHash)
{
  // Arrange
  WriteFile("");
  const auto expected = ComputeSha256({});

  // Act
  const auto result = ComputeFileSha256(temp_file_);

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify ComputeFileSha256 handles file larger than buffer size.
NOLINT_TEST_F(Sha256FileTest, LargeFileMatchesMemoryHash)
{
  // Arrange - Create file larger than 256KB buffer
  constexpr size_t kSize = 512 * 1024;
  std::vector<std::byte> data(kSize);
  std::mt19937 rng(12345);
  for (auto& b : data) {
    b = static_cast<std::byte>(rng() & 0xFF);
  }
  WriteFile(data);
  const auto expected = ComputeSha256(data);

  // Act
  const auto result = ComputeFileSha256(temp_file_);

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify ComputeFileSha256 throws for non-existent file.
NOLINT_TEST_F(Sha256FileTest, NonExistentFileThrows)
{
  // Arrange
  const auto non_existent = temp_file_.parent_path() / "non_existent_file.xyz";

  // Act & Assert
  EXPECT_THROW(
    [[maybe_unused]] auto _ = ComputeFileSha256(non_existent), std::exception);
}

// ---------------------------------------------------------------------------
// Hardware Support Detection Test
// ---------------------------------------------------------------------------

//! Verify HasHardwareSupport returns a valid boolean (does not crash).
NOLINT_TEST(Sha256HardwareSupport, HasHardwareSupportDoesNotCrash)
{
  // Act
  const bool has_support = Sha256::HasHardwareSupport();

  // Assert - Just verify it returns without crashing
  // The actual value depends on the CPU
  (void)has_support; // Suppress unused variable warning
  SUCCEED();
}

// ---------------------------------------------------------------------------
// Edge Case Tests
// ---------------------------------------------------------------------------

class Sha256EdgeCaseTest : public ::testing::Test { };

//! Verify single byte produces correct hash.
NOLINT_TEST_F(Sha256EdgeCaseTest, SingleByteHash)
{
  // Arrange
  const std::byte single_byte { 0x00 };
  // SHA-256 of single null byte
  const auto expected = HexToDigest(
    "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d");

  // Act
  const auto result = ComputeSha256({ &single_byte, 1 });

  // Assert
  EXPECT_THAT(result, ElementsAreArray(expected));
}

//! Verify null byte vs 0xFF byte produce different hashes.
NOLINT_TEST_F(Sha256EdgeCaseTest, NullByteVsFFByte)
{
  // Arrange
  const std::byte null_byte { 0x00 };
  const std::byte ff_byte { 0xFF };

  // Act
  const auto result_null = ComputeSha256({ &null_byte, 1 });
  const auto result_ff = ComputeSha256({ &ff_byte, 1 });

  // Assert
  EXPECT_NE(DigestToHex(result_null), DigestToHex(result_ff));
}

//! Verify binary data with embedded nulls hashes correctly.
NOLINT_TEST_F(Sha256EdgeCaseTest, BinaryDataWithEmbeddedNulls)
{
  // Arrange - Data with embedded null bytes
  const std::array<std::byte, 8> data
    = { std::byte { 0x01 }, std::byte { 0x00 }, std::byte { 0x02 },
        std::byte { 0x00 }, std::byte { 0x03 }, std::byte { 0x00 },
        std::byte { 0x04 }, std::byte { 0x00 } };

  // Act
  const auto result1 = ComputeSha256(data);
  const auto result2 = ComputeSha256(data);

  // Assert
  EXPECT_THAT(result1, ElementsAreArray(result2));
  EXPECT_FALSE(IsAllZero(result1));
}

//! Verify avalanche effect - small input change produces large output change.
NOLINT_TEST_F(Sha256EdgeCaseTest, AvalancheEffect)
{
  // Arrange
  const std::string input1 = "test";
  const std::string input2 = "Test"; // Only case difference

  // Act
  const auto result1 = ComputeSha256(ToBytes(input1));
  const auto result2 = ComputeSha256(ToBytes(input2));

  // Assert - Expect many bits to be different
  size_t different_bytes = 0;
  for (size_t i = 0; i < 32; ++i) {
    if (result1[i] != result2[i]) {
      ++different_bytes;
    }
  }
  // SHA-256 should show significant difference (most bytes should differ)
  EXPECT_GT(different_bytes, 20u);
}

} // namespace
