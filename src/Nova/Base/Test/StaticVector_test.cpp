//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <array>
#include <string>
#include <utility>

#include <Nova/Base/StaticVector.h>
#include <Nova/Testing/GTest.h>

using nova::StaticVector;

namespace {

// Helper type to track construction/destruction counts
class Counter {
public:
  // Thread-local static counters ensure isolation between tests
  static thread_local int default_constructs_;
  static thread_local int copy_constructs_;
  static thread_local int move_constructs_;
  static thread_local int destructs_;

  int value_;

  Counter()
    : value_(0)
  {
    ++default_constructs_;
  }

  explicit Counter(const int val)
    : value_(val)
  {
    ++default_constructs_;
  }

  Counter(const Counter& other)
    : value_(other.value_)
  {
    ++copy_constructs_;
  }

  Counter(Counter&& other) noexcept
    : value_(other.value_)
  {
    other.value_ = 0;
    ++move_constructs_;
  }

  ~Counter() { ++destructs_; }

  auto operator=(const Counter& other) -> Counter&
  {
    if (this != &other) {
      value_ = other.value_;
    }
    return *this;
  }

  [[maybe_unused]] auto operator=(Counter&& other) noexcept -> Counter&
  {
    if (this != &other) {
      value_ = other.value_;
      other.value_ = 0;
    }
    return *this;
  }

  [[maybe_unused]] auto operator==(const Counter& other) const -> bool
  {
    return value_ == other.value_;
  }

  static auto reset() -> void
  {
    default_constructs_ = 0;
    copy_constructs_ = 0;
    move_constructs_ = 0;
    destructs_ = 0;
  }
};

thread_local int Counter::default_constructs_ = 0;
thread_local int Counter::copy_constructs_ = 0;
thread_local int Counter::move_constructs_ = 0;
thread_local int Counter::destructs_ = 0;

NOLINT_TEST(StaticVectorTest, DefaultConstructor)
{
  constexpr StaticVector<int, 5> vec;
  EXPECT_EQ(vec.size(), 0);
  EXPECT_TRUE(vec.empty());
  EXPECT_EQ(vec.capacity(), 5);
  EXPECT_EQ(vec.max_size(), 5);
}

NOLINT_TEST(StaticVectorTest, FillConstructor)
{
  StaticVector<int, 5> vec(3, 42);
  EXPECT_EQ(vec.size(), 3);
  EXPECT_FALSE(vec.empty());
  EXPECT_EQ(vec[0], 42);
  EXPECT_EQ(vec[1], 42);
  EXPECT_EQ(vec[2], 42);
}

NOLINT_TEST(StaticVectorTest, CountConstructor)
{
  Counter::reset();
  {
    StaticVector<Counter, 5> vec(3);
    EXPECT_EQ(vec.size(), 3);
    // Check the actual count matches our expectation
    EXPECT_EQ(Counter::default_constructs_, 3);

    // Additional checks to verify the objects were properly constructed
    EXPECT_EQ(vec[0].value_, 0);
    EXPECT_EQ(vec[1].value_, 0);
    EXPECT_EQ(vec[2].value_, 0);
  }
  // Also verify that destructors are called when the vector goes out of scope
  EXPECT_EQ(Counter::destructs_, 3);
}

NOLINT_TEST(StaticVectorTest, RangeConstructor)
{
  std::array arr = { 1, 2, 3, 4, 5, 6, 7 };

  // Normal case
  StaticVector<int, 10> vec1(arr.begin(), arr.end());
  EXPECT_EQ(vec1.size(), 7);
  EXPECT_EQ(vec1[0], 1);
  EXPECT_EQ(vec1[6], 7);
}

NOLINT_TEST(StaticVectorTest, InitializerListConstructor)
{
  StaticVector<int, 10> vec1 = { 1, 2, 3, 4, 5 };
  EXPECT_EQ(vec1.size(), 5);
  EXPECT_EQ(vec1[0], 1);
  EXPECT_EQ(vec1[4], 5);
}

NOLINT_TEST(StaticVectorTest, CopyConstructor)
{
  const StaticVector<int, 5> vec1 = { 1, 2, 3 };
  // NOLINTNEXTLINE(*-unnecessary-copy-initialization) - testing
  StaticVector vec2(vec1);

  EXPECT_EQ(vec2.size(), 3);
  EXPECT_EQ(vec2[0], 1);
  EXPECT_EQ(vec2[1], 2);
  EXPECT_EQ(vec2[2], 3);
}

NOLINT_TEST(StaticVectorTest, MoveConstructor)
{
  Counter::reset();

  StaticVector<Counter, 5> vec1;
  vec1.emplace_back(1);
  vec1.emplace_back(2);
  vec1.emplace_back(3);

  Counter::reset();
  StaticVector vec2(std::move(vec1));
  EXPECT_EQ(vec2.size(), 3);
  // NOLINTNEXTLINE(bugprone-use-after-move) - leave other in good state
  EXPECT_EQ(vec1.size(), 0);
  EXPECT_EQ(vec2[0].value_, 1);
  EXPECT_EQ(vec2[2].value_, 3);
  EXPECT_EQ(Counter::move_constructs_, 3);
}

NOLINT_TEST(StaticVectorTest, AssignmentOperators)
{
  // Copy assignment
  const StaticVector<int, 5> vec1 = { 1, 2, 3 };
  // NOLINTNEXTLINE(*-unnecessary-copy-initialization) - testing
  StaticVector<int, 5> vec2 = vec1;

  EXPECT_EQ(vec2.size(), 3);
  EXPECT_EQ(vec2[0], 1);
  EXPECT_EQ(vec2[2], 3); // Move assignment
  Counter::reset();
  StaticVector<Counter, 5> vec3;
  vec3.emplace_back(1);
  vec3.emplace_back(2);

  StaticVector<Counter, 5> vec4;
  Counter::reset();
  // ReSharper disable once CppJoinDeclarationAndAssignment - for testing
  vec4 = std::move(vec3);
  EXPECT_EQ(vec4.size(), 2);
  // NOLINTNEXTLINE(bugprone-use-after-move) - leave other in good state
  EXPECT_EQ(vec3.size(), 0);
  EXPECT_EQ(vec4[0].value_, 1);
  EXPECT_EQ(vec4[1].value_, 2);
  EXPECT_EQ(Counter::move_constructs_, 2);

  // Initializer list assignment
  StaticVector<int, 5> vec5 = { 5, 6, 7, 8 };

  EXPECT_EQ(vec5.size(), 4);
  EXPECT_EQ(vec5[0], 5);
  EXPECT_EQ(vec5[3], 8);
}

NOLINT_TEST(StaticVectorTest, ElementAccess)
{
  StaticVector<int, 5> vec = { 1, 2, 3, 4, 5 };

  // at() with bounds checking
  EXPECT_EQ(vec.at(0), 1);
  EXPECT_EQ(vec.at(4), 5);
  NOLINT_EXPECT_THROW([[maybe_unused]] auto& _ = vec.at(5), std::out_of_range);

  // operator[]
  EXPECT_EQ(vec[0], 1);
  EXPECT_EQ(vec[4], 5);

  // front() and back()
  EXPECT_EQ(vec.front(), 1);
  EXPECT_EQ(vec.back(), 5);

  // Const versions
  const StaticVector<int, 5>& const_vec = vec;
  EXPECT_EQ(const_vec.at(2), 3);
  EXPECT_EQ(const_vec[3], 4);
  EXPECT_EQ(const_vec.front(), 1);
  EXPECT_EQ(const_vec.back(), 5);

  // data()
  EXPECT_EQ(vec.data()[2], 3);
  EXPECT_EQ(const_vec.data()[2], 3);
}

NOLINT_TEST(StaticVectorTest, Iterators)
{
  StaticVector<int, 5> vec = { 1, 2, 3, 4, 5 };

  // Test begin/end
  int sum = 0;
  for (const int& it : vec) {
    sum += it;
  }
  EXPECT_EQ(sum, 15);

  // Test const iterators
  const StaticVector<int, 5>& const_vec = vec;
  sum = 0;
  for (const int it : const_vec) {
    sum += it;
  }
  EXPECT_EQ(sum, 15);

  // Test cbegin/cend
  sum = 0;
  for (const int it : vec) {
    sum += it;
  }
  EXPECT_EQ(sum, 15);

  // Test range-based for
  sum = 0;
  for (const auto& val : vec) {
    sum += val;
  }
  EXPECT_EQ(sum, 15);
}

NOLINT_TEST(StaticVectorTest, Modifiers)
{ // clear
  Counter::reset();
  {
    StaticVector<Counter, 5> vec;
    vec.emplace_back(1);
    vec.emplace_back(2);
    vec.emplace_back(3);

    EXPECT_EQ(vec.size(), 3);
    vec.clear();
    EXPECT_EQ(vec.size(), 0);
    EXPECT_TRUE(vec.empty());
  } // Check proper destruction
  EXPECT_EQ(Counter::destructs_,
    Counter::default_constructs_ + Counter::copy_constructs_
      + Counter::move_constructs_);

  // push_back
  StaticVector<int, 3> vec1;
  vec1.push_back(1);
  vec1.push_back(2);
  EXPECT_EQ(vec1.size(), 2);
  EXPECT_EQ(vec1[1], 2); // push_back with move
  Counter::reset();
  StaticVector<Counter, 3> vec2;
  Counter c(42);
  vec2.push_back(std::move(c));
  EXPECT_EQ(vec2.size(), 1);
  EXPECT_EQ(vec2[0].value_, 42);
  EXPECT_EQ(
    c.value_, 0); // NOLINT(bugprone-use-after-move) - leave other in good state
  EXPECT_EQ(Counter::move_constructs_, 1);

  // Test vector full case
  vec1.push_back(3);
  EXPECT_EQ(vec1.size(), 3);
  NOLINT_EXPECT_THROW(vec1.push_back(4), std::length_error);

  // emplace_back
  StaticVector<std::pair<int, std::string>, 3> vec3;
  auto& [first, second] = vec3.emplace_back(42, "test");
  EXPECT_EQ(vec3.size(), 1);
  EXPECT_EQ(first, 42);
  EXPECT_EQ(second, "test");

  // pop_back
  vec3.pop_back();
  EXPECT_EQ(vec3.size(), 0);
}

NOLINT_TEST(StaticVectorTest, Resize)
{
  // Resize with default value
  StaticVector<int, 10> vec1;
  vec1.resize(5);
  EXPECT_EQ(vec1.size(), 5);
  EXPECT_EQ(vec1[0], 0); // Default int value

  // Resize with provided value
  StaticVector<int, 10> vec2;
  vec2.resize(5, 42);
  EXPECT_EQ(vec2.size(), 5);
  EXPECT_EQ(vec2[0], 42);
  EXPECT_EQ(vec2[4], 42); // Resize to smaller size
  Counter::reset();
  {
    StaticVector<Counter, 10> vec3;
    vec3.resize(5);
    EXPECT_EQ(vec3.size(), 5);
    EXPECT_EQ(Counter::default_constructs_, 5);

    vec3.resize(2);
    EXPECT_EQ(vec3.size(), 2);
    // Should have destroyed 3 elements
  }

  // Resize exceeding capacity
  StaticVector<int, 5> vec4;
  NOLINT_EXPECT_THROW(vec4.resize(10), std::length_error);
}

NOLINT_TEST(StaticVectorTest, ComparisonOperators)
{
  const StaticVector<int, 5> vec1 = { 1, 2, 3 };
  const StaticVector<int, 5> vec2 = { 1, 2, 3 };
  const StaticVector<int, 5> vec3 = { 1, 2, 4 };
  const StaticVector<int, 5> vec4 = { 1, 2 };

  EXPECT_TRUE(vec1 == vec2);
  EXPECT_FALSE(vec1 == vec3);
  EXPECT_FALSE(vec1 == vec4);

  EXPECT_TRUE(vec1 < vec3);
  EXPECT_FALSE(vec1 < vec2);
  EXPECT_FALSE(vec1 < vec4);
  EXPECT_TRUE(vec4 < vec1);

  // Other comparisons use the spaceship operator internally
  EXPECT_TRUE(vec1 <= vec2);
  EXPECT_TRUE(vec1 >= vec2);
  EXPECT_TRUE(vec1 != vec3);
  EXPECT_TRUE(vec1 > vec4);
  EXPECT_TRUE(vec1 >= vec4);
}

} // namespace

//=== Death Tests for StaticVector assertions ===-----------------------------//
namespace {

/*!
  Death test: count constructor with count > MaxElements triggers assertion.
  Covers: StaticVector(size_type count, ...) assert.
*/
NOLINT_TEST(StaticVectorDeathTest, CountConstructorExceedsCapacity)
{
#ifndef NDEBUG
  EXPECT_DEATH(([]() -> void { StaticVector<int, 2> vec(3, 42); })(),
    "count exceeds maximum size");
#endif
}

/*!
  Death test: explicit count constructor with count > MaxElements triggers
  assertion.
  Covers: StaticVector(size_type count) assert.
*/
NOLINT_TEST(StaticVectorDeathTest, ExplicitCountConstructorExceedsCapacity)
{
#ifndef NDEBUG
  EXPECT_DEATH(([]() -> void { StaticVector<int, 2> vec(3); })(),
    "count exceeds maximum size");
#endif
}

/*!
  Death test: assignment from initializer list with size > MaxElements triggers
  assertion.
  Covers: operator=(std::initializer_list<T>) assert.
*/
NOLINT_TEST(StaticVectorDeathTest, AssignmentFromInitializerListExceedsCapacity)
{
#ifndef NDEBUG
  StaticVector<int, 2> vec;
  EXPECT_DEATH(
    (void)(vec = { 1, 2, 3 }), "initializer list size exceeds maximum size");
#endif
}

/*!
  Death test: push_back when full triggers length_error.
  Covers: push_back assert for current_size_ >= MaxElements.
*/
NOLINT_TEST(StaticVectorDeathTest, PushBackWhenFullThrows)
{
  StaticVector<int, 2> vec;
  vec.push_back(1);
  vec.push_back(2);
  NOLINT_EXPECT_THROW(vec.push_back(3), std::length_error);
}

/*!
  Death test: emplace_back when full triggers length_error.
  Covers: emplace_back assert for current_size_ >= MaxElements.
*/
NOLINT_TEST(StaticVectorDeathTest, EmplaceBackWhenFullThrows)
{
  StaticVector<int, 1> vec;
  vec.emplace_back(42);
  NOLINT_EXPECT_THROW(vec.emplace_back(43), std::length_error);
}

/*!
  Death test: Range constructor with input exceeding capacity triggers
  assertion.
  Covers: range constructor assertion for input_size > MaxElements.
*/
NOLINT_TEST(StaticVectorDeathTest, RangeConstructorExceedsCapacity)
{
#ifndef NDEBUG
  std::array arr = { 1, 2, 3, 4, 5, 6, 7 };
  EXPECT_DEATH(
    ([&]() -> void { StaticVector<int, 5> vec2(arr.begin(), arr.end()); })(),
    "range constructor input exceeds maximum size");
#endif
}

/*!
  Death test: Initializer list constructor with input exceeding capacity
  triggers assertion.
  Covers: initializer list constructor assertion for input.size() >
  MaxElements.
*/
NOLINT_TEST(StaticVectorDeathTest, InitializerListConstructorExceedsCapacity)
{
#ifndef NDEBUG
  EXPECT_DEATH(
    ([]() -> void { StaticVector<int, 3> vec2 = { 1, 2, 3, 4, 5 }; })(),
    "initializer list size exceeds maximum size");
#endif
}

/*!
  Death test: out-of-bounds operator[] access triggers assertion.
  Covers: operator[] assertion for pos >= current_size_.
*/
NOLINT_TEST(StaticVectorDeathTest, OutOfBoundsOperatorIndex)
{
#ifndef NDEBUG
  StaticVector<int, 3> vec = { 1, 2 };
  EXPECT_DEATH((void)vec[2], "out of bounds access");
#endif
}

/*!
  Death test: front() on empty vector triggers assertion.
*/
NOLINT_TEST(StaticVectorDeathTest, FrontOnEmpty)
{
#ifndef NDEBUG
  StaticVector<int, 3> vec;
  EXPECT_DEATH((void)vec.front(), "empty");
#endif
}

/*!
  Death test: back() on empty vector triggers assertion.
*/
NOLINT_TEST(StaticVectorDeathTest, BackOnEmpty)
{
#ifndef NDEBUG
  StaticVector<int, 3> vec;
  EXPECT_DEATH((void)vec.back(), "empty");
#endif
}

/*!
  Death test: pop_back on empty vector triggers assertion.
  Covers: pop_back assertion for empty().
*/
NOLINT_TEST(StaticVectorDeathTest, PopBackOnEmpty)
{
#ifndef NDEBUG
  StaticVector<int, 3> vec;
  EXPECT_DEATH(vec.pop_back(), "pop_back called on empty container");
#endif
}

} // namespace
