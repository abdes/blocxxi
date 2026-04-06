//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <stdexcept>

#include <gtest/gtest.h>

#include <Nova/Base/Compilers.h>

#include <Blocxxi/Codec/bencode.h>

// Disable compiler and linter warnings originating from the unit test framework
// and for which we cannot do anything. Additionally, every TEST or TEST_X macro
// usage must be preceded by a '// NOLINTNEXTLINE'.
NOVA_DIAGNOSTIC_PUSH
#if NOVA_CLANG_VERSION
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace blocxxi::codec::bencode {

// NOLINTNEXTLINE
TEST(BencodeTest, EncodesDictionaryKeysLexicographically)
{
  auto value = Value(Value::DictionaryType {
    { "z", Value("tail") },
    { "a", Value("head") },
  });

  ASSERT_EQ(Encode(value), "d1:a4:head1:z4:taile");
}

// NOLINTNEXTLINE
TEST(BencodeTest, RoundTripsNestedValues)
{
  auto value = Value(Value::DictionaryType {
    { "t", Value("aa") },
    { "y", Value("q") },
    { "a",
      Value(Value::DictionaryType {
        { "id", Value("abcdefghij0123456789") },
        { "target", Value("mnopqrstuvwxyz123456") },
      }) },
  });

  auto const encoded = Encode(value);
  auto const decoded = Decode(encoded);

  ASSERT_EQ(decoded, value);
}

// NOLINTNEXTLINE
TEST(BencodeTest, DecodesBep5PingQueryExample)
{
  auto const encoded
    = std::string_view(
      "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe");

  auto const decoded = Decode(encoded);
  auto const& root = decoded.AsDictionary();

  ASSERT_EQ(root.at("q").AsString(), "ping");
  ASSERT_EQ(root.at("t").AsString(), "aa");
  ASSERT_EQ(root.at("y").AsString(), "q");
  ASSERT_EQ(root.at("a").AsDictionary().at("id").AsString(),
    "abcdefghij0123456789");
}

// NOLINTNEXTLINE
TEST(BencodeTest, RejectsTrailingData)
{
  ASSERT_THROW(static_cast<void>(Decode("i42ee")), std::invalid_argument);
}

// NOLINTNEXTLINE
TEST(BencodeTest, RejectsNonStringDictionaryKey)
{
  ASSERT_THROW(static_cast<void>(Decode("di1e3:fooe")), std::invalid_argument);
}

} // namespace blocxxi::codec::bencode
NOVA_DIAGNOSTIC_POP
