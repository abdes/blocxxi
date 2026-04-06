//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Testing/GTest.h>

#include <Nova/Base/ResourceHandle.h>
#include <Nova/Base/TypeList.h>

using nova::IndexOf;
using nova::TypeList;

namespace {

//! Helper to get the index of a type in a TypeList as std::size_t.
template <typename T, typename TypeListT>
constexpr auto GetTypeIndex() noexcept -> std::size_t
{
  return IndexOf<T, TypeListT>::value;
}

//! Test correct index assignment for types in the list.
NOLINT_TEST(TypeListTest, CorrectIndexAssignment)
{
  // Arrange
  class A { };
  class B { };
  class C { };
  using MyTypeList = TypeList<A, B, C>;

  // Act & Assert
  EXPECT_EQ((GetTypeIndex<A, MyTypeList>()), 0);
  EXPECT_EQ((GetTypeIndex<B, MyTypeList>()), 1);
  EXPECT_EQ((GetTypeIndex<C, MyTypeList>()), 2);
}

//! Test index stability when appending new types.
NOLINT_TEST(TypeListTest, IndexStabilityOnAppend)
{
  // Arrange
  class A { };
  class B { };
  class C { };
  class D { };
  using MyTypeList = TypeList<A, B, C>;
  using ExtendedList = TypeList<A, B, C, D>;

  // Act & Assert
  EXPECT_EQ((GetTypeIndex<A, MyTypeList>()), (GetTypeIndex<A, ExtendedList>()));
  EXPECT_EQ((GetTypeIndex<B, MyTypeList>()), (GetTypeIndex<B, ExtendedList>()));
  EXPECT_EQ((GetTypeIndex<C, MyTypeList>()), (GetTypeIndex<C, ExtendedList>()));
  EXPECT_EQ((GetTypeIndex<D, ExtendedList>()), 3);
}

//! Test constexpr usability of GetTypeIndex.
NOLINT_TEST(TypeListTest, ConstexprUsability)
{
  // Arrange
  class A { };
  class B { };
  using MyTypeList = TypeList<A, B>;

  // Act
  constexpr auto index_b = GetTypeIndex<B, MyTypeList>();

  // Assert
  static_assert(
    index_b == 1, "GetTypeIndex must be usable in constexpr context");
  EXPECT_EQ(index_b, 1);
}

//! Test GetTypeIndex works with forward-declared types.
NOLINT_TEST(TypeListTest, WorksWithForwardDeclarations)
{
  // Arrange
  class Fwd;
  using FwdList = TypeList<Fwd>;

  // Act & Assert
  EXPECT_EQ((GetTypeIndex<Fwd, FwdList>()), 0);
}

//! Test that only exact types in the list are accepted (not derived types).
NOLINT_TEST(TypeListTest, OnlyExactTypeAccepted)
{
  // Arrange
  class Base { };
  class Derived : public Base { };
  using MyTypeList = TypeList<Base>;

  // Act & Assert
  EXPECT_EQ((GetTypeIndex<Base, MyTypeList>()), 0);
  // The following line should fail to compile if uncommented:
  // EXPECT_EQ(GetTypeIndex<Derived, MyTypeList>(), 0);
}

//! Test TypeListSize returns correct number of types.
NOLINT_TEST(TypeListTest, TypeListSize)
{
  // Arrange
  using EmptyList = TypeList<>;
  using OneTypeList = TypeList<int>;
  using ThreeTypeList = TypeList<int, float, double>;

  // Act & Assert
  EXPECT_EQ(nova::TypeListSize<EmptyList>::value, 0);
  EXPECT_EQ(nova::TypeListSize<OneTypeList>::value, 1);
  EXPECT_EQ(nova::TypeListSize<ThreeTypeList>::value, 3);
}

//! Helper template for TypeListTransform tests.
template <typename T> using MakePointer = T*;

//! Test TypeListTransform applies template to all types in the list.
NOLINT_TEST(TypeListTest, TypeListTransform)
{
  // Arrange
  using MyTypeList = TypeList<int, float, double>;
  using Transformed = nova::TypeListTransform<MyTypeList, MakePointer>::Type;

  // Act
  using Expected = std::tuple<int*, float*, double*>;

  // Assert
  static_assert(std::is_same_v<Transformed, Expected>,
    "TypeListTransform should produce tuple of pointer types");
  EXPECT_TRUE((std::is_same_v<Transformed, Expected>));
}

} // namespace
