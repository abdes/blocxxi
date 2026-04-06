//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Base/Endian.h>
#include <Nova/Base/Types/Geometry.h>

#include <cstdint>
#include <numbers>
#include <string>
#include <type_traits>

#include <Nova/Testing/GTest.h>

namespace {

//===----------------------------------------------------------------------===//

template <typename T>
concept HasToString = requires(const T& type) {
  { to_string(type) } -> std::convertible_to<std::string>;
};

#define CHECK_HAS_TO_STRING(T)                                                 \
  static_assert(HasToString<nova::T>, "T must have to_string implementation")

NOLINT_TEST(CommonTypes, HaveToString)
{
  CHECK_HAS_TO_STRING(PixelPosition);
  CHECK_HAS_TO_STRING(SubPixelPosition);
  CHECK_HAS_TO_STRING(PixelExtent);
  CHECK_HAS_TO_STRING(SubPixelExtent);
  CHECK_HAS_TO_STRING(PixelBounds);
  CHECK_HAS_TO_STRING(SubPixelBounds);
  CHECK_HAS_TO_STRING(PixelMotion);
  CHECK_HAS_TO_STRING(SubPixelMotion);
  CHECK_HAS_TO_STRING(Axis1D);
  CHECK_HAS_TO_STRING(Axis2D);
}

//===----------------------------------------------------------------------===//

NOLINT_TEST(EndianTest, IsLittleEndian_ChecksSystemEndianness)
{
  constexpr uint32_t value = 0x01234567;
  const auto bytes = std::as_bytes(std::span { &value, 1 });
  const bool is_little = bytes[0] == std::byte { 0x67 };

  EXPECT_EQ(nova::IsLittleEndian(), is_little);
}

//===----------------------------------------------------------------------===//

NOLINT_TEST(ByteSwapTest, ByteSwap_SingleByte_NoChange)
{
  constexpr uint8_t value = 0x12;
  EXPECT_EQ(nova::ByteSwap(value), value);
}

NOLINT_TEST(ByteSwapTest, ByteSwap_16Bit)
{
  constexpr uint16_t value = 0x1234;
  constexpr uint16_t expected = 0x3412;
  EXPECT_EQ(nova::ByteSwap(value), expected);
}

NOLINT_TEST(ByteSwapTest, ByteSwap_32Bit)
{
  constexpr uint32_t value = 0x12345678;
  constexpr uint32_t expected = 0x78563412;
  EXPECT_EQ(nova::ByteSwap(value), expected);
}

NOLINT_TEST(ByteSwapTest, ByteSwap_64Bit)
{
  constexpr uint64_t value = 0x1234567890ABCDEF;
  constexpr uint64_t expected = 0xEFCDAB9078563412;
  EXPECT_EQ(nova::ByteSwap(value), expected);
}

NOLINT_TEST(ByteSwapTest, ByteSwap_Float)
{
  union {
    float f;
    uint32_t i;
  } u1 = { std::numbers::pi_v<float> }, u2;

  u2.i = nova::ByteSwap(u1.i);
  u1.i = nova::ByteSwap(u2.i);
  EXPECT_FLOAT_EQ(u1.f, std::numbers::pi_v<float>);
}

NOLINT_TEST(ByteSwapTest, ByteSwap_Double)
{
  union {
    double d;
    uint64_t i;
  } u1 = { std::numbers::pi }, u2;

  u2.i = nova::ByteSwap(u1.i);
  u1.i = nova::ByteSwap(u2.i);
  EXPECT_DOUBLE_EQ(u1.d, std::numbers::pi);
}

} // namespace
