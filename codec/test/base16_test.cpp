//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include "codec/base16.h"

#include <gtest/gtest.h>

#include <iostream>
#include <vector>

namespace blocxxi {
namespace codec {
namespace hex {

struct EncodeTestParams {
  EncodeTestParams(gsl::span<const uint8_t> binary, bool reverse,
                   bool lower_case, std::string hex)
      : binary_(binary),
        reverse_(reverse),
        lower_case_(lower_case),
        hex_(std::move(hex)) {}
  gsl::span<const uint8_t> binary_;
  bool reverse_;
  bool lower_case_;
  std::string hex_;
};

class CoDecTest : public ::testing::TestWithParam<EncodeTestParams> {
 public:
  void SetUp() override {
    binary_ = GetParam().binary_;
    reverse_ = GetParam().reverse_;
    lower_case_ = GetParam().lower_case_;
    hex_ = GetParam().hex_;
  }
  void TearDown() override { delete[] binary_.data(); }

 protected:
  gsl::span<const uint8_t> binary_;
  bool reverse_{false};
  bool lower_case_{false};
  std::string hex_;
};

template <size_t N>
void addTestValue(std::vector<EncodeTestParams> &values,
                  const std::array<const uint8_t, N> &data, bool reverse,
                  bool lower_case, const std::string hex) {
  auto buffer = new uint8_t[N];
  if (N > 0) memcpy(buffer, &data[0], N);
  auto data_span = gsl::make_span(buffer, N);
  values.emplace_back(EncodeTestParams(data_span, reverse, lower_case, hex));
}

std::vector<EncodeTestParams> createValidValuesForEncodeTest() {
  std::vector<EncodeTestParams> valid_values;

  // No reverse, Upper Case
  addTestValue<1>(valid_values, {{0xFF}}, false, false, "FF");
  addTestValue<1>(valid_values, {{0x00}}, false, false, "00");
  addTestValue<3>(valid_values, {{0xFF, 0xEE, 0xDD}}, false, false, "FFEEDD");
  addTestValue<4>(valid_values, {{0x11, 0x22, 0x33, 0x44}}, false, false,
                  "11223344");

  // No reverse, Lower Case
  addTestValue<1>(valid_values, {{0xFF}}, false, true, "ff");
  addTestValue<1>(valid_values, {{0x00}}, false, true, "00");
  addTestValue<3>(valid_values, {{0xFF, 0xEE, 0xDD}}, false, true, "ffeedd");
  addTestValue<4>(valid_values, {{0x11, 0x22, 0x33, 0x44}}, false, true,
                  "11223344");

  // Reverse, Upper Case
  addTestValue<1>(valid_values, {{0xFF}}, true, false, "FF");
  addTestValue<1>(valid_values, {{0x00}}, true, false, "00");
  addTestValue<3>(valid_values, {{0x1D, 0x2E, 0x3F}}, true, false, "F3E2D1");
  addTestValue<4>(valid_values, {{0xA4, 0xB3, 0xC2, 0xD1}}, true, false,
                  "1D2C3B4A");

  // Reverse, Lower Case
  addTestValue<1>(valid_values, {{0xFF}}, true, true, "ff");
  addTestValue<1>(valid_values, {{0x00}}, true, true, "00");
  addTestValue<3>(valid_values, {{0x1D, 0x2E, 0x3F}}, true, true, "f3e2d1");
  addTestValue<4>(valid_values, {{0xA4, 0xB3, 0x62, 0xD1}}, true, true,
                  "1d263b4a");

  // corner case
  addTestValue<5>(valid_values, {{0x00, 0x00, 0x00, 0x29, 0x00}}, false, false,
                  "0000002900");
  addTestValue<5>(valid_values, {{0x00, 0x00, 0x00, 0xEF, 0x00}}, true, false,
                  "00FE000000");

  return valid_values;
}

INSTANTIATE_TEST_SUITE_P(Base16, CoDecTest,
                         ::testing::ValuesIn(createValidValuesForEncodeTest()));

TEST_P(CoDecTest, EncodeCorrectness) {
  auto out = Encode(binary_, reverse_, lower_case_);
  ASSERT_EQ(hex_, out);
}

TEST_P(CoDecTest, DecodeCorrectness) {
  uint8_t buf[256]{0};
  Decode(gsl::cstring_span(hex_), gsl::span<uint8_t>(buf), reverse_);
  for (auto ii = 0U; ii < binary_.size(); ii++) {
    ASSERT_EQ(binary_.data()[ii], buf[ii])
        << "Decoded buffer and expected buffer differ at index " << ii;
  }
}

TEST(Base16, DecodeOddSizeHexStringAborts) {
  uint8_t buf[1]{0};
  ASSERT_DEATH(Decode(gsl::cstring_span("F"), gsl::span<uint8_t>(buf), false),
               ".*");
  ASSERT_DEATH(
      Decode(gsl::cstring_span("FAB2244"), gsl::span<uint8_t>(buf), false),
      ".*");
}

TEST(Base16, DecodeWithImproperlySizedDestBufferAborts) {
  uint8_t buf[1]{0};
  ASSERT_DEATH(Decode(gsl::cstring_span("F"), gsl::span<uint8_t>(buf), false),
               ".*");
  ASSERT_DEATH(
      Decode(gsl::cstring_span("FAB2244"), gsl::span<uint8_t>(buf), false),
      ".*");
}

TEST(Base16, DecodeInputStringWithInvalidHexThrowsDomainError) {
  uint8_t buf[5]{0};
  ASSERT_THROW(
      Decode(gsl::cstring_span("FE%@33"), gsl::span<uint8_t>(buf), false),
      std::domain_error);
  ASSERT_THROW(
      Decode(gsl::cstring_span("FEA-33"), gsl::span<uint8_t>(buf), false),
      std::domain_error);
}

}  // namespace hex
}  // namespace codec
}  // namespace blocxxi
