//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

// Based on NamedType, Copyright (c) 2017 Jonathan Boccara
// License: MIT
// https://github.com/joboccara/NamedType

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <Nova/Testing/GTest.h>

#include <Nova/Base/NamedType.h>

// Usage examples

template <typename T> auto tee(T&& value) -> decltype(auto)
{
  std::cout << value << '\n';
  return std::forward<T>(value);
}

using Meter = nova::NamedType<unsigned long long, struct MeterParameter,
  nova::Addable, nova::Comparable>;
constexpr auto operator""_meter(unsigned long long value) -> Meter
{
  return Meter(value);
}

using Width = nova::NamedType<Meter, struct WidthParameter>;
using Height = nova::NamedType<Meter, struct HeightParameter>;

class Rectangle {
public:
  Rectangle(Width width, Height height)
    : width_(width.get())
    , height_(height.get())
  {
  }
  auto getWidth() const -> Meter { return width_; }
  auto getHeight() const -> Meter { return height_; }

private:
  Meter width_;
  Meter height_;
};

namespace { // Anonymous namespace for tests

// Fixtures to group tests by category.
class NamedTypeBasicTest : public ::testing::Test { };
class NamedTypeSkillsTest : public ::testing::Test { };
class NamedTypeNamedArgsTest : public ::testing::Test { };
class NamedTypeHashTest : public ::testing::Test { };

//! Basic usage of NamedType with a Rectangle wrapper.
NOLINT_TEST_F(NamedTypeBasicTest, BasicUsage)
{
  Rectangle r(Width(10_meter), Height(12_meter));
  EXPECT_EQ(10ULL, r.getWidth().get());
  EXPECT_EQ(12ULL, r.getHeight().get());
}

using NameRef = nova::NamedType<std::string&, struct NameRefParameter>;

void changeValue(NameRef name) { name.get() = "value2"; }

//! Passing a strong reference allows mutation of the underlying value.
NOLINT_TEST_F(NamedTypeBasicTest, PassingStrongReference)
{
  std::string value = "value1";
  changeValue(NameRef(value));
  EXPECT_EQ(std::string("value2"), value);
}

//! Construct a NamedType::ref from the underlying type.
NOLINT_TEST_F(NamedTypeBasicTest, RefFromUnderlying)
{
  using StrongInt = nova::NamedType<int, struct StrongIntTag>;
  auto addOne = [](StrongInt::ref si) -> void { ++(si.get()); };

  int i = 42;
  addOne(StrongInt::ref(i));
  EXPECT_EQ(43, i);
}

//! Implicit conversion from NamedType to NamedType::ref for lvalues.
NOLINT_TEST_F(NamedTypeBasicTest, ImplicitConversionToRef)
{
  using StrongInt = nova::NamedType<int, struct StrongIntTag>;
  auto addOne = [](StrongInt::ref si) -> void { ++(si.get()); };

  StrongInt i(42);
  addOne(i);
  EXPECT_EQ(43, i.get());

  StrongInt j(42);
  addOne(StrongInt::ref(j));
  EXPECT_EQ(43, j.get());
}

//! Ref conversion must only bind to lvalues, not rvalues.
NOLINT_TEST_F(NamedTypeBasicTest, RefConversionOnLvaluesOnly)
{
  using StrongInt = nova::NamedType<int, struct StrongIntTag>;
  auto read = [](StrongInt::ref si) -> int { return si.get(); };

  StrongInt si(7);
  EXPECT_EQ(7, read(si));

  // rvalues must not bind to ref-views
  static_assert(!std::is_convertible_v<StrongInt&&, StrongInt::ref>,
    "rvalue should not convert to ref");
}

//! NamedType preserves triviality and size of underlying type.
NOLINT_TEST_F(NamedTypeBasicTest, TrivialityAndSizeUnchanged)
{
  using StrongInt = nova::NamedType<int, struct TrivialityTag>;
  static_assert(std::is_trivially_constructible_v<StrongInt>,
    "StrongInt must remain trivially constructible");
  static_assert(
    sizeof(StrongInt) == sizeof(int), "EBCO or storage must not change size");
}

struct PotentiallyThrowing {
  PotentiallyThrowing() { }
};

struct NonDefaultConstructible {
  NonDefaultConstructible(int) { }
};

struct UserProvided {
  UserProvided();
};
UserProvided::UserProvided() = default;

//! Default construction characteristics propagate from underlying type.
NOLINT_TEST_F(NamedTypeBasicTest, DefaultConstruction)
{
  using StrongInt = nova::NamedType<int, struct StrongIntTag>;
  StrongInt strongInt;
  strongInt.get() = 42;
  EXPECT_EQ(42, strongInt.get());
  static_assert(std::is_nothrow_constructible_v<StrongInt>,
    "StrongInt is not nothrow constructible");

  // Default constructible
  static_assert(std::is_default_constructible_v<StrongInt>,
    "StrongInt is not default constructible");
  using StrongNonDefaultConstructible = nova::NamedType<NonDefaultConstructible,
    struct StrongNonDefaultConstructibleTag>;
  static_assert(!std::is_default_constructible_v<StrongNonDefaultConstructible>,
    "StrongNonDefaultConstructible is default constructible");

  // Trivially constructible
  static_assert(std::is_trivially_constructible_v<StrongInt>,
    "StrongInt is not trivially constructible");
  using StrongUserProvided
    = nova::NamedType<UserProvided, struct StrongUserProvidedTag>;
  static_assert(!std::is_trivially_constructible_v<StrongUserProvided>,
    "StrongUserProvided is trivially constructible");

  // Nothrow constructible
  static_assert(std::is_nothrow_constructible_v<StrongInt>,
    "StrongInt is not nothrow constructible");
  using StrongPotentiallyThrowing
    = nova::NamedType<PotentiallyThrowing, struct StrongPotentiallyThrowingTag>;
  static_assert(!std::is_nothrow_constructible_v<StrongPotentiallyThrowing>,
    "StrongPotentiallyThrowing is nothrow constructible");
}

//! DefaultInitialized zero-initializes arithmetic types by default.
NOLINT_TEST_F(NamedTypeBasicTest, DefaultInitializedZeroesArithmetic)
{
  using ZeroedInt
    = nova::NamedType<int, struct ZeroedIntTag, nova::DefaultInitialized>;
  ZeroedInt zi;
  EXPECT_EQ(0, zi.get());

  // Still constructible as before
  static_assert(std::is_default_constructible_v<ZeroedInt>,
    "ZeroedInt should be default constructible");
}

//! DefaultInitialized does not add default-constructibility where not present.
NOLINT_TEST_F(NamedTypeBasicTest, DefaultInitializedDoesntAddDefaultCtor)
{
  using StrongNonDefaultInit = nova::NamedType<NonDefaultConstructible,
    struct StrongNonDefaultInitTag, nova::DefaultInitialized>;
  static_assert(!std::is_default_constructible_v<StrongNonDefaultInit>,
    "Should not become default constructible");
}

//! DefaultInitialized default-constructs containers into empty state.
NOLINT_TEST_F(NamedTypeBasicTest, DefaultInitializedContainersEmpty)
{
  using Vec = std::vector<int>;
  using ZeroedVec
    = nova::NamedType<Vec, struct ZeroedVecTag, nova::DefaultInitialized>;
  ZeroedVec zv;
  EXPECT_TRUE(zv.get().empty());

  // Without the skill, behavior stays as before.
  using PlainVec = nova::NamedType<Vec, struct PlainVecTag>;
  PlainVec pv;
  EXPECT_TRUE(pv.get().empty());
}

//! DefaultInitialized zero-initializes std::array elements.
NOLINT_TEST_F(NamedTypeBasicTest, DefaultInitializedZeroesStdArray)
{
  using Arr = std::array<int, 4>;
  using ZeroedArr
    = nova::NamedType<Arr, struct ZeroedArrTag, nova::DefaultInitialized>;
  ZeroedArr za;
  auto const& a = za.get();
  EXPECT_EQ(0, a[0]);
  EXPECT_EQ(0, a[1]);
  EXPECT_EQ(0, a[2]);
  EXPECT_EQ(0, a[3]);
}

template <typename Function>
using Comparator = nova::NamedType<Function, struct ComparatorParameter>;

template <typename Function>
auto performAction(Comparator<Function> comp) -> std::string
{
  return comp.get()();
}

//! NamedType can hold callables and forward invocation.
NOLINT_TEST_F(NamedTypeBasicTest, StrongGenericType)
{
  EXPECT_EQ(std::string("compare"),
    performAction(nova::MakeNamed<Comparator>(
      []() -> std::string { return std::string("compare"); })));
}

//! Addable skill supports + and unary +.
NOLINT_TEST_F(NamedTypeSkillsTest, Addable)
{
  using AddableType = nova::NamedType<int, struct AddableTag, nova::Addable>;
  AddableType s1(12);
  AddableType s2(10);
  EXPECT_EQ(22, (s1 + s2).get());
  EXPECT_EQ(12, (+s1).get());
}

//! Addable skill is constexpr-enabled.
NOLINT_TEST_F(NamedTypeSkillsTest, AddableConstexpr)
{
  using AddableType = nova::NamedType<int, struct AddableTag, nova::Addable>;
  constexpr AddableType s1(12);
  constexpr AddableType s2(10);
  static_assert((s1 + s2).get() == 22, "Addable is not constexpr");
  static_assert((+s1).get() == 12, "Addable is not constexpr");
}

//! BinaryAddable supports binary + and is noexcept.
NOLINT_TEST_F(NamedTypeSkillsTest, BinaryAddable)
{
  using BinaryAddableType
    = nova::NamedType<int, struct BinaryAddableTag, nova::BinaryAddable>;
  BinaryAddableType s1(12);
  BinaryAddableType s2(10);
  EXPECT_EQ(22, (s1 + s2).get());
  EXPECT_TRUE(noexcept(s1 + s2));
}

//! BinaryAddable supports constexpr binary +.
NOLINT_TEST_F(NamedTypeSkillsTest, BinaryAddableConstexprAssign)
{
  using BinaryAddableType
    = nova::NamedType<int, struct BinaryAddableTag, nova::BinaryAddable>;
  constexpr BinaryAddableType s1(12);
  constexpr BinaryAddableType s2(10);
  static_assert((s1 + s2).get() == 22, "BinaryAddable is not constexpr");
}

//! BinaryAddable supports constexpr operator+=.
NOLINT_TEST_F(NamedTypeSkillsTest, BinaryAddableConstexpr)
{
  using BinaryAddableType
    = nova::NamedType<int, struct BinaryAddableTag, nova::BinaryAddable>;
  constexpr BinaryAddableType s(10);
  static_assert(BinaryAddableType { 12 }.operator+=(s).get() == 22,
    "BinaryAddable is not constexpr");
}

//! UnaryAddable supports unary +.
NOLINT_TEST_F(NamedTypeSkillsTest, UnaryAddable)
{
  using UnaryAddableType
    = nova::NamedType<int, struct UnaryAddableTag, nova::UnaryAddable>;
  UnaryAddableType s1(12);
  EXPECT_EQ(12, (+s1).get());
}

//! UnaryAddable is constexpr-enabled.
NOLINT_TEST_F(NamedTypeSkillsTest, UnaryAddableConstexpr)
{
  using UnaryAddableType
    = nova::NamedType<int, struct UnaryAddableTag, nova::UnaryAddable>;
  constexpr UnaryAddableType s1(12);
  static_assert((+s1).get() == 12, "UnaryAddable is not constexpr");
}

//! Subtractable supports - and unary -.
NOLINT_TEST_F(NamedTypeSkillsTest, Subtractable)
{
  using SubtractableType
    = nova::NamedType<int, struct SubtractableTag, nova::Subtractable>;
  SubtractableType s1(12);
  SubtractableType s2(10);
  EXPECT_EQ(2, (s1 - s2).get());
  EXPECT_EQ(-12, (-s1).get());
}

//! Subtractable is constexpr-enabled.
NOLINT_TEST_F(NamedTypeSkillsTest, SubtractableConstexpr)
{
  using SubtractableType
    = nova::NamedType<int, struct SubtractableTag, nova::Subtractable>;
  constexpr SubtractableType s1(12);
  constexpr SubtractableType s2(10);
  static_assert((s1 - s2).get() == 2, "Subtractable is not constexpr");
  static_assert((-s1).get() == -12, "Subtractable is not constexpr");
}

//! BinarySubtractable supports binary -.
NOLINT_TEST_F(NamedTypeSkillsTest, BinarySubtractable)
{
  using BinarySubtractableType = nova::NamedType<int,
    struct BinarySubtractableTag, nova::BinarySubtractable>;
  BinarySubtractableType s1(12);
  BinarySubtractableType s2(10);
  EXPECT_EQ(2, (s1 - s2).get());
}

//! BinarySubtractable supports constexpr binary -.
NOLINT_TEST_F(NamedTypeSkillsTest, BinarySubtractableConstexprAssign)
{
  using BinarySubtractableType = nova::NamedType<int,
    struct BinarySubtractableTag, nova::BinarySubtractable>;
  constexpr BinarySubtractableType s1(12);
  constexpr BinarySubtractableType s2(10);
  static_assert((s1 - s2).get() == 2, "BinarySubtractable is not constexpr");
}

//! BinarySubtractable supports constexpr operator-=.
NOLINT_TEST_F(NamedTypeSkillsTest, BinarySubtractableConstexpr)
{
  using BinarySubtractableType = nova::NamedType<int,
    struct BinarySubtractableTag, nova::BinarySubtractable>;
  constexpr BinarySubtractableType s(10);
  static_assert(BinarySubtractableType { 12 }.operator-=(s).get() == 2,
    "BinarySubtractable is not constexpr");
}

//! UnarySubtractable supports unary -.
NOLINT_TEST_F(NamedTypeSkillsTest, UnarySubtractable)
{
  using UnarySubtractableType = nova::NamedType<int,
    struct UnarySubtractableTag, nova::UnarySubtractable>;
  UnarySubtractableType s(12);
  EXPECT_EQ(-12, (-s).get());
}

//! UnarySubtractable is constexpr-enabled.
NOLINT_TEST_F(NamedTypeSkillsTest, UnarySubtractableConstexpr)
{
  using UnarySubtractableType = nova::NamedType<int,
    struct UnarySubtractableTag, nova::UnarySubtractable>;
  constexpr UnarySubtractableType s(12);
  static_assert((-s).get() == -12, "UnarySubtractable is not constexpr");
}

//! Multiplicable supports * and operator*=.
NOLINT_TEST_F(NamedTypeSkillsTest, Multiplicable)
{
  using MultiplicableType
    = nova::NamedType<int, struct MultiplicableTag, nova::Multiplicable>;
  MultiplicableType s1(12);
  MultiplicableType s2(10);
  EXPECT_EQ(120, (s1 * s2).get());
  s1 *= s2;
  EXPECT_EQ(120, s1.get());
}

//! Multiplicable supports constexpr binary *.
NOLINT_TEST_F(NamedTypeSkillsTest, MultiplicableConstexprAssign)
{
  using MultiplicableType
    = nova::NamedType<int, struct MultiplicableTag, nova::Multiplicable>;
  constexpr MultiplicableType s1(12);
  constexpr MultiplicableType s2(10);
  static_assert((s1 * s2).get() == 120, "Multiplicable is not constexpr");
}

//! Multiplicable supports constexpr operator*=.
NOLINT_TEST_F(NamedTypeSkillsTest, MultiplicableConstexpr)
{
  using MultiplicableType
    = nova::NamedType<int, struct MultiplicableTag, nova::Multiplicable>;
  constexpr MultiplicableType s(10);
  static_assert(MultiplicableType { 12 }.operator*=(s).get() == 120,
    "Multiplicable is not constexpr");
}

//! Divisible supports / and operator/=.
NOLINT_TEST_F(NamedTypeSkillsTest, Divisible)
{
  using DivisibleType
    = nova::NamedType<int, struct DivisibleTag, nova::Divisible>;
  DivisibleType s1(120);
  DivisibleType s2(10);
  EXPECT_EQ(12, (s1 / s2).get());
  s1 /= s2;
  EXPECT_EQ(12, s1.get());
}

//! Divisible supports constexpr binary /.
NOLINT_TEST_F(NamedTypeSkillsTest, DivisibleConstexprAssign)
{
  using DivisibleType
    = nova::NamedType<int, struct DivisibleTag, nova::Divisible>;
  constexpr DivisibleType s1(120);
  constexpr DivisibleType s2(10);
  static_assert((s1 / s2).get() == 12, "Divisible is not constexpr");
}

//! Divisible supports constexpr operator/=.
NOLINT_TEST_F(NamedTypeSkillsTest, DivisibleConstexpr)
{
  using DivisibleType
    = nova::NamedType<int, struct DivisibleTag, nova::Divisible>;
  constexpr DivisibleType s(10);
  static_assert(DivisibleType { 120 }.operator/=(s).get() == 12,
    "Divisible is not constexpr");
}

//! Modulable supports % and operator%=.
NOLINT_TEST_F(NamedTypeSkillsTest, Modulable)
{
  using ModulableType
    = nova::NamedType<int, struct ModulableTag, nova::Modulable>;
  ModulableType s1(5);
  ModulableType s2(2);
  EXPECT_EQ(1, (s1 % s2).get());
  s1 %= s2;
  EXPECT_EQ(1, s1.get());
}

//! Modulable supports constexpr binary %.
NOLINT_TEST_F(NamedTypeSkillsTest, ModulableConstexprAssign)
{
  using ModulableType
    = nova::NamedType<int, struct ModulableTag, nova::Modulable>;
  constexpr ModulableType s1(5);
  constexpr ModulableType s2(2);
  static_assert((s1 % s2).get() == 1, "Modulable is not constexpr");
}

//! Modulable supports constexpr operator%=.
NOLINT_TEST_F(NamedTypeSkillsTest, ModulableConstexpr)
{
  using ModulableType
    = nova::NamedType<int, struct ModulableTag, nova::Modulable>;
  constexpr ModulableType s(2);
  static_assert(
    ModulableType { 5 }.operator%=(s).get() == 1, "Modulable is not constexpr");
}

//! BitWiseInvertable supports bitwise ~.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseInvertable)
{
  using BitWiseInvertableType = nova::NamedType<int,
    struct BitWiseInvertableTag, nova::BitWiseInvertable>;
  BitWiseInvertableType s1(13);
  EXPECT_EQ((~13), (~s1).get());
}

//! BitWiseInvertable is constexpr-enabled.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseInvertableConstexpr)
{
  using BitWiseInvertableType = nova::NamedType<int,
    struct BitWiseInvertableTag, nova::BitWiseInvertable>;
  constexpr BitWiseInvertableType s1(13);
  static_assert((~s1).get() == (~13), "BitWiseInvertable is not constexpr");
}

//! BitWiseAndable supports & and operator&=.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseAndable)
{
  using BitWiseAndableType
    = nova::NamedType<int, struct BitWiseAndableTag, nova::BitWiseAndable>;
  BitWiseAndableType s1(2);
  BitWiseAndableType s2(64);
  EXPECT_EQ((2 & 64), (s1 & s2).get());
  s1 &= s2;
  EXPECT_EQ((2 & 64), s1.get());
}

//! BitWiseAndable supports constexpr binary &.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseAndableConstexprAssign)
{
  using BitWiseAndableType
    = nova::NamedType<int, struct BitWiseAndableTag, nova::BitWiseAndable>;
  constexpr BitWiseAndableType s1(2);
  constexpr BitWiseAndableType s2(64);
  static_assert((s1 & s2).get() == (2 & 64), "BitWiseAndable is not constexpr");
}

//! BitWiseAndable supports constexpr operator&=.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseAndableConstexpr)
{
  using BitWiseAndableType
    = nova::NamedType<int, struct BitWiseAndableTag, nova::BitWiseAndable>;
  constexpr BitWiseAndableType s(64);
  static_assert(BitWiseAndableType { 2 }.operator&=(s).get() == (2 & 64),
    "BitWiseAndable is not constexpr");
}

//! BitWiseOrable supports | and operator|=.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseOrable)
{
  using BitWiseOrableType
    = nova::NamedType<int, struct BitWiseOrableTag, nova::BitWiseOrable>;
  BitWiseOrableType s1(2);
  BitWiseOrableType s2(64);
  EXPECT_EQ((2 | 64), (s1 | s2).get());
  s1 |= s2;
  EXPECT_EQ((2 | 64), s1.get());
}

//! BitWiseOrable supports constexpr binary |.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseOrableConstexprAssign)
{
  using BitWiseOrableType
    = nova::NamedType<int, struct BitWiseOrableTag, nova::BitWiseOrable>;
  constexpr BitWiseOrableType s1(2);
  constexpr BitWiseOrableType s2(64);
  static_assert((s1 | s2).get() == (2 | 64), "BitWiseOrable is not constexpr");
}

//! BitWiseOrable supports constexpr operator|=.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseOrableConstexpr)
{
  using BitWiseOrableType
    = nova::NamedType<int, struct BitWiseOrableTag, nova::BitWiseOrable>;
  constexpr BitWiseOrableType s(64);
  static_assert(BitWiseOrableType { 2 }.operator|=(s).get() == (2 | 64),
    "BitWiseOrable is not constexpr");
}

//! BitWiseXorable supports ^ and operator^=.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseXorable)
{
  using BitWiseXorableType
    = nova::NamedType<int, struct BitWiseXorableTag, nova::BitWiseXorable>;
  BitWiseXorableType s1(2);
  BitWiseXorableType s2(64);
  EXPECT_EQ((2 ^ 64), (s1 ^ s2).get());
  s1 ^= s2;
  EXPECT_EQ((2 ^ 64), s1.get());
}

//! BitWiseXorable supports constexpr binary ^.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseXorableConstexprAssign)
{
  using BitWiseXorableType
    = nova::NamedType<int, struct BitWiseXorableTag, nova::BitWiseXorable>;
  constexpr BitWiseXorableType s1(2);
  constexpr BitWiseXorableType s2(64);
  static_assert((s1 ^ s2).get() == 66, "BitWiseXorable is not constexpr");
}

//! BitWiseXorable supports constexpr operator^=.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseXorableConstexpr)
{
  using BitWiseXorableType
    = nova::NamedType<int, struct BitWiseXorableTag, nova::BitWiseXorable>;
  constexpr BitWiseXorableType s(64);
  static_assert(BitWiseXorableType { 2 }.operator^=(s).get() == 66,
    "BitWiseXorable is not constexpr");
}

//! BitWiseLeftShiftable supports << and operator<<=.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseLeftShiftable)
{
  using BitWiseLeftShiftableType = nova::NamedType<int,
    struct BitWiseLeftShiftableTag, nova::BitWiseLeftShiftable>;
  BitWiseLeftShiftableType s1(2);
  BitWiseLeftShiftableType s2(3);
  EXPECT_EQ((2 << 3), (s1 << s2).get());
  s1 <<= s2;
  EXPECT_EQ((2 << 3), s1.get());
}

//! BitWiseLeftShiftable supports constexpr binary <<.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseLeftShiftableConstexprAssign)
{
  using BitWiseLeftShiftableType = nova::NamedType<int,
    struct BitWiseLeftShiftableTag, nova::BitWiseLeftShiftable>;
  constexpr BitWiseLeftShiftableType s1(2);
  constexpr BitWiseLeftShiftableType s2(3);
  static_assert(
    (s1 << s2).get() == (2 << 3), "BitWiseLeftShiftable is not constexpr");
}

//! BitWiseLeftShiftable supports constexpr operator<<=.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseLeftShiftableConstexpr)
{
  using BitWiseLeftShiftableType = nova::NamedType<int,
    struct BitWiseLeftShiftableTag, nova::BitWiseLeftShiftable>;
  constexpr BitWiseLeftShiftableType s(3);
  static_assert(BitWiseLeftShiftableType { 2 }.operator<<=(s).get() == (2 << 3),
    "BitWiseLeftShiftable is not constexpr");
}

//! BitWiseRightShiftable supports >> and operator>>=.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseRightShiftable)
{
  using BitWiseRightShiftableType = nova::NamedType<int,
    struct BitWiseRightShiftableTag, nova::BitWiseRightShiftable>;
  BitWiseRightShiftableType s1(2);
  BitWiseRightShiftableType s2(3);
  EXPECT_EQ((2 >> 3), (s1 >> s2).get());
  s1 >>= s2;
  EXPECT_EQ((2 >> 3), s1.get());
}

//! BitWiseRightShiftable supports constexpr binary >>.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseRightShiftableConstexprAssign)
{
  using BitWiseRightShiftableType = nova::NamedType<int,
    struct BitWiseRightShiftableTag, nova::BitWiseRightShiftable>;
  constexpr BitWiseRightShiftableType s1(2);
  constexpr BitWiseRightShiftableType s2(3);
  static_assert(
    (s1 >> s2).get() == (2 >> 3), "BitWiseRightShiftable is not constexpr");
}

//! BitWiseRightShiftable supports constexpr operator>>=.
NOLINT_TEST_F(NamedTypeSkillsTest, BitWiseRightShiftableConstexpr)
{
  using BitWiseRightShiftableType = nova::NamedType<int,
    struct BitWiseRightShiftableTag, nova::BitWiseRightShiftable>;
  constexpr BitWiseRightShiftableType s(3);
  static_assert(
    BitWiseRightShiftableType { 2 }.operator>>=(s).get() == (2 >> 3),
    "BitWiseRightShiftable is not constexpr");
}

//! Comparable supports all relational operators.
NOLINT_TEST_F(NamedTypeSkillsTest, Comparable)
{
  EXPECT_TRUE(10_meter == 10_meter);
  EXPECT_FALSE(10_meter == 11_meter);
  EXPECT_TRUE(10_meter != 11_meter);
  EXPECT_FALSE(10_meter != 10_meter);
  EXPECT_TRUE(10_meter < 11_meter);
  EXPECT_FALSE(10_meter < 10_meter);
  EXPECT_TRUE(10_meter <= 10_meter);
  EXPECT_TRUE(10_meter <= 11_meter);
  EXPECT_FALSE(10_meter <= 9_meter);
  EXPECT_TRUE(11_meter > 10_meter);
  EXPECT_FALSE(10_meter > 11_meter);
  EXPECT_TRUE(11_meter >= 10_meter);
  EXPECT_TRUE(10_meter >= 10_meter);
  EXPECT_FALSE(9_meter >= 10_meter);
}

//! Comparable is constexpr-enabled.
NOLINT_TEST_F(NamedTypeSkillsTest, ComparableConstexpr)
{
  static_assert((10_meter == 10_meter), "Comparable is not constexpr");
  static_assert(!(10_meter == 11_meter), "Comparable is not constexpr");
  static_assert((10_meter != 11_meter), "Comparable is not constexpr");
  static_assert(!(10_meter != 10_meter), "Comparable is not constexpr");
  static_assert((10_meter < 11_meter), "Comparable is not constexpr");
  static_assert(!(10_meter < 10_meter), "Comparable is not constexpr");
  static_assert((10_meter <= 10_meter), "Comparable is not constexpr");
  static_assert((10_meter <= 11_meter), "Comparable is not constexpr");
  static_assert(!(10_meter <= 9_meter), "Comparable is not constexpr");
  static_assert((11_meter > 10_meter), "Comparable is not constexpr");
  static_assert(!(10_meter > 11_meter), "Comparable is not constexpr");
  static_assert((11_meter >= 10_meter), "Comparable is not constexpr");
  static_assert((10_meter >= 10_meter), "Comparable is not constexpr");
  static_assert(!(9_meter >= 10_meter), "Comparable is not constexpr");
}

//! Implicit conversion via user-defined conversion operator.
NOLINT_TEST_F(NamedTypeBasicTest, ConvertibleWithOperator)
{
  struct B {
    B(int x_)
      : x(x_)
    {
    }
    int x;
  };

  struct A {
    A(int x_)
      : x(x_)
    {
    }
    operator B() const { return B(x); }
    int x;
  };

  using StrongA = nova::NamedType<A, struct StrongATag,
    nova::ImplicitlyConvertibleTo<B>::templ>;
  StrongA strongA(A(42));
  B b = strongA;
  EXPECT_EQ(42, b.x);
}

//! Implicit conversion via conversion operator is constexpr-enabled.
NOLINT_TEST_F(NamedTypeBasicTest, ConvertibleWithOperatorConstexpr)
{
  struct B {
    constexpr B(int x_)
      : x(x_)
    {
    }
    int x;
  };

  struct A {
    constexpr A(int x_)
      : x(x_)
    {
    }
    constexpr operator B() const { return B(x); }
    int x;
  };

  using StrongA = nova::NamedType<A, struct StrongATag,
    nova::ImplicitlyConvertibleTo<B>::templ>;
  constexpr StrongA strongA(A(42));
  constexpr B b = strongA;
  static_assert(b.x == 42, "ImplicitlyConvertibleTo is not constexpr");
}

//! Implicit conversion via converting constructor on target type.
NOLINT_TEST_F(NamedTypeBasicTest, ConvertibleWithConstructor)
{
  struct A {
    A(int x_)
      : x(x_)
    {
    }
    int x;
  };

  struct B {
    B(A a)
      : x(a.x)
    {
    }
    int x;
  };

  using StrongA = nova::NamedType<A, struct StrongATag,
    nova::ImplicitlyConvertibleTo<B>::templ>;
  StrongA strongA(A(42));
  B b = strongA;
  EXPECT_EQ(42, b.x);
}

//! Converting constructor path is constexpr-enabled.
NOLINT_TEST_F(NamedTypeBasicTest, ConvertibleWithConstructorConstexpr)
{
  struct A {
    constexpr A(int x_)
      : x(x_)
    {
    }
    int x;
  };

  struct B {
    constexpr B(A a)
      : x(a.x)
    {
    }
    int x;
  };

  using StrongA = nova::NamedType<A, struct StrongATag,
    nova::ImplicitlyConvertibleTo<B>::templ>;
  constexpr StrongA strongA(A(42));
  constexpr B b = strongA;
  static_assert(b.x == 42, "ImplicitlyConvertibleTo is not constexpr");
}

//! Implicit conversion to same underlying type.
NOLINT_TEST_F(NamedTypeBasicTest, ConvertibleToItself)
{
  using MyInt = nova::NamedType<int, struct MyIntTag,
    nova::ImplicitlyConvertibleTo<int>::templ>;
  MyInt myInt(42);
  int i = myInt;
  EXPECT_EQ(42, i);
}

//! Implicit conversion to same underlying type is constexpr-enabled.
NOLINT_TEST_F(NamedTypeBasicTest, ConvertibleToItselfConstexpr)
{
  using MyInt = nova::NamedType<int, struct MyIntTag,
    nova::ImplicitlyConvertibleTo<int>::templ>;
  constexpr MyInt myInt(42);
  constexpr int i = myInt;
  static_assert(i == 42, "ImplicitlyConvertibleTo is not constexpr");
}

//! Hashable skill enables use in unordered_map.
NOLINT_TEST_F(NamedTypeHashTest, Hash)
{
  using SerialNumber = nova::NamedType<std::string, struct SerialNumberTag,
    nova::Comparable, nova::Hashable>;

  std::unordered_map<SerialNumber, int> hashMap
    = { { SerialNumber { "AA11" }, 10 }, { SerialNumber { "BB22" }, 20 } };
  SerialNumber cc33 { "CC33" };
  hashMap[cc33] = 30;
  EXPECT_EQ(10, hashMap[SerialNumber { "AA11" }]);
  EXPECT_EQ(20, hashMap[SerialNumber { "BB22" }]);
  EXPECT_EQ(30, hashMap[cc33]);
}

struct testFunctionCallable_A {
  testFunctionCallable_A(int x_)
    : x(x_)
  {
  }
  // ensures that passing the argument to a function doesn't make a copy
  testFunctionCallable_A(testFunctionCallable_A const&) = delete;
  testFunctionCallable_A(testFunctionCallable_A&&) = default;
  auto operator+=(testFunctionCallable_A const& other)
    -> testFunctionCallable_A&
  {
    x += other.x;
    return *this;
  }
  int x;
};

auto operator+(testFunctionCallable_A const& a1,
  testFunctionCallable_A const& a2) -> testFunctionCallable_A
{
  return testFunctionCallable_A(a1.x + a2.x);
}

auto operator==(
  testFunctionCallable_A const& a1, testFunctionCallable_A const& a2) -> bool
{
  return a1.x == a2.x;
}

//! FunctionCallable enables passing to free functions and arithmetic.
NOLINT_TEST_F(NamedTypeSkillsTest, FunctionCallable)
{
  using A = testFunctionCallable_A;
  auto functionTakingA = [](A const& a) -> int { return a.x; };

  using StrongA = nova::NamedType<A, struct StrongATag, nova::FunctionCallable>;
  StrongA strongA(A(42));
  const StrongA constStrongA(A(42));
  EXPECT_EQ(42, functionTakingA(strongA));
  EXPECT_EQ(42, functionTakingA(constStrongA));
  EXPECT_TRUE(strongA + strongA == 84);
}

struct testFunctionCallable_B {
  constexpr testFunctionCallable_B(int x_)
    : x(x_)
  {
  }
  // ensures that passing the argument to a function doesn't make a copy
  testFunctionCallable_B(testFunctionCallable_B const&) = delete;
  testFunctionCallable_B(testFunctionCallable_B&&) = default;
  constexpr auto operator+=(testFunctionCallable_B const& other)
    -> testFunctionCallable_B&
  {
    x += other.x;
    return *this;
  }
  int x;
};

constexpr auto operator+(testFunctionCallable_B const& a1,
  testFunctionCallable_B const& a2) -> testFunctionCallable_B
{
  return testFunctionCallable_B(a1.x + a2.x);
}

constexpr auto operator==(
  testFunctionCallable_B const& a1, testFunctionCallable_B const& a2) -> bool
{
  return a1.x == a2.x;
}

constexpr auto functionTakingB(testFunctionCallable_B const& b) -> int
{
  return b.x;
}

//! FunctionCallable is constexpr-enabled.
NOLINT_TEST_F(NamedTypeSkillsTest, FunctionCallableConstexpr)
{
  using B = testFunctionCallable_B;

  using StrongB = nova::NamedType<B, struct StrongATag, nova::FunctionCallable>;
  constexpr StrongB constStrongB(B(42));
  static_assert(
    functionTakingB(StrongB(B(42))) == 42, "FunctionCallable is not constexpr");
  static_assert(
    functionTakingB(constStrongB) == 42, "FunctionCallable is not constexpr");
  static_assert(
    StrongB(B(42)) + StrongB(B(42)) == 84, "FunctionCallable is not constexpr");
  static_assert(
    constStrongB + constStrongB == 84, "FunctionCallable is not constexpr");
}

//! MethodCallable enables operator-> to underlying type.
NOLINT_TEST_F(NamedTypeSkillsTest, MethodCallable)
{
  class A {
  public:
    A(int x_)
      : x(x_)
    {
    }
    A(A const&) = delete; // ensures that invoking a method doesn't make a copy
    A(A&&) = default;

    auto method() -> int { return x; }
    auto constMethod() const -> int { return x; }

  private:
    int x;
  };

  using StrongA = nova::NamedType<A, struct StrongATag, nova::MethodCallable>;
  StrongA strongA(A(42));
  const StrongA constStrongA(A((42)));
  EXPECT_EQ(42, strongA->method());
  EXPECT_EQ(42, constStrongA->constMethod());
}

//! MethodCallable is constexpr-enabled.
NOLINT_TEST_F(NamedTypeSkillsTest, MethodCallableConstexpr)
{
  class A {
  public:
    constexpr A(int x_)
      : x(x_)
    {
    }
    A(A const&) = delete; // ensures that invoking a method doesn't make a copy
    A(A&&) = default;

    constexpr auto method() -> int { return x; }
    constexpr auto constMethod() const -> int { return x; }

  private:
    int x;
  };

  using StrongA = nova::NamedType<A, struct StrongATag, nova::MethodCallable>;
  constexpr const StrongA constStrongA(A((42)));
  static_assert(
    StrongA(A(42))->method() == 42, "MethodCallable is not constexpr");
  static_assert(
    constStrongA->constMethod() == 42, "MethodCallable is not constexpr");
}

//! Callable enables both function and method invocation on underlying type.
NOLINT_TEST_F(NamedTypeSkillsTest, Callable)
{
  class A {
  public:
    A(int x_)
      : x(x_)
    {
    }
    A(A const&) = delete; // ensures that invoking a method or function doesn't
                          // make a copy
    A(A&&) = default;

    auto method() -> int { return x; }
    auto constMethod() const -> int { return x; }

  private:
    int x;
  };

  auto functionTakingA = [](A const& a) -> int { return a.constMethod(); };

  using StrongA = nova::NamedType<A, struct StrongATag, nova::Callable>;
  StrongA strongA(A(42));
  const StrongA constStrongA(A(42));
  EXPECT_EQ(42, functionTakingA(strongA));
  EXPECT_EQ(42, strongA->method());
  EXPECT_EQ(42, constStrongA->constMethod());
}

//! Named arguments emulate keyword-style parameters.
NOLINT_TEST_F(NamedTypeNamedArgsTest, NamedArguments)
{
  using FirstName = nova::NamedType<std::string, struct FirstNameTag>;
  using LastName = nova::NamedType<std::string, struct LastNameTag>;
  static const FirstName::argument firstName;
  static const LastName::argument lastName;
  auto getFullName
    = [](FirstName const& firstName_, LastName const& lastName_) //
    -> std::string {
    return firstName_.get() + lastName_.get(); //
  };

  auto fullName = getFullName(firstName = "James", lastName = "Bond");
  EXPECT_EQ(std::string("JamesBond"), fullName);
}

//! Named arguments can be passed in any order via helper.
NOLINT_TEST_F(NamedTypeNamedArgsTest, NamedArgumentsAnyOrder)
{
  using FirstName = nova::NamedType<std::string, struct FirstNameTag>;
  using LastName = nova::NamedType<std::string, struct LastNameTag>;
  static const FirstName::argument firstName;
  static const LastName::argument lastName;

  auto getFullName = nova::MakeNamedArgFunction<FirstName, LastName>(
    [](FirstName const& firstName_, LastName const& lastName_) -> std::string {
      return firstName_.get() + lastName_.get();
    });

  auto fullName = getFullName(lastName = "Bond", firstName = "James");
  EXPECT_EQ(std::string("JamesBond"), fullName);

  auto otherFullName = getFullName(firstName = "James", lastName = "Bond");
  EXPECT_EQ(std::string("JamesBond"), otherFullName);
}

//! Named arguments support braced-init construction.
NOLINT_TEST_F(NamedTypeNamedArgsTest, NamedArgumentsWithBracketConstructor)
{
  using Numbers = nova::NamedType<std::vector<int>, struct NumbersTag>;
  static const Numbers::argument numbers;
  auto getNumbers = [](Numbers const& numbers_)
    -> std::remove_reference_t<std::vector<int, std::allocator<int>>> {
    return numbers_.get();
  };

  auto vec = getNumbers(numbers = { 1, 2, 3 });
  EXPECT_EQ(std::vector<int>({ 1, 2, 3 }), vec);
}

//! Empty base class optimization keeps wrapper size minimal.
NOLINT_TEST_F(NamedTypeBasicTest, EmptyBaseClassOptimization)
{
  EXPECT_EQ(sizeof(double), sizeof(Meter));
}

using strong_int = nova::NamedType<int, struct IntTag>;

//! NamedType supports constexpr get().
NOLINT_TEST_F(NamedTypeBasicTest, Constexpr)
{
  using strong_bool = nova::NamedType<bool, struct BoolTag>;

  static_assert(strong_bool { true }.get(), "NamedType is not constexpr");
}

struct throw_on_construction {
  throw_on_construction() { throw 42; }

  throw_on_construction(int) { throw "exception"; }
};

using C = nova::NamedType<throw_on_construction, struct throwTag>;

//! Construction forwards noexcept from underlying type.
NOLINT_TEST_F(NamedTypeBasicTest, Noexcept)
{
  EXPECT_TRUE(noexcept(strong_int {}));
  EXPECT_FALSE(noexcept(C {}));

  EXPECT_TRUE(noexcept(strong_int(3)));
  EXPECT_FALSE(noexcept(C { 5 }));
}

//! Arithmetic skill composes all arithmetic operators.
NOLINT_TEST_F(NamedTypeSkillsTest, Arithmetic)
{
  using strong_arithmetic
    = nova::NamedType<int, struct ArithmeticTag, nova::Arithmetic>;
  strong_arithmetic a { 1 };
  strong_arithmetic b { 2 };

  EXPECT_EQ(3, (a + b).get());

  a += b;
  EXPECT_EQ(3, a.get());

  EXPECT_EQ(1, (a - b).get());

  a -= b;
  EXPECT_EQ(1, a.get());

  a.get() = 5;
  EXPECT_EQ(10, (a * b).get());

  a *= b;
  EXPECT_EQ(10, a.get());

  EXPECT_EQ(5, (a / b).get());

  a /= b;
  EXPECT_EQ(5, a.get());

  b = ++a;
  EXPECT_EQ(6, a.get());
  EXPECT_EQ(6, b.get());

  b = a++;
  EXPECT_EQ(7, a.get());
  EXPECT_EQ(6, b.get());
}

//! Printable skill streams the underlying value.
NOLINT_TEST_F(NamedTypeSkillsTest, Printable)
{
  using StrongInt = nova::NamedType<int, struct StrongIntTag, nova::Printable>;

  std::ostringstream oss;
  oss << StrongInt(42);
  EXPECT_EQ(std::string("42"), oss.str());
}

//! Dereferencable skill exposes reference to underlying value.
NOLINT_TEST_F(NamedTypeSkillsTest, Dereferencable)
{
  using StrongInt
    = nova::NamedType<int, struct StrongIntTag, nova::Dereferencable>;

  {
    StrongInt a { 1 };
    int& value = *a;
    EXPECT_EQ(1, value);
  }

  {
    const StrongInt a { 1 };
    const int& value = *a;
    EXPECT_EQ(1, value);
  }

  {
    StrongInt a { 1 };
    int& value = *a;
    value = 2;
    EXPECT_EQ(2, a.get());
  }

  {
    auto functionReturningStrongInt
      = []() -> StrongInt { return StrongInt { 28 }; };
    auto functionTakingInt = [](int value) -> int { return value; };

    int value = functionTakingInt(*functionReturningStrongInt());
    EXPECT_EQ(28, value);
  }
}

//! Dereferencable is constexpr-enabled.
NOLINT_TEST_F(NamedTypeSkillsTest, DereferencableConstexpr)
{
  using StrongInt
    = nova::NamedType<int, struct StrongIntTag, nova::Dereferencable>;

  constexpr StrongInt a { 28 };
  static_assert(*a == 28, "Dereferencable is not constexpr");
  static_assert(*StrongInt { 28 } == 28, "Dereferencable is not constexpr");
}

//! PreIncrementable supports ++prefix.
NOLINT_TEST_F(NamedTypeSkillsTest, PreIncrementable)
{
  using StrongInt
    = nova::NamedType<int, struct StrongIntTag, nova::PreIncrementable>;
  StrongInt a { 1 };
  StrongInt b = ++a;
  EXPECT_EQ(2, a.get());
  EXPECT_EQ(2, b.get());
}

//! PreIncrementable is constexpr-enabled and noexcept forwards.
NOLINT_TEST_F(NamedTypeSkillsTest, PreIncrementableConstexpr)
{
  using StrongInt
    = nova::NamedType<int, struct StrongIntTag, nova::PreIncrementable>;
  static_assert(
    (++StrongInt { 1 }).get() == 2, "PreIncrementable is not constexpr");
  StrongInt a { 1 };
  EXPECT_TRUE(noexcept(++a));
}

//! PostIncrementable supports postfix++.
NOLINT_TEST_F(NamedTypeSkillsTest, PostIncrementable)
{
  using StrongInt
    = nova::NamedType<int, struct StrongIntTag, nova::PostIncrementable>;
  StrongInt a { 1 };
  StrongInt b = a++;
  EXPECT_EQ(2, a.get());
  EXPECT_EQ(1, b.get());
}

//! PostIncrementable is constexpr-enabled.
NOLINT_TEST_F(NamedTypeSkillsTest, PostIncrementableConstexpr)
{
  using StrongInt
    = nova::NamedType<int, struct StrongIntTag, nova::PostIncrementable>;
  static_assert(
    (StrongInt { 1 } ++).get() == 1, "PostIncrementable is not constexpr");
}

//! PreDecrementable supports --prefix.
NOLINT_TEST_F(NamedTypeSkillsTest, PreDecrementable)
{
  using StrongInt
    = nova::NamedType<int, struct StrongIntTag, nova::PreDecrementable>;
  StrongInt a { 1 };
  StrongInt b = --a;
  EXPECT_EQ(0, a.get());
  EXPECT_EQ(0, b.get());
}

//! PreDecrementable is constexpr-enabled.
NOLINT_TEST_F(NamedTypeSkillsTest, PreDecrementableConstexpr)
{
  using StrongInt
    = nova::NamedType<int, struct StrongIntTag, nova::PreDecrementable>;
  static_assert(
    (--StrongInt { 1 }).get() == 0, "PreDecrementable is not constexpr");
}

//! PostDecrementable supports postfix--.
NOLINT_TEST_F(NamedTypeSkillsTest, PostDecrementable)
{
  using StrongInt
    = nova::NamedType<int, struct StrongIntTag, nova::PostDecrementable>;
  StrongInt a { 1 };
  StrongInt b = a--;
  EXPECT_EQ(0, a.get());
  EXPECT_EQ(1, b.get());
}

//! PostDecrementable is constexpr-enabled.
NOLINT_TEST_F(NamedTypeSkillsTest, PostDecrementableConstexpr)
{
  using StrongInt
    = nova::NamedType<int, struct StrongIntTag, nova::PostDecrementable>;
  static_assert(
    (StrongInt { 1 } --).get() == 1, "PostDecrementable is not constexpr");
}

//! Incrementable aggregates both pre and post increment.
NOLINT_TEST_F(NamedTypeSkillsTest, Incrementable)
{
  using StrongInt
    = nova::NamedType<int, struct StrongIntTag, nova::Incrementable>;
  {
    StrongInt a { 1 };
    StrongInt b = ++a;
    EXPECT_EQ(2, a.get());
    EXPECT_EQ(2, b.get());
  }
  {
    StrongInt a { 1 };
    StrongInt b = a++;
    EXPECT_EQ(2, a.get());
    EXPECT_EQ(1, b.get());
  }
}

//! Decrementable aggregates both pre and post decrement.
NOLINT_TEST_F(NamedTypeSkillsTest, Decrementable)
{
  using StrongInt
    = nova::NamedType<int, struct StrongIntTag, nova::Decrementable>;
  {
    StrongInt a { 1 };
    StrongInt b = --a;
    EXPECT_EQ(0, a.get());
    EXPECT_EQ(0, b.get());
  }
  {
    StrongInt a { 1 };
    StrongInt b = a--;
    EXPECT_EQ(0, a.get());
    EXPECT_EQ(1, b.get());
  }
}

template <template <typename> class... Skills>
using SkilledType = nova::NamedType<int, struct SkilledTypeTag, Skills...>;

//! Skills preserve empty base optimization; wrapper stays same size as int.
NOLINT_TEST_F(NamedTypeSkillsTest, EBCOForSkills)
{
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::PreIncrementable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::PostIncrementable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::PreDecrementable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::PostDecrementable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::BinaryAddable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::UnaryAddable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::Addable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::BinarySubtractable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::UnarySubtractable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::Subtractable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::Multiplicable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::Divisible>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::Modulable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::BitWiseInvertable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::BitWiseAndable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::BitWiseOrable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::BitWiseXorable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::BitWiseLeftShiftable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::BitWiseRightShiftable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::Comparable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::Dereferencable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::ImplicitlyConvertibleTo>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::Printable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::Hashable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::FunctionCallable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::MethodCallable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::Callable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::Incrementable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::Decrementable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::Arithmetic>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::PreIncrementable>));
  EXPECT_EQ(sizeof(int), sizeof(SkilledType<nova::PreIncrementable>));
}

} // namespace
