//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Testing/GTest.h>

#include <Nova/Base/Resource.h>

using nova::Resource;
using nova::ResourceHandle;
using nova::TypeList;

namespace base::resources {

//=== Test Resource Types ===------------------------------------------------//

// Forward declare test resource types
class TestResource;
class AnotherTestResource;

// Define the centralized type list for test resources
using TestResourceTypeList = TypeList<TestResource, AnotherTestResource>;

// Test resource implementations
class TestResource final : public Resource<TestResource, TestResourceTypeList> {
public:
  using Resource::Resource;

  auto InvalidateResource() -> void { Invalidate(); }
};

class AnotherTestResource final
  : public Resource<AnotherTestResource, TestResourceTypeList> {
public:
  using Resource::Resource;

  auto InvalidateResource() -> void { Invalidate(); }
};

//=== Basic Resource Construction Tests ===-----------------------------------//

//! Test default constructor creates invalid resource
/*!
 Verify that default-constructed resources are properly initialized
 as invalid with the correct compile-time resource type.
*/
NOLINT_TEST(ResourceTest, DefaultConstructor_CreatesInvalidResource)
{
  // Arrange & Act
  const TestResource resource;

  // Assert
  EXPECT_FALSE(resource.IsValid());
  EXPECT_EQ(resource.GetResourceType(), TestResource::GetResourceType());
}

//! Test parameterized constructor with valid handle
/*!
 Verify that resources constructed with valid handles maintain
 the handle and report as valid.
*/
NOLINT_TEST(ResourceTest, ParameterizedConstructor_WithValidHandle)
{
  // Arrange
  const ResourceHandle handle(1U, TestResource::GetResourceType());

  // Act
  const TestResource resource(handle);

  // Assert
  EXPECT_TRUE(resource.IsValid());
  EXPECT_EQ(resource.GetHandle(), handle);
  EXPECT_EQ(resource.GetResourceType(), TestResource::GetResourceType());
}

//=== Copy Semantics Tests ===------------------------------------------------//

//! Test copy constructor preserves resource state
/*!
 Verify that copy construction creates an independent resource
 with the same handle and validity state.
*/
NOLINT_TEST(ResourceTest, CopyConstructor_PreservesState)
{
  // Arrange
  const ResourceHandle handle(1U, TestResource::GetResourceType());
  const TestResource resource1(handle);

  // Act
  const auto& resource2(resource1);

  // Assert
  EXPECT_EQ(resource1.GetHandle(), resource2.GetHandle());
  EXPECT_EQ(resource1.GetResourceType(), resource2.GetResourceType());
  EXPECT_TRUE(resource2.IsValid());
}

//! Test copy assignment preserves resource state
/*!
 Verify that copy assignment creates an independent resource
 with the same handle and validity state.
*/
NOLINT_TEST(ResourceTest, CopyAssignment_PreservesState)
{
  // Arrange
  const ResourceHandle handle(1U, TestResource::GetResourceType());
  const TestResource resource1(handle);

  // Act
  const auto& resource2 = resource1;

  // Assert
  EXPECT_EQ(resource1.GetHandle(), resource2.GetHandle());
  EXPECT_EQ(resource1.GetResourceType(), resource2.GetResourceType());
  EXPECT_TRUE(resource2.IsValid());
}

//=== Move Semantics Tests ===------------------------------------------------//

//! Test move constructor transfers resource ownership
/*!
 Verify that move construction properly transfers the handle
 and invalidates the source resource.
*/
NOLINT_TEST(ResourceTest, MoveConstructor_TransfersOwnership)
{
  // Arrange
  const ResourceHandle handle(1U, TestResource::GetResourceType());
  TestResource resource1(handle);

  // Act
  const auto resource2(std::move(resource1));

  // Assert
  EXPECT_EQ(resource2.GetHandle(), handle);
  EXPECT_TRUE(resource2.IsValid());
  // NOLINTNEXTLINE(bugprone-use-after-move) for testing purposes
  EXPECT_FALSE(resource1.IsValid());
}

//! Test move assignment transfers resource ownership
/*!
 Verify that move assignment properly transfers the handle
 and invalidates the source resource.
*/
NOLINT_TEST(ResourceTest, MoveAssignment_TransfersOwnership)
{
  // Arrange
  const ResourceHandle handle(1U, TestResource::GetResourceType());
  TestResource resource1(handle);

  // Act
  const auto resource2 = std::move(resource1);

  // Assert
  EXPECT_EQ(resource2.GetHandle(), handle);
  EXPECT_TRUE(resource2.IsValid());
  // NOLINTNEXTLINE(bugprone-use-after-move) for testing purposes
  EXPECT_FALSE(resource1.IsValid());
}

//=== Resource State Management Tests ===-------------------------------------//

//! Test resource invalidation changes validity state
/*!
 Verify that calling InvalidateResource() properly invalidates
 the resource while preserving the resource type.
*/
NOLINT_TEST(ResourceTest, Invalidate_ChangesValidityState)
{
  // Arrange
  const ResourceHandle handle(1U, TestResource::GetResourceType());
  TestResource resource(handle);
  EXPECT_TRUE(resource.IsValid());

  // Act
  resource.InvalidateResource();

  // Assert
  EXPECT_FALSE(resource.IsValid());
  EXPECT_EQ(resource.GetResourceType(), TestResource::GetResourceType());
}

//=== Compile-Time Resource Type Tests ===------------------------------------//

//! Test different resource types get unique compile-time IDs
/*!
 Verify that the compile-time resource type system assigns
 unique IDs to different resource types.
*/
NOLINT_TEST(ResourceTest, CompileTimeResourceTypes_AreUnique)
{
  using testing::Ne;

  // Arrange & Act
  constexpr auto test_resource_type = TestResource::GetResourceType();
  constexpr auto another_resource_type = AnotherTestResource::GetResourceType();

  // Assert
  EXPECT_THAT(test_resource_type, Ne(another_resource_type));
}

} // namespace base::resources
