//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <array>
#include <functional>
#include <numeric>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include <Nova/Testing/GTest.h>

#include <Nova/Base/EnumIndexedArray.h>

using nova::EnumAsIndex;
using nova::EnumIndexedArray;

namespace {

//=== Runtime tests ===-------------------------------------------------------//

//! Verifies basic runtime construction and IsValid behavior for EnumAsIndex.
NOLINT_TEST(EnumAsIndexHarnessTest, RuntimeConstructionAndValidity)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kThird,
    kCount
  };

  // Act
  auto first = EnumAsIndex { MyEnum::kFirst };
  auto second = EnumAsIndex { MyEnum::kSecond };

  // Assert
  EXPECT_TRUE(first.IsValid());
  EXPECT_TRUE(second.IsValid());
  EXPECT_EQ(static_cast<std::size_t>(first.get()), 0u);
  EXPECT_EQ(static_cast<std::size_t>(second.get()), 1u);
}

//! Death test: constructing an out-of-range runtime index should terminate.
NOLINT_TEST(EnumAsIndexHarnessDeathTest, RuntimeOutOfRangeTerminates)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };

  // Act & Assert: death-test must evaluate the expression that terminates.
  EXPECT_DEATH_IF_SUPPORTED(
    []() -> void { (void)EnumAsIndex { MyEnum::kCount }; }(), "");
}

//! Verify braced-init deduction form: EnumAsIndex{MyEnum::kFirst}
NOLINT_TEST(EnumAsIndexTest, BracedInitDeduction)
{
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };

  // Use deduction guide enabled in EnumIndexedArray.h
  auto i = EnumAsIndex { MyEnum::kSecond };
  EXPECT_TRUE(i.IsValid());
  EXPECT_EQ(i.get(), static_cast<std::size_t>(MyEnum::kSecond));
}

//! Ensure that increment/decrement and distance arithmetic behave correctly.
NOLINT_TEST(EnumAsIndexHarnessTest, ArithmeticAndDistance)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kThird,
    kCount
  };
  auto it = EnumAsIndex { MyEnum::kFirst };

  // Act & Assert: pre-increment
  ++it;
  EXPECT_EQ(it.get(), 1u);

  // Act & Assert: post-increment
  auto tmp = it++;
  EXPECT_EQ(tmp.get(), 1u);
  EXPECT_EQ(it.get(), 2u);

  // Distance
  EXPECT_EQ((it - EnumAsIndex { MyEnum::kFirst }), 2);

  // Addition and subtraction
  EXPECT_EQ((EnumAsIndex { MyEnum::kFirst } + 2).get(), 2u);
  EXPECT_EQ((EnumAsIndex { MyEnum::kThird } - 2).get(), 0u);
}

//! Iteration sentinel behaviors and comparison operators.
NOLINT_TEST(EnumAsIndexHarnessTest, IterationAndComparisons)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };
  auto begin = EnumAsIndex { MyEnum::kFirst };
  auto end = EnumAsIndex<MyEnum>::end();

  // Act & Assert
  EXPECT_LT(begin, end);
  auto it = begin;
  size_t count = 0;
  for (; it < end; ++it) {
    ++count;
  }
  EXPECT_EQ(count, static_cast<std::size_t>(MyEnum::kCount));
}

//! Indexing into EnumIndexedArray using enum, numeric, and named index forms.
NOLINT_TEST(EnumAsIndexHarnessTest, EnumIndexedArrayIndexing)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };
  EnumIndexedArray<MyEnum, int> arr {};

  // Act
  arr[MyEnum::kFirst] = 10;
  arr[EnumAsIndex<MyEnum>(MyEnum::kSecond)] = 20;

  // Assert
  EXPECT_EQ(arr[MyEnum::kFirst], 10);
  EXPECT_EQ(arr[EnumAsIndex<MyEnum>::Checked(MyEnum::kSecond)], 20);
}

//! Hashability test: ensure std::hash works for EnumAsIndex.
NOLINT_TEST(EnumAsIndexHarnessTest, Hashability)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };
  EnumAsIndex<MyEnum> a = EnumAsIndex<MyEnum>::Checked(MyEnum::kFirst);
  EnumAsIndex<MyEnum> b = EnumAsIndex<MyEnum>::Checked(MyEnum::kSecond);

  // Act
  std::hash<EnumAsIndex<MyEnum>> hasher;
  auto ha = hasher(a);
  auto hb = hasher(b);

  // Assert
  EXPECT_NE(ha, hb);
}

//! Runtime edge cases: End sentinel behavior and to_enum accessor.
//! End sentinel: End() is one-past-last and is not valid.
NOLINT_TEST(EnumAsIndexHarnessTest, EndSentinel_IsNotValid)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kThird,
    kCount
  };

  // Act
  auto end = EnumAsIndex<MyEnum>::end();

  // Assert
  EXPECT_FALSE(end.IsValid());
}

//! Validity of the last enumerator (kThird) should be true.
NOLINT_TEST(EnumAsIndexHarnessTest, LastIndex_IsValid)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kThird,
    kCount
  };

  // Act
  auto last = EnumAsIndex<MyEnum>::Checked(MyEnum::kThird);

  // Assert
  EXPECT_TRUE(last.IsValid());
}

//! to_enum roundtrip: to_enum should return the original enum value.
NOLINT_TEST(EnumAsIndexHarnessTest, ToEnum_Roundtrips)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kThird,
    kCount
  };

  // Act
  auto last = EnumAsIndex<MyEnum>::Checked(MyEnum::kThird);

  // Assert
  EXPECT_EQ(last.to_enum(), MyEnum::kThird);
}

//! Underflow death test: decrementing below 0 must terminate at runtime.
NOLINT_TEST(EnumAsIndexHarnessDeathTest, ArithmeticUnderflowTerminates)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };

  // Act & Assert: moving before kFirst should terminate.
  EXPECT_DEATH_IF_SUPPORTED(([]() -> void {
    auto v = EnumAsIndex<MyEnum>::Checked(MyEnum::kFirst);
    (void)(v - 1);
  }()),
    "");
}

//! Overflow death test: moving past end() must terminate at runtime.
NOLINT_TEST(EnumAsIndexHarnessDeathTest, ArithmeticOverflowTerminates)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };

  // Act & Assert: moving past end() should terminate.
  EXPECT_DEATH_IF_SUPPORTED(([]() -> void {
    auto v = EnumAsIndex<MyEnum>::end();
    (void)(v + 1);
  }()),
    "");
}

//=== Helpers for iteration tests ===-----------------------------------------//

enum class Color {
  kFirst = 0,
  kRed = kFirst,
  kGreen,
  kBlue,

  // ReSharper disable once CppEnumeratorNeverUsed
  kCount,
};
using ColorIndex = EnumAsIndex<Color>;
constexpr auto ColorToString(ColorIndex color) -> std::string_view
{
  switch (color.to_enum()) {
  case Color::kRed:
    return "Red";
  case Color::kGreen:
    return "Green";
  case Color::kBlue:
    return "Blue";
  default:
    return "Unknown";
  }
}

//=== Compile time tests ===--------------------------------------------------//

//! Scenario: EnumAsIndex provides a safe way to work with enum values as
//! indices, at compile time evaluation.
NOLINT_TEST(EnumAsIndexConstexprTest, CompileTimeChecked)
{
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };

  // Use the consteval template form to perform a compile-time check.
  // This must be evaluable at compile time.
  constexpr auto idx = EnumAsIndex<MyEnum>::Checked<MyEnum::kFirst>();

  // Because the above is valid, we will reach here and the static_assert will
  // pass, but it is not needed as the Checked() factory does the job already.
  static_assert(
    idx.IsValid(), "Checked<MyEnum::kFirst>() must be valid at compile time");
}

//! Additional constexpr checks: arithmetic and to_enum at compile time
NOLINT_TEST(EnumAsIndexConstexprTest, CompileTimeArithmeticAndToEnum)
{
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kThird,
    kCount
  };

  // Constexpr checked indices
  constexpr auto i0 = EnumAsIndex<MyEnum>::Checked<MyEnum::kFirst>();
  constexpr auto i1 = EnumAsIndex<MyEnum>::Checked<MyEnum::kSecond>();
  constexpr auto i2 = EnumAsIndex<MyEnum>::Checked<MyEnum::kThird>();

  static_assert(i0.get() == 0u, "i0 must be 0");
  static_assert(i1.get() == 1u, "i1 must be 1");
  static_assert(i2.get() == 2u, "i2 must be 2");

  // Arithmetic at compile time
  static_assert((i0 + 2).get() == i2.get(), "i0 + 2 == i2");
  static_assert((i2 - i0) == 2, "distance i2-i0 == 2");

  // to_enum roundtrip
  static_assert(i2.to_enum() == MyEnum::kThird, "to_enum roundtrip");
}

//! Constexpr loop sum: compute a compile-time sum by iterating
//! EnumAsIndex::begin() ... end().
NOLINT_TEST(EnumAsIndexConstexprTest, ConstexprLoopSum)
{
  // Compute sum at compile time using a consteval lambda and
  // EnumAsIndex::Checked
  constexpr auto sum = []() consteval -> std::size_t {
    std::size_t s = 0;
    for (auto i = nova::EnumAsIndex<Color>::begin();
      i < nova::EnumAsIndex<Color>::end(); ++i) {
      s += i.get();
    }
    return s;
  }();

  static_assert(sum == 3u, "compile-time sum must be 3");
  EXPECT_EQ(sum, 3u);
}

//! Constexpr name table: build a compile-time array of names using enum
//! indices.
NOLINT_TEST(EnumAsIndexConstexprTest, ConstexprNameTable)
{
  constexpr auto names = []() consteval -> std::array<std::string_view, 3> {
    std::array<std::string_view, 3> out {};
    std::size_t idx = 0;
    for (auto i = nova::EnumAsIndex<Color>::begin();
      i < nova::EnumAsIndex<Color>::end(); ++i) {
      out[idx++] = ColorToString(i);
    }
    return out;
  }();

  static_assert(names
    == std::array<std::string_view, 3> {
      std::string_view("Red"),
      std::string_view("Green"),
      std::string_view("Blue"),
    });

  EXPECT_EQ(names[0], "Red");
  EXPECT_EQ(names[1], "Green");
  EXPECT_EQ(names[2], "Blue");
}

//=== EnumRange view focused tests ===---------------------------------------//

//! Ranges pipeline: transform enum_as_index<Color> into a vector of names via
//! views.
NOLINT_TEST(EnumAsIndexViewTest, SupportsRangesPipeline)
{
  std::vector<std::string> names;
  for (const auto name :
    enum_as_index<Color> | std::views::transform(ColorToString)) {
    names.emplace_back(name);
  }

  EXPECT_EQ(std::ranges::size(names), 3);
  EXPECT_EQ(names, (std::vector<std::string> { "Red", "Green", "Blue" }));
}

//! Ranges size: enum_as_index<Color> reports the correct size via
//! std::ranges::size.
NOLINT_TEST(EnumAsIndexViewTest, RangesSize)
{
  EXPECT_EQ(std::ranges::size(enum_as_index<Color>), 3);
}

//! Range for-loop: iterate enum_as_index<Color> with range-for to collect
//! names.
NOLINT_TEST(EnumAsIndexViewTest, RangeForLoop)
{
  std::vector<std::string> names;
  for (ColorIndex c : enum_as_index<Color>) {
    names.emplace_back(std::string(ColorToString(c)));
  }

  EXPECT_EQ(names, (std::vector<std::string> { "Red", "Green", "Blue" }));
}

//! Algorithm integration: use std::ranges algorithms (find_if, any_of) on
//! enum_as_index.
NOLINT_TEST(EnumAsIndexViewTest, AlgorithmIntegration_FindAnyOf)
{
  // find the first green entry using ranges::find_if
  auto it = std::ranges::find_if(enum_as_index<Color>,
    [](ColorIndex c) -> bool { return c.to_enum() == Color::kGreen; });

  EXPECT_NE(it, std::ranges::end(enum_as_index<Color>));
  EXPECT_EQ((*it).to_enum(), Color::kGreen);

  // any_of: check if any element is Blue
  bool hasBlue = std::ranges::any_of(enum_as_index<Color>,
    [](ColorIndex c) -> bool { return c.to_enum() == Color::kBlue; });
  EXPECT_TRUE(hasBlue);
}

//! Transform+reverse composition: compose views to produce reversed name list.
NOLINT_TEST(EnumAsIndexViewTest, TransformReverseComposition)
{
  std::vector<std::string> out;
  for (auto&& name : enum_as_index<Color> | std::views::reverse
      | std::views::transform([](ColorIndex c) -> std::string {
          return std::string(ColorToString(c));
        })) {
    out.emplace_back(std::move(name));
  }

  EXPECT_EQ(out, (std::vector<std::string> { "Blue", "Green", "Red" }));
}

//! Hash container usage: EnumAsIndex is usable as a key in unordered_map.
NOLINT_TEST(EnumAsIndexViewTest, HashContainerUsage)
{
  std::unordered_map<ColorIndex, std::string> map;
  for (ColorIndex c : enum_as_index<Color>) {
    map.emplace(c, std::string(ColorToString(c)));
  }

  EXPECT_EQ(map.size(), 3u);
  EXPECT_EQ(map.at(ColorIndex(Color::kRed)), "Red");
  EXPECT_EQ(map.at(ColorIndex(Color::kGreen)), "Green");
  EXPECT_EQ(map.at(ColorIndex(Color::kBlue)), "Blue");
}

} // namespace
