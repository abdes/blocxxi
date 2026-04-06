//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Testing/GTest.h>

#include <Nova/Base/ObserverPtr.h>

#include <compare>
#include <unordered_map>
#include <unordered_set>

using nova::make_observer;
using nova::observer_ptr;

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define NOVA_OBSERVER_PTR_TEST_HAS_ASAN 1
#  endif
#endif

#if !defined(NOVA_OBSERVER_PTR_TEST_HAS_ASAN) && defined(__SANITIZE_ADDRESS__)
#  define NOVA_OBSERVER_PTR_TEST_HAS_ASAN 1
#endif

#if !defined(NOVA_OBSERVER_PTR_TEST_HAS_ASAN)
#  define NOVA_OBSERVER_PTR_TEST_HAS_ASAN 0
#endif

//-----------------------------------------------------------------------------
// ObserverPtrTest - unit tests for nova::observer_ptr
//-----------------------------------------------------------------------------

namespace {

// Helper function to prevent compiler optimization of death test scenarios
template <typename T> void ForceEvaluation(T&& value)
{
  volatile auto temp = std::forward<T>(value);
  (void)temp;
}

//! Test default and nullptr construction
/*!
 Scenario: create default-constructed and nullptr-constructed observer_ptr and
 verify they are empty and convert to false.
*/
NOLINT_TEST(ObserverPtrTest, DefaultAndNullptrConstruction)
{
  // Arrange
  observer_ptr<int> a;
  observer_ptr<int> b(nullptr);

  // Act / Assert
  EXPECT_EQ(a.get(), nullptr);
  EXPECT_EQ(b.get(), nullptr);
  EXPECT_FALSE(a);
  EXPECT_FALSE(b);
}

//! Test pointer construction and access
/*!
 Scenario: construct from raw pointer and exercise get, operator*, and member
 access. Verify modifications through the observer reflect on the pointee.
*/
NOLINT_TEST(ObserverPtrTest, PointerConstructionAndAccess)
{
  // Arrange
  int x = 42;
  observer_ptr<int> ptr(&x);

  // Act / Assert
  EXPECT_EQ(ptr.get(), &x);
  EXPECT_TRUE(ptr);
  EXPECT_EQ(*ptr, 42);
  *ptr = 99;
  EXPECT_EQ(x, 99);
}

//! Test copy construction and assignment
/*!
 Scenario: copy-construct from another observer_ptr and assign a new one.
*/
NOLINT_TEST(ObserverPtrTest, CopyConstructAndAssign)
{
  // Arrange
  int x = 1, y = 2;
  observer_ptr<int> a(&x);

  // Act
  observer_ptr<int> b(a);
  b = observer_ptr<int>(&y);

  // Assert
  EXPECT_EQ(b.get(), &y);
}

//! Test implicit conversion to compatible observer_ptr<U>
/*!
 Scenario: observer_ptr<Derived> converts to observer_ptr<Base> when
 Derived* is convertible to Base*.
*/
NOLINT_TEST(ObserverPtrTest, ImplicitConversionToCompatibleType)
{
  // Arrange
  struct Base {
    int v = 7;
  };
  struct Derived : Base {
  } d;

  // Act
  observer_ptr<Derived> dptr(&d);
  observer_ptr<Base> bptr = dptr;

  // Assert
  EXPECT_EQ(bptr->v, 7);
}

//! Test explicit conversion to raw pointer
/*!
 Scenario: observer_ptr<T> converts explicitly to T*.
*/
NOLINT_TEST(ObserverPtrTest, ExplicitConversionToPointer)
{
  // Arrange
  int x = 5;
  observer_ptr<int> ptr(&x);

  // Act
  int* raw = static_cast<int*>(ptr);

  // Assert
  EXPECT_EQ(raw, &x);
}

//! Test reset and release
/*!
 Scenario: reset to a different pointer and release the watched pointer.
*/
NOLINT_TEST(ObserverPtrTest, ResetAndRelease)
{
  // Arrange
  int x = 9, y = 10;
  observer_ptr<int> ptr(&x);

  // Act
  ptr.reset(&y);
  int* released = ptr.release();

  // Assert
  EXPECT_EQ(ptr.get(), nullptr);
  EXPECT_EQ(released, &y);
}

//! Test swap member and non-member
/*!
 Scenario: swap two observer_ptrs using member swap and ADL-enabled swap.
*/
NOLINT_TEST(ObserverPtrTest, Swap)
{
  // Arrange
  int x = 1, y = 2;
  observer_ptr<int> a(&x), b(&y);

  // Act
  a.swap(b);

  // Assert
  EXPECT_EQ(a.get(), &y);
  EXPECT_EQ(b.get(), &x);

  // Act
  swap(a, b);

  // Assert
  EXPECT_EQ(a.get(), &x);
  EXPECT_EQ(b.get(), &y);
}

//! Test comparisons
/*!
 Scenario: equality, inequality and ordering comparisons between observer_ptrs
 and with nullptr.
*/
NOLINT_TEST(ObserverPtrTest, Comparisons)
{
  // Arrange
  int x = 1, y = 2;
  observer_ptr<int> a(&x), b(&y), c(&x), n(nullptr);

  // Act / Assert
  EXPECT_TRUE(a == c);
  EXPECT_FALSE(a != c);
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(n == nullptr);
  EXPECT_TRUE(a != nullptr);
  EXPECT_TRUE(a <= c);
  EXPECT_TRUE(a >= c);
  EXPECT_FALSE(a < c);
  EXPECT_FALSE(a > c);
}

//! Test three-way comparison operator
/*!
 Scenario: use the spaceship operator (<=>) for comparisons and verify all
 derived comparison operators work correctly.
*/
NOLINT_TEST(ObserverPtrTest, ThreeWayComparison)
{
  // Arrange
  int x = 1, y = 2;
  observer_ptr<int> a(&x), b(&y), c(&x);

  // Act / Assert - Three-way comparison
  auto ordering_equal = a <=> c;
  auto ordering_less = (&x < &y) ? (a <=> b) : (b <=> a);
  auto ordering_greater = (&x > &y) ? (a <=> b) : (b <=> a);

  EXPECT_EQ(ordering_equal, std::strong_ordering::equal);
  EXPECT_EQ(ordering_less, std::strong_ordering::less);
  EXPECT_EQ(ordering_greater, std::strong_ordering::greater);

  // Verify all comparison operators are synthesized correctly
  EXPECT_TRUE(a == c);
  EXPECT_FALSE(a != c);
  EXPECT_EQ(a < b, &x < &y);
  EXPECT_EQ(a > b, &x > &y);
  EXPECT_EQ(a <= b, &x <= &y);
  EXPECT_EQ(a >= b, &x >= &y);
}

//! Test hash support
/*!
 Scenario: verify that observer_ptr can be used in hash-based containers
 and that hash values are consistent with pointer equality.
*/
NOLINT_TEST(ObserverPtrTest, HashSupport)
{
  // Arrange
  int x = 1, y = 2;
  observer_ptr<int> a(&x), b(&y), c(&x);

  // Act
  std::hash<observer_ptr<int>> hasher;
  auto hash_a = hasher(a);
  auto hash_c = hasher(c);

  // Assert - Equal pointers should have equal hashes
  EXPECT_EQ(hash_a, hash_c);

  // Use in unordered_set
  std::unordered_set<observer_ptr<int>> ptr_set;
  ptr_set.insert(a);
  ptr_set.insert(b);
  ptr_set.insert(c); // Should not increase size since c == a

  EXPECT_EQ(ptr_set.size(), 2u);
  EXPECT_TRUE(ptr_set.contains(a));
  EXPECT_TRUE(ptr_set.contains(b));
  EXPECT_TRUE(ptr_set.contains(c));

  // Use in unordered_map
  std::unordered_map<observer_ptr<int>, std::string> ptr_map;
  ptr_map[a] = "first";
  ptr_map[b] = "second";
  ptr_map[c] = "updated_first"; // Should update a's value

  EXPECT_EQ(ptr_map.size(), 2u);
  EXPECT_EQ(ptr_map[a], "updated_first");
  EXPECT_EQ(ptr_map[c], "updated_first");
  EXPECT_EQ(ptr_map[b], "second");
}

//! Test non-member nullptr comparisons
/*!
 Scenario: verify that nullptr can be compared from both sides with observer_ptr
 using non-member comparison operators.
*/
NOLINT_TEST(ObserverPtrTest, NonMemberNullptrComparisons)
{
  // Arrange
  int x = 42;
  observer_ptr<int> ptr(&x);
  observer_ptr<int> null_ptr(nullptr);

  // Act / Assert - Member operator comparisons
  EXPECT_TRUE(ptr == ptr);
  EXPECT_FALSE(ptr == nullptr);
  EXPECT_TRUE(null_ptr == nullptr);
  EXPECT_FALSE(nullptr == ptr);
  EXPECT_TRUE(nullptr == null_ptr);

  // Verify non-member comparison works (nullptr on left)
  EXPECT_FALSE(nullptr == ptr);
  EXPECT_TRUE(nullptr == null_ptr);
  EXPECT_TRUE(nullptr != ptr);
  EXPECT_FALSE(nullptr != null_ptr);
}

//! Test concept constraints compliance
/*!
 Scenario: verify that observer_ptr works correctly with valid types and
 that the concepts are properly defined.
*/
NOLINT_TEST(ObserverPtrTest, ConceptConstraintsCompliance)
{
  // Arrange & Act & Assert - These should all compile and work correctly

  // Valid types that should work:
  [[maybe_unused]] observer_ptr<int> int_ptr;
  [[maybe_unused]] observer_ptr<void> void_ptr;
  [[maybe_unused]] observer_ptr<const int> const_int_ptr;
  [[maybe_unused]] observer_ptr<volatile int> volatile_int_ptr;
  [[maybe_unused]] observer_ptr<const volatile int> cv_int_ptr;

  // Test incomplete types (forward declaration should work)
  struct IncompleteType;
  [[maybe_unused]] observer_ptr<IncompleteType> incomplete_ptr;

  // Test array types
  int arr[5] = { 1, 2, 3, 4, 5 };
  observer_ptr<int[5]> array_ptr(&arr);

  // Verify concept requirements at runtime
  static_assert(nova::ObservableType<int>);
  static_assert(nova::ObservableType<void>);
  static_assert(nova::ObservableType<const int>);
  static_assert(nova::ObservableType<int[5]>);
  static_assert(!nova::ObservableType<int&>);
  static_assert(!nova::ObservableType<const int&>);

  static_assert(nova::Dereferenceable<int>);
  static_assert(nova::Dereferenceable<const int>);
  static_assert(!nova::Dereferenceable<void>);

  SUCCEED(); // Test passes if compilation succeeds
}

//! Test observer_ptr<void> arrow operator
/*!
 Scenario: verify that observer_ptr<void> supports the arrow operator
 correctly and returns void*.
*/
NOLINT_TEST(ObserverPtrTest, VoidArrowOperator)
{
  // Arrange
  int x = 42;
  observer_ptr<void> void_ptr(&x);
  observer_ptr<void> null_void_ptr(nullptr);

  // Act & Assert
  void* raw_ptr = void_ptr.operator->();
  EXPECT_EQ(raw_ptr, static_cast<void*>(&x));
  EXPECT_EQ(void_ptr.get(), static_cast<void*>(&x));

  // Verify arrow operator works directly
  void* arrow_result = void_ptr.operator->();
  EXPECT_EQ(arrow_result, static_cast<void*>(&x));

  // Test with null pointer
  void* null_raw = null_void_ptr.operator->();
  EXPECT_EQ(null_raw, nullptr);

  // Verify we can cast the result
  int* cast_back = static_cast<int*>(void_ptr.operator->());
  EXPECT_EQ(cast_back, &x);
  EXPECT_EQ(*cast_back, 42);
}

//! Test observer_ptr with array types
/*!
 Scenario: verify that observer_ptr works correctly with array types
 and provides proper access to array elements.
*/
NOLINT_TEST(ObserverPtrTest, ArrayTypes)
{
  // Arrange
  int arr[5] = { 10, 20, 30, 40, 50 };
  observer_ptr<int[5]> array_ptr(&arr);
  observer_ptr<int> element_ptr(&arr[0]);

  // Act & Assert - Array pointer operations
  EXPECT_EQ(array_ptr.get(), &arr);
  EXPECT_TRUE(array_ptr);

  // Verify we can get pointer to array
  int (*raw_array)[5] = array_ptr.get();
  EXPECT_EQ(raw_array, &arr);
  EXPECT_EQ((*raw_array)[0], 10);
  EXPECT_EQ((*raw_array)[4], 50);

  // Test dereference of array pointer (gives reference to array)
  auto& array_ref = *array_ptr;
  EXPECT_EQ(array_ref[0], 10);
  EXPECT_EQ(array_ref[4], 50);

  // Test arrow operator (returns pointer to array)
  auto* arrow_result = array_ptr.operator->();
  EXPECT_EQ(arrow_result, &arr);
  EXPECT_EQ((*arrow_result)[2], 30);

  // Test conversions and comparisons
  observer_ptr<int[5]> array_ptr2(&arr);
  EXPECT_TRUE(array_ptr == array_ptr2);
  EXPECT_FALSE(array_ptr != array_ptr2);
}

//! Test move semantics and advanced use cases
/*!
 Scenario: verify that observer_ptr supports move operations correctly
 and works with complex scenarios.
*/
NOLINT_TEST(ObserverPtrTest, MoveSemantics)
{
  // Arrange
  int x = 100;
  observer_ptr<int> original(&x);

  // Act & Assert - Move construction
  observer_ptr<int> moved(std::move(original));
  EXPECT_EQ(moved.get(), &x);
  EXPECT_EQ(*moved, 100);
  // Note: original is not required to be null after move for observer_ptr

  // Move assignment
  observer_ptr<int> target;
  target = std::move(moved);
  EXPECT_EQ(target.get(), &x);
  EXPECT_EQ(*target, 100);

  // Test with const objects
  const int const_val = 200;
  observer_ptr<const int> const_ptr(&const_val);
  EXPECT_EQ(*const_ptr, 200);

  // Test polymorphic usage
  struct Base {
    virtual ~Base() = default;
    int base_val = 1;
  };
  struct Derived : Base {
    int derived_val = 2;
  };

  Derived d;
  observer_ptr<Derived> derived_ptr(&d);
  observer_ptr<Base> base_ptr = derived_ptr; // Implicit conversion

  EXPECT_EQ(base_ptr->base_val, 1);
  EXPECT_EQ(derived_ptr->derived_val, 2);
}

//! Test make_observer helper
/*!
 Scenario: create an observer_ptr using make_observer.
*/
NOLINT_TEST(ObserverPtrTest, MakeObserver)
{
  // Arrange
  int x = 123;

  // Act
  auto ptr = make_observer(&x);

  // Assert
  EXPECT_EQ(ptr.get(), &x);
}

//! Test const correctness
/*!
 Scenario: const observer_ptr should allow read-only access to pointee.
*/
NOLINT_TEST(ObserverPtrTest, ConstCorrectness)
{
  // Arrange
  int x = 7;
  const observer_ptr<int> ptr(&x);

  // Act / Assert
  EXPECT_EQ(*ptr, 7);
  EXPECT_EQ(ptr.get(), &x);
}

//! Test observer_ptr<void>
/*!
 Scenario: observer_ptr<void> stores and returns void* pointers.
*/
NOLINT_TEST(ObserverPtrTest, ObserverPtrToVoid)
{
  // Arrange
  int x = 42;

  // Act
  observer_ptr<void> vptr(&x);

  // Assert
  EXPECT_EQ(vptr.get(), static_cast<void*>(&x));
}

//! Test dereferencing null causes death
/*!
 Scenario: dereferencing a null observer_ptr should trigger an assertion or
 crash. This test verifies the behavior is properly caught.
*/
NOLINT_TEST(ObserverPtrTest, DereferenceNull_Death)
{
  // Arrange
  observer_ptr<int> ptr;

  // Act & Assert - Dereferencing null should cause death
  // Use volatile to prevent compiler optimization, and also use helper function
#if defined(NDEBUG) && !NOVA_OBSERVER_PTR_TEST_HAS_ASAN
  (void)ptr;
  GTEST_SKIP() << "Optimized non-ASan builds do not guarantee a trap for null "
                  "dereference undefined behavior.";
#else
  EXPECT_DEATH({ ForceEvaluation(*ptr); }, ".*");
#endif
}

//! Test arrow operator on null causes death
/*!
 Scenario: using the arrow operator on a null observer_ptr should trigger
 an assertion or crash for non-void types.
*/
NOLINT_TEST(ObserverPtrTest, ArrowOperatorNull_Death)
{
  // Arrange
  struct TestStruct {
    int value;
  };
  observer_ptr<TestStruct> ptr;

  // Act & Assert - Arrow operator on null should cause death
  // Use volatile to prevent compiler optimization, and also use helper function
#if defined(NDEBUG) && !NOVA_OBSERVER_PTR_TEST_HAS_ASAN
  (void)ptr;
  GTEST_SKIP() << "Optimized non-ASan builds do not guarantee a trap for null "
                  "dereference undefined behavior.";
#else
  EXPECT_DEATH({ ForceEvaluation(ptr->value); }, ".*");
#endif
}

} // namespace
