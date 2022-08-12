//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <common/compilers.h>
#include <contract/ut/framework.h>
#include <contract/ut/gtest.h>

#include <codec/base16.h>

// Disable compiler and linter warnings originating from the unit test framework
// and for which we cannot do anything. Additionally, every TEST or TEST_X macro
// usage must be preceded by a '// NOLINTNEXTLINE'.
ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_CLANG_VERSION)
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace blocxxi::codec::hex {

struct EncodeTestParams {
  EncodeTestParams(std::vector<uint8_t> binary, bool reverse, bool lower_case,
      std::string hex)
      : binary_(std::move(binary)), reverse_(reverse), lower_case_(lower_case),
        hex_(std::move(hex)) {
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
using Base16DecodeTest = CoDecTest;
using Base16EncodeTest = CoDecTest;

const std::vector<EncodeTestParams> normal_cases{
    EncodeTestParams({}, false, false, ""),
    EncodeTestParams({0xFF}, false, false, "FF"),
    EncodeTestParams({0x00}, false, false, "00"),
    EncodeTestParams({0xFF, 0xEE, 0xDD}, false, false, "FFEEDD"),
    EncodeTestParams({0x11, 0x22, 0x33, 0x44}, false, false, "11223344"),

    // No reverse, Lower Case
    EncodeTestParams({}, false, true, ""),
    EncodeTestParams({0xFF}, false, true, "ff"),
    EncodeTestParams({0x00}, false, true, "00"),
    EncodeTestParams({0xFF, 0xEE, 0xDD}, false, true, "ffeedd"),
    EncodeTestParams({0x11, 0x22, 0x33, 0x44}, false, true, "11223344"),

    // Reverse, Upper Case
    EncodeTestParams({}, true, false, ""),
    EncodeTestParams({0xFF}, true, false, "FF"),
    EncodeTestParams({0x00}, true, false, "00"),
    EncodeTestParams({0x1D, 0x2E, 0x3F}, true, false, "F3E2D1"),
    EncodeTestParams({0xA4, 0xB3, 0xC2, 0xD1}, true, false, "1D2C3B4A"),

    // Reverse, Lower Case
    EncodeTestParams({}, true, true, ""),
    EncodeTestParams({0xFF}, true, true, "ff"),
    EncodeTestParams({0x00}, true, true, "00"),
    EncodeTestParams({0x1D, 0x2E, 0x3F}, true, true, "f3e2d1"),
    EncodeTestParams({0xA4, 0xB3, 0x62, 0xD1}, true, true, "1d263b4a"),

    // corner case
    EncodeTestParams(
        {0x00, 0x00, 0x00, 0x29, 0x00}, false, false, "0000002900"),
    EncodeTestParams({0x00, 0x00, 0x00, 0xEF, 0x00}, true, false, "00FE000000"),
};

auto MakeTestCaseName(
    const testing::TestParamInfo<Base16EncodeTest::ParamType> &info) {
  std::string name = info.param.hex_;
  if (name.empty()) {
    name = "_empty";
  }
  if (info.param.lower_case_) {
    name += "_lc";
  }
  if (info.param.reverse_) {
    name += "_r";
  }
  return name;
}

// NOLINTNEXTLINE
INSTANTIATE_TEST_SUITE_P(NormalCases, Base16EncodeTest,
    ::testing::ValuesIn(normal_cases), MakeTestCaseName);

// NOLINTNEXTLINE
TEST_P(Base16EncodeTest, ProperlyEncodesBinaryData) {
  auto out = Encode(binary_, reverse_, lower_case_);
  ASSERT_EQ(hex_, out);
}

// NOLINTNEXTLINE
INSTANTIATE_TEST_SUITE_P(NormalCases, Base16DecodeTest,
    ::testing::ValuesIn(normal_cases), MakeTestCaseName);

// NOLINTNEXTLINE
TEST_P(Base16DecodeTest, ProperlyDecodesHexString) {
  constexpr auto c_max_encoded_data_size = 256;
  std::array<uint8_t, c_max_encoded_data_size> buf{};
  Decode(gsl::make_span(hex_), gsl::make_span(buf), reverse_);
  for (auto ii = 0U; ii < binary_.size(); ii++) {
    ASSERT_EQ(binary_[ii], buf.at(ii))
        << "Decoded buffer and expected buffer differ at index " << ii;
  }
}

#if !defined(ASAP_CONTRACT_OFF)
// NOLINTNEXTLINE
TEST(Base16DecodeContractCheckTest, AbortsOnOddSizedInput) {
  constexpr auto c_output_buffer_size = 16;
  std::array<uint8_t, c_output_buffer_size> buf{};
  EXPECT_VIOLATES_CONTRACT(
      Decode(std::string_view("F"), gsl::make_span(buf), false));
  EXPECT_VIOLATES_CONTRACT(
      Decode(std::string_view("FAB2244"), gsl::make_span(buf), false));
}

// NOLINTNEXTLINE
TEST(Base16DecodeContractCheckTest, AbortsOnSmallerThenNeededOutputBuffer) {
  std::array<uint8_t, 0> zero_buf{};
  EXPECT_VIOLATES_CONTRACT(
      Decode(std::string_view("FF"), gsl::make_span(zero_buf), false));

  std::array<uint8_t, 2> small_buf{};
  EXPECT_VIOLATES_CONTRACT(
      Decode(std::string_view("FF23AED2"), gsl::make_span(small_buf), false));
}
#endif

// NOLINTNEXTLINE
TEST(Base16DecodeInvalidInputTest, ThrowsDomainError) {
  constexpr auto c_max_encoded_data_size = 256;
  std::array<uint8_t, c_max_encoded_data_size> buf{};
  // NOLINTNEXTLINE
  ASSERT_THROW(Decode(std::string_view("FE%@33"), gsl::make_span(buf), false),
      std::domain_error);
  // NOLINTNEXTLINE
  ASSERT_THROW(Decode(std::string_view("FEA-33"), gsl::make_span(buf), true),
      std::domain_error);
}

} // namespace blocxxi::codec::hex
