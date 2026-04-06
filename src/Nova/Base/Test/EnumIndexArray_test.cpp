//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <memory>
#include <numeric>
#include <ranges>
#include <vector>

#include <Nova/Testing/GTest.h>

#include <Nova/Base/EnumIndexedArray.h>

using nova::EnumAsIndex;
using nova::EnumIndexedArray;

namespace {

//=== Runtime tests ===-------------------------------------------------------//

//! Indexing by enum value stores and retrieves the correct element.
NOLINT_TEST(EnumIndexedArrayTest, IndexByEnum)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };
  EnumIndexedArray<MyEnum, int> arr {};

  // Act
  arr[MyEnum::kFirst] = 42;

  // Assert
  EXPECT_EQ(arr[MyEnum::kFirst], 42);
}

//! Numeric indexing forwards to the underlying array semantics (like
//! std::array).
NOLINT_TEST(EnumIndexedArrayTest, IndexByNumber)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };
  EnumIndexedArray<MyEnum, int> arr {};
  size_t idx = 1U;
  int value = 7;

  // Act
  arr[idx] = value;

  // Assert
  EXPECT_EQ(arr[idx], value);
}

//! Indexing via EnumAsIndex works; End() sentinel should be invalid in debug.
NOLINT_TEST(EnumIndexedArrayTest, IndexByEnumAsIndex)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };
  EnumIndexedArray<MyEnum, int> arr {};

  // Act
  arr[EnumAsIndex { MyEnum::kSecond }] = 99;

  // Assert
  EXPECT_EQ(arr[EnumAsIndex { MyEnum::kSecond }], 99);
}

//! begin()/end() and size() reflect the underlying container's iterators and
//! size.
NOLINT_TEST(EnumIndexedArrayTest, IteratorsAndSize)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };
  EnumIndexedArray<MyEnum, int> arr {};
  arr[MyEnum::kFirst] = 1;
  arr[MyEnum::kSecond] = 2;

  // Act
  std::vector<int> collected(arr.begin(), arr.end());

  // Assert: size and contents preserved
  EXPECT_EQ(arr.size(), static_cast<std::size_t>(MyEnum::kCount));
  EXPECT_EQ(collected.size(), arr.size());
  EXPECT_EQ(collected[0], 1);
  EXPECT_EQ(collected[1], 2);
}

//! Named-index wrapper: simulate a strong-type index with get().
struct MyIndex {
  // ReSharper disable once CppDeclaratorNeverUsed
  auto get() const noexcept -> std::size_t { return value; }
  std::size_t value;
};

//! Supports indexed access from a named-index wrapper type that exposes get().
NOLINT_TEST(EnumIndexedArrayTest, IndexByNamedIndexWrapper)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };
  EnumIndexedArray<MyEnum, int> arr {};
  MyIndex idx { 1 };

  // Act
  arr[idx] = 123;

  // Assert
  EXPECT_EQ(arr[idx], 123);
}

//! at(size_t) throws std::out_of_range for numeric indices >= count.
NOLINT_TEST(EnumIndexedArrayDeathTest, AtThrowsOnOutOfRangeNumeric)
{
  enum class TestColor : std::size_t { kFirst = 0, kSecond, kThird, kCount };
  EnumIndexedArray<TestColor, int> arr {};
  arr[TestColor::kFirst] = 0;
  arr[TestColor::kSecond] = 1;
  arr[TestColor::kThird] = 2;

  NOLINT_EXPECT_THROW(arr.at(static_cast<std::size_t>(3)), std::out_of_range);
}

//! at(Enum) throws std::out_of_range for enum values outside the valid range.
NOLINT_TEST(EnumIndexedArrayDeathTest, AtThrowsOnOutOfRangeEnum)
{
  enum class TestColor : std::size_t { kFirst = 0, kSecond, kThird, kCount };
  EnumIndexedArray<TestColor, int> arr {};
  arr[TestColor::kFirst] = 0;
  arr[TestColor::kSecond] = 1;
  arr[TestColor::kThird] = 2;
  // Create an enum value outside the valid range by casting
  TestColor invalid = static_cast<TestColor>(
    static_cast<std::underlying_type_t<TestColor>>(TestColor::kCount));
  NOLINT_EXPECT_THROW(arr.at(invalid), std::out_of_range);
}

//! Indexing with the EnumAsIndex end() sentinel must trigger termination in
//! debug.
NOLINT_TEST(EnumIndexedArrayDeathTest, IndexWithEndTerminates)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };
  EnumIndexedArray<MyEnum, int> arr {};

  // Act & Assert: evaluate indexing with end() which should trigger assert in
  // debug
  EXPECT_DEATH_IF_SUPPORTED(
    ([&arr]() -> void { arr[EnumAsIndex<MyEnum>::end()] = 1; }()), "");
}

//! Constexpr usage: EnumIndexedArray must be usable in compile-time contexts.
NOLINT_TEST(EnumIndexedArrayConstexprTest, ConstexprUsage)
{
  // Arrange
  enum class MyEnum : std::size_t { kFirst = 0, kSecond, kCount };

  // Act / Assert: create a constexpr instance and perform basic operations.
  constexpr EnumIndexedArray<MyEnum, int> carr
    = []() constexpr -> EnumIndexedArray<MyEnum, int> {
    EnumIndexedArray<MyEnum, int> a {};
    a[MyEnum::kFirst] = 4;
    a[MyEnum::kSecond] = 6;
    return a;
  }();

  static_assert(carr[MyEnum::kFirst] == 4, "constexpr first");
  static_assert(carr[MyEnum::kSecond] == 6, "constexpr second");
}

//! Non-trivial element type support: container works with move-only types.
NOLINT_TEST(EnumIndexedArrayTest, NonTrivialElementTypeUniquePtr)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kCount
  };
  EnumIndexedArray<MyEnum, std::unique_ptr<int>> arr {};

  // Act
  arr[MyEnum::kFirst] = std::make_unique<int>(55);

  // Assert
  ASSERT_NE(arr[MyEnum::kFirst], nullptr);
  EXPECT_EQ(*arr[MyEnum::kFirst], 55);
}

//! Ranges compatibility: supports accumulation via iterators and ranges
//! adaptors.
NOLINT_TEST(EnumIndexedArrayTest, RangesAccumulate)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kThird,
    kCount
  };
  EnumIndexedArray<MyEnum, int> arr {};
  arr[MyEnum::kFirst] = 1;
  arr[MyEnum::kSecond] = 2;
  arr[MyEnum::kThird] = 3;

  // Act
  auto v = arr; // copy
  auto sum = std::accumulate(v.begin(), v.end(), 0);

  // Assert
  EXPECT_EQ(sum, 6);
}

//! Ranges transform: works with std::views::transform to create a mapped view.
NOLINT_TEST(EnumIndexedArrayTest, RangesTransformView)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kThird,
    kCount
  };
  EnumIndexedArray<MyEnum, int> arr {};
  arr[MyEnum::kFirst] = 1;
  arr[MyEnum::kSecond] = 2;
  arr[MyEnum::kThird] = 3;

  // Act: use a transform view to add 1 to each element
  auto v = arr | std::views::transform([](int x) -> int { return x + 1; });
  std::vector<int> collected(v.begin(), v.end());

  // Assert
  EXPECT_EQ(collected.size(), arr.size());
  EXPECT_EQ(collected[0], 2);
  EXPECT_EQ(collected[1], 3);
  EXPECT_EQ(collected[2], 4);
}

//! Ranges reverse: supports std::views::reverse to iterate in reverse order.
NOLINT_TEST(EnumIndexedArrayTest, RangesReverseView)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kThird,
    kCount
  };
  EnumIndexedArray<MyEnum, int> arr {};
  arr[MyEnum::kFirst] = 10;
  arr[MyEnum::kSecond] = 20;
  arr[MyEnum::kThird] = 30;

  // Act: use reverse view to iterate in reverse
  auto v = arr | std::views::reverse;
  std::vector<int> collected(v.begin(), v.end());

  // Assert
  EXPECT_EQ(collected.size(), arr.size());
  EXPECT_EQ(collected[0], 30);
  EXPECT_EQ(collected[1], 20);
  EXPECT_EQ(collected[2], 10);
}

//! Ranges algorithm: std::ranges::find locates values in the container.
NOLINT_TEST(EnumIndexedArrayTest, RangesFindAlgorithm)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kThird,
    kCount
  };
  EnumIndexedArray<MyEnum, int> arr {};
  arr[MyEnum::kFirst] = 5;
  arr[MyEnum::kSecond] = 7;
  arr[MyEnum::kThird] = 9;

  // Act: find value 7
  auto it = std::ranges::find(arr, 7);

  // Assert
  ASSERT_NE(it, arr.end());
  EXPECT_EQ(*it, 7);
}

//! Ranges algorithm: std::ranges::find_if finds elements matching a predicate.
NOLINT_TEST(EnumIndexedArrayTest, RangesFindIfAlgorithm)
{
  // Arrange
  using MyEnum = enum class MyEnum : std::size_t {
    kFirst = 0,
    kSecond,
    kThird,
    kCount
  };
  EnumIndexedArray<MyEnum, int> arr {};
  arr[MyEnum::kFirst] = 2;
  arr[MyEnum::kSecond] = 4;
  arr[MyEnum::kThird] = 6;

  // Act: find first even value greater than 3
  auto it = std::ranges::find_if(
    arr, [](int x) -> bool { return x > 3 && (x % 2) == 0; });

  // Assert
  ASSERT_NE(it, arr.end());
  EXPECT_EQ(*it, 4);
}

//=== Compile time tests ===--------------------------------------------------//

enum class CTEnum : std::size_t {
  kFirst = 0,
  kSecond,

  // ReSharper disable once CppEnumeratorNeverUsed
  kCount,
};

// consteval function that creates and manipulates an EnumIndexedArray
consteval auto ConstevalEnumIndexedArrayChecks() -> bool
{
  // Create a constexpr EnumIndexedArray and populate it.
  EnumIndexedArray<CTEnum, int> me {};
  me[CTEnum::kFirst] = 10;
  me[CTEnum::kSecond] = 20;

  // Use EnumAsIndex consteval Checked<>() to construct an index and read it
  auto idx = EnumAsIndex<CTEnum>::Checked<CTEnum::kFirst>();
  if (me[idx] != 10) {
    return false;
  }

  // Test arithmetic on EnumAsIndex at compile-time
  auto idx2 = idx + 1; // should be second
  if (me[idx2] != 20) {
    return false;
  }

  // Test the enum_as_index view size and iterator distance in consteval
  auto viewSize = enum_as_index<CTEnum>.size();
  // ReSharper disable once CppDFAConstantConditions
  if (viewSize != std::ranges::size(enum_as_index<CTEnum>)) {
    // ReSharper disable once CppDFAUnreachableCode
    return false;
  }

  return true;
}

static_assert(ConstevalEnumIndexedArrayChecks(),
  "Consteval EnumIndexedArray checks failed");

// Additional static_assert coverage: direct constexpr instance initializer
constexpr EnumIndexedArray<CTEnum, int> constexprInit
  = []() constexpr -> EnumIndexedArray<CTEnum, int> {
  EnumIndexedArray<CTEnum, int> a {};
  a[CTEnum::kFirst] = 1;
  a[CTEnum::kSecond] = 2;
  return a;
}();

static_assert(constexprInit[CTEnum::kFirst] == 1, "constexprInit first");
static_assert(constexprInit[CTEnum::kSecond] == 2, "constexprInit second");

} // namespace
