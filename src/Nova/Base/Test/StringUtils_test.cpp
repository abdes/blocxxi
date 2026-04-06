//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Base/StringUtils.h>

#include <array>
#include <variant>

#include <Nova/Testing/GTest.h>
#include <gmock/gmock-matchers.h>

#include <Nova/Base/Platforms.h>

#ifdef NOVA_WINDOWS
using nova::windows::WindowsException;
#endif

namespace nova::string_utils {

struct StringConversionTestParam {
  std::variant<const char*, const char8_t*, const uint8_t*, std::string_view,
    std::u8string_view>
    input;
  std::wstring expected_output;
};

class ToWideTest : public testing::TestWithParam<StringConversionTestParam> { };

NOLINT_TEST_P(ToWideTest, ConvertsValidUtf8SequenceToWideString)
{
  const auto& [input, expected_output] = GetParam();
  std::wstring output;

  NOLINT_EXPECT_NO_THROW(
    std::visit([&](auto&& arg) { Utf8ToWide(arg, output); }, input););

  EXPECT_EQ(output, expected_output);
}

INSTANTIATE_TEST_SUITE_P( // NOLINT
  ToWideTests, ToWideTest,
  ::testing::Values(StringConversionTestParam { std::string_view(""), L"" },
    StringConversionTestParam { std::u8string_view(u8""), L"" },
    StringConversionTestParam { "", L"" },
    StringConversionTestParam { u8"", L"" },
    StringConversionTestParam { reinterpret_cast<const uint8_t*>(""), L"" },
    StringConversionTestParam {
      std::string_view("Hello, World!"), L"Hello, World!" },
    StringConversionTestParam {
      std::u8string_view(u8"Hello, World!"), L"Hello, World!" },
    StringConversionTestParam { "Hello, World!", L"Hello, World!" },
    StringConversionTestParam { u8"Hello, World!", L"Hello, World!" },
    StringConversionTestParam {
      reinterpret_cast<const uint8_t*>("Hello, World!"), L"Hello, World!" },
    StringConversionTestParam {
      std::string_view("こんにちは世界"), L"こんにちは世界" },
    StringConversionTestParam {
      std::u8string_view(u8"こんにちは世界"), L"こんにちは世界" },
    StringConversionTestParam { "こんにちは世界", L"こんにちは世界" },
    StringConversionTestParam { u8"こんにちは世界", L"こんにちは世界" },
    StringConversionTestParam {
      reinterpret_cast<const uint8_t*>("こんにちは世界"), L"こんにちは世界" }),
  [](const ::testing::TestParamInfo<ToWideTest::ParamType>& info) {
    // Return a simple name based on the index
    return std::to_string(info.index);
  });

NOLINT_TEST_F(ToWideTest, RejectsInvalidUtf8Sequence)
{
  constexpr char invalid_utf8[]
    = { '\xC3', '\x28', '\0' }; // 0xC3 0x28 is invalid UTF-8
  std::wstring output {};
#ifdef NOVA_WINDOWS
  try {
    Utf8ToWide(&invalid_utf8[0], output);
    FAIL() << "Expected nova::windows::WindowsException";
  } catch (const WindowsException& ex) {
    static_assert(ERROR_NO_UNICODE_TRANSLATION >= 0);
    EXPECT_EQ(ex.GetErrorCode(),
      static_cast<DWORD>(
        ERROR_NO_UNICODE_TRANSLATION)); // Replace with the expected error code
    EXPECT_THAT(ex.what(), testing::StartsWith("1113"));
  } catch (...) {
    FAIL() << "Expected nova::windows::WindowsException";
  }
#else
  EXPECT_THROW(Utf8ToWide(&invalid_utf8[0], output), std::runtime_error);
#endif

  EXPECT_TRUE(output.empty());
}

NOLINT_TEST_F(ToWideTest, CanConvertLargeUtf8String)
{
  constexpr size_t length = 1000;
  constexpr char8_t value = 42;
  // Create an array and fill it with the same value
  std::array<char8_t, length> big_utf8 {};
  big_utf8.fill(value);
  big_utf8.back() = '\0'; // Null-terminate the string

  std::wstring output {};
  const std::string_view view(reinterpret_cast<const char*>(big_utf8.data()));
  NOLINT_EXPECT_NO_THROW(Utf8ToWide(view, output));

  EXPECT_EQ(length - 1, output.size());
}

struct WideStringConversionTestParam {
  std::variant<const wchar_t*, const char16_t*, std::wstring_view, std::wstring>
    input;
  std::variant<const char*, const char8_t*> expected_output;
};

class ToUtf8Test
  : public testing::TestWithParam<WideStringConversionTestParam> { };

NOLINT_TEST_P(ToUtf8Test, ConvertsValidWideStringToUtf8String)
{
  const auto& [input, expected_output] = GetParam();
  std::string output;

  NOLINT_EXPECT_NO_THROW(
    std::visit([&](auto&& arg) { WideToUtf8(arg, output); }, input););

  std::string expected;
  std::visit(
    [&](auto&& arg) -> auto {
      expected = std::string(reinterpret_cast<const char*>(arg));
    },
    expected_output);

  EXPECT_EQ(output, expected);
}

INSTANTIATE_TEST_SUITE_P( // NOLINT
  ToUtf8Tests, ToUtf8Test,
  ::testing::Values(WideStringConversionTestParam { L"", u8"" },
    WideStringConversionTestParam { L"Hello, World!", u8"Hello, World!" },
    WideStringConversionTestParam { L"こんにちは世界", u8"こんにちは世界" },
    WideStringConversionTestParam {
      std::wstring_view(L"Hello, World!"), u8"Hello, World!" },
    WideStringConversionTestParam {
      std::wstring_view(L"こんにちは世界"), u8"こんにちは世界" },
    WideStringConversionTestParam {
      std::wstring(L"Hello, World!"), u8"Hello, World!" },
    WideStringConversionTestParam {
      std::wstring(L"こんにちは世界"), u8"こんにちは世界" },
    WideStringConversionTestParam {
      reinterpret_cast<const char16_t*>(L"Hello, World!"), u8"Hello, World!" },
    WideStringConversionTestParam {
      reinterpret_cast<const char16_t*>(L"こんにちは世界"),
      u8"こんにちは世界" }),
  [](const ::testing::TestParamInfo<ToUtf8Test::ParamType>& info) {
    // Return a simple name based on the index
    return std::to_string(info.index);
  });

NOLINT_TEST_F(ToUtf8Test, RejectsInvalidWideSequence)
{
  constexpr wchar_t invalid_wide[] = { 0xD800, L'a', L'\0' };
  std::string output {};
#ifdef NOVA_WINDOWS
  try {
    WideToUtf8(&invalid_wide[0], output);
    FAIL() << "Expected nova::windows::WindowsException";
  } catch (const WindowsException& ex) {
    static_assert(ERROR_NO_UNICODE_TRANSLATION >= 0);
    EXPECT_EQ(
      ex.GetErrorCode(), static_cast<DWORD>(ERROR_NO_UNICODE_TRANSLATION));
    EXPECT_THAT(ex.what(), testing::StartsWith("1113"));
  } catch (...) {
    FAIL() << "Expected nova::windows::WindowsException";
  }
#else
  EXPECT_THROW(WideToUtf8(&invalid_wide[0], output), std::runtime_error);
#endif
  EXPECT_TRUE(output.empty());
}

NOLINT_TEST_F(ToUtf8Test, CanConvertLargeWideString)
{
  constexpr size_t length = 200;
  constexpr wchar_t value = L'a';
  // Create an array and fill it with the same value
  std::array<wchar_t, length> big_wide {};
  big_wide.fill(value);
  big_wide.back() = L'\0'; // Null-terminate the string

  std::string output {};
  NOLINT_EXPECT_NO_THROW(WideToUtf8(big_wide.data(), output););

  EXPECT_EQ(length - 1, output.size());
}

} // namespace nova::string_utils
