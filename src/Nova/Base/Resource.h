//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Nova/Base/Macros.h>
#include <Nova/Base/ResourceHandle.h>
#include <Nova/Base/TypeList.h>

namespace nova {

/*!
 Base class for objects that require handle-based access into a ResourceTable
 where the objects are stored.

 Resources use high-performance, cache-friendly storage for frequently accessed
 objects using compile-time type identification and contiguous memory layout.
 They are designed for scenarios where O(1) access, automatic handle validation,
 and memory defragmentation are critical for performance.

 ### Global Resource Type List
 - All resource types (any type derived from Resource) and pooled object types
   (any type that uses ResourceTable for storage) must be listed in a single
   `TypeList` (e.g., `ResourceTypeList`).
 - The order of types in the list determines their type ID. **Never reorder
   existing types**; only append new types to the end to maintain binary
   compatibility across builds and modules.
 - Forward declare all resource/component types before defining the type list to
   avoid circular dependencies and enable use in headers.
 - The type list must be visible to all code that needs to resolve resource type
   IDs at compile time (e.g., pools, handles, registries).

 ### When to Use Resource

 Inherit from Resource when your object needs:
 - **High-frequency access** (transforms, scene nodes, pooled components)
 - **Cache-friendly storage** with contiguous memory layout
 - **Handle-based indirection** with automatic validation and invalidation
 - **Built-in defragmentation** to maintain cache locality over time
 - **Cross-module consistency** with compile-time type safety

 ### When NOT to Use Resource

 Do NOT inherit from Resource for:
 - **Low-frequency objects** (settings, managers, one-off instances)
 - **RAII wrappers** around external APIs
 - **Simple data containers** without complex lifecycle needs

 ### Usage Example

 ```cpp
 // In ResourceTypes.h - centralized type registry
 using ResourceTypeList = TypeList<SceneNode, TransformComponent, Texture>;

 // Scene node as a Resource
 class SceneNode : public Resource<SceneNode, ResourceTypeList> {
 public:
     explicit SceneNode(const std::string& name);
     // ... scene node functionality
 };

 // Pooled component as both Component and Resource
 class TransformComponent : public Component,
                           public Resource<TransformComponent, ResourceTypeList>
 { NOVA_COMPONENT(TransformComponent) public: void SetPosition(const Vec3&
 pos);
     // ... transform functionality
 };
 ```

 ### Performance Characteristics

 The use of ResourceTable for storage and ResourceHandle for indirection
 provides the following performance characteristics:
 - **Access Time**: O(1) with generation counter validation
 - **Memory Layout**: Contiguous storage, Pooled allocation

 The use of compile-time resource type IDs provides the following benefits:
 - **Resource Type Resolution**: Zero runtime overhead

 @tparam ResourceT The derived resource type (CRTP pattern - must be the
 inheriting class)
 @tparam ResourceTypeListT Centralized TypeList containing ALL resource types
 (order matters!)
 @tparam HandleT Handle type for indirection (defaults to ResourceHandle)

 @warning ResourceTypeList order determines compile-time type IDs. Never reorder
 existing types - only append new types to maintain binary compatibility!
 @warning Maximum 256 resource types supported due to
 ResourceHandle::ResourceTypeT being uint8_t

 @see ResourceTable, ResourceHandle, GetResourceTypeId
*/
template <typename ResourceT, typename ResourceTypeListT,
  typename HandleT = ResourceHandle>
  requires(std::is_base_of_v<ResourceHandle, HandleT>)
class Resource {
public:
  //! The centralized TypeList containing all valid resource types.
  using ResourceTypeList = ResourceTypeListT;

  //! Get the compile-time unique resource type ID for the `ResourceT` type,
  //! provided it has been registered in the global `ResourceTypeList`.
  /*!
   Returns the zero-based index of Resource concrete type within
   ResourceTypeList, providing zero runtime overhead type resolution through
   template metaprogramming.

   @return Compile-time constant resource type ID (0-255)

   @see @ref ResourceTypeList.h "Compile Time Resource Type System for usage and
   requirements"
  */
  static constexpr auto GetResourceType() noexcept
    -> ResourceHandle::ResourceTypeT
  {
    static_assert(IndexOf<ResourceT, ResourceTypeList>::value
        <= ResourceHandle::kResourceTypeMax,
      "Too many resource types for ResourceHandle::ResourceTypeT!");
    return static_cast<ResourceHandle::ResourceTypeT>(
      IndexOf<ResourceT, ResourceTypeList>::value);
  }

  constexpr explicit Resource(HandleT handle)
    : handle_(std::move(handle))
  {
    assert(handle_.ResourceType() == GetResourceType());
  }

  virtual ~Resource() = default;

  NOVA_DEFAULT_COPYABLE(Resource)
  NOVA_DEFAULT_MOVABLE(Resource)

  // ReSharper disable once CppHiddenFunction
  [[nodiscard]] constexpr auto GetHandle() const noexcept -> const HandleT&
  {
    return handle_;
  }

  [[nodiscard]] virtual auto IsValid() const noexcept -> bool
  {
    return handle_.IsValid();
  }

protected:
  constexpr Resource()
    : handle_(HandleT::kInvalidIndex, GetResourceType())
  {
  }

  constexpr auto Invalidate() -> void { handle_.Invalidate(); }

private:
  HandleT handle_;
};

} // namespace nova
