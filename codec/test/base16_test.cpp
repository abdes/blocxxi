//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <codec/base16.h>

#include <common/compilers.h>

#include <gtest/gtest.h>

#include <vector>

// Disable compiler and linter warnings originating from the unit test framework
// and for which we cannot do anything. Additionally, every TEST or TEST_X macro
// usage must be preceded by a '// NOLINTNEXTLINE'.
ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_CLANG_VERSION)
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wunused-member-function"
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace blocxxi::codec::hex {

struct EncodeTestParams {
  EncodeTestParams(std::vector<uint8_t> binary, bool reverse, bool lower_case,
      std::string hex)
      : binary_(binary), reverse_(reverse), lower_case_(lower_case), hex_(hex) {
  }

  ~EncodeTestParams() {
  }

  std::vector<uint8_t> binary_;
  bool reverse_;
  bool lower_case_;
  std::string hex_;
};

class CoDecTest : public ::testing::TestWithParam<EncodeTestParams> {
public:
  void SetUp() override {
    binary_ = gsl::make_span(GetParam().binary_);
    reverse_ = GetParam().reverse_;
    lower_case_ = GetParam().lower_case_;
    hex_ = GetParam().hex_;
  }

  gsl::span<const uint8_t> binary_;
  bool reverse_{false};
  bool lower_case_{false};
  std::string hex_;
};

const std::vector<EncodeTestParams> test_values{
    EncodeTestParams({0xFF}, false, false, "FF"),
    EncodeTestParams({0x00}, false, false, "00"),
    EncodeTestParams({0xFF, 0xEE, 0xDD}, false, false, "FFEEDD"),
    EncodeTestParams({0x11, 0x22, 0x33, 0x44}, false, false, "11223344"),

    // No reverse, Lower Case
    EncodeTestParams({0xFF}, false, true, "ff"),
    EncodeTestParams({0x00}, false, true, "00"),
    EncodeTestParams({0xFF, 0xEE, 0xDD}, false, true, "ffeedd"),
    EncodeTestParams({0x11, 0x22, 0x33, 0x44}, false, true, "11223344"),

    // Reverse, Upper Case
    EncodeTestParams({0xFF}, true, false, "FF"),
    EncodeTestParams({0x00}, true, false, "00"),
    EncodeTestParams({0x1D, 0x2E, 0x3F}, true, false, "F3E2D1"),
    EncodeTestParams({0xA4, 0xB3, 0xC2, 0xD1}, true, false, "1D2C3B4A"),

    // Reverse, Lower Case
    EncodeTestParams({0xFF}, true, true, "ff"),
    EncodeTestParams({0x00}, true, true, "00"),
    EncodeTestParams({0x1D, 0x2E, 0x3F}, true, true, "f3e2d1"),
    EncodeTestParams({0xA4, 0xB3, 0x62, 0xD1}, true, true, "1d263b4a"),

    // corner case
    EncodeTestParams(
        {0x00, 0x00, 0x00, 0x29, 0x00}, false, false, "0000002900"),
    EncodeTestParams({0x00, 0x00, 0x00, 0xEF, 0x00}, true, false, "00FE000000"),
};

// NOLINTNEXTLINE
INSTANTIATE_TEST_SUITE_P(Base16, CoDecTest, ::testing::ValuesIn(test_values));

// NOLINTNEXTLINE
TEST_P(CoDecTest, EncodeCorrectness) {
  auto out = Encode(binary_, reverse_, lower_case_);
  ASSERT_EQ(hex_, out);
}

// NOLINTNEXTLINE
TEST_P(CoDecTest, DecodeCorrectness) {
  constexpr auto c_max_encoded_data_size = 256;
  std::array<uint8_t, c_max_encoded_data_size> buf{};
  Decode(gsl::make_span(hex_), gsl::make_span(buf), reverse_);
  for (auto ii = 0U; ii < binary_.size(); ii++) {
    ASSERT_EQ(binary_[ii], buf.at(ii))
        << "Decoded buffer and expected buffer differ at index " << ii;
  }
}

// NOLINTNEXTLINE
TEST(Base16, DecodeOddSizeHexStringAborts) {
  std::array<uint8_t, 1> buf{};
  // NOLINTNEXTLINE
  ASSERT_DEATH(Decode(std::string_view("F"), gsl::make_span(buf), false), ".*");
  // NOLINTNEXTLINE
  ASSERT_DEATH(
      Decode(std::string_view("FAB2244"), gsl::make_span(buf), false), ".*");
}

// NOLINTNEXTLINE
TEST(Base16, DecodeWithImproperlySizedDestBufferAborts) {
  std::array<uint8_t, 0> buf{};
  // NOLINTNEXTLINE
  ASSERT_DEATH(
      Decode(std::string_view("FF"), gsl::make_span(buf), false), ".*");
  // NOLINTNEXTLINE
  ASSERT_DEATH(
      Decode(std::string_view("AB2244"), gsl::make_span(buf), false), ".*");
}

// NOLINTNEXTLINE
TEST(Base16, DecodeInputStringWithInvalidHexThrowsDomainError) {
  constexpr auto c_max_encoded_data_size = 256;
  std::array<uint8_t, c_max_encoded_data_size> buf{};
  // NOLINTNEXTLINE
  ASSERT_THROW(Decode(std::string_view("FE%@33"), gsl::make_span(buf), false),
      std::domain_error);
  // NOLINTNEXTLINE
  ASSERT_THROW(Decode(std::string_view("FEA-33"), gsl::make_span(buf), false),
      std::domain_error);
}

} // namespace blocxxi::codec::hex
