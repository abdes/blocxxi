//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <cassert>
#include <string>

namespace nova {

/*
A graphics API agnostic POD structure representing different types of resources
that get linked to their counterparts on the core backend.

The handle is used as an alternative to pointers / associative container lookup
to achieve several enhancements:

1. Store data in a contiguous block of memory.
2. Create an associative mapping between the application view of the resource
   and the actual data on the core side, while ensuring O(1) lookups, O(1)
   insertions and O(1) removals for maximum efficiency.

The handle is a 64-bit value, so there is no additional overhead compared to a
pointer on 64-bit platforms.

The 64-bit value is laid out in the following way, with the order of the fields
being important for sorting prioritized by the reserved bits, then free status,
then custom bits, then resource type, then generation, and finally index.

```
    reserved
      free bit
    3 1    8        8         12                       32
    ---X<-cust--><-type--><--- gen ---> <------------- index ------------->
    ........ ........ ........ ........ ........ ........ ........ ........
```

The 4 most significant bits of the handle are reserved (used for implementation
of the handle lookup table). The free bit is used to manage the freelist. When
set, the handle is part of a freelist managed by the lookup table and can be
allocated for a new resource. Otherwise, the handle is active. This gives us an
embedded singly linked list within the lookup table costing just 1 bit in the
handle. As long as we store the front index of the freelist separately, it is an
O(1) operation to find the next available slot and maintain the singly linked
list.

The next most significant bits of the handle hold the resource type. This is
extra information, that can introduce an element of type safety in the
application or be used for special handling of resources by type.

The generation field is used as a safety mechanism to detect when a stale handle
is trying to access data that has since been overwritten in the corresponding
slot. Every time a slot in the lookup table is removed, the generation
increments. Handle lookups assert that the generations match.

The remaining bits are simply an index into an array for that specific resource
type inside the Render Device.
*/
class ResourceHandle {
public:
  using HandleT = uint64_t;

private:
  static constexpr uint8_t kHandleBits = sizeof(HandleT) * 8;
  static constexpr uint8_t kReservedBits { 3 };
  static constexpr uint8_t kFreeBits { 1 };
  static constexpr uint8_t kCustomBits { 8 };
  static constexpr uint8_t kResourceTypeBits { 8 };
  static constexpr uint8_t kGenerationBits { 12 };
  static constexpr uint8_t kIndexBits { 32 };

  static constexpr HandleT kHandleMask = static_cast<HandleT>(-1);
  static constexpr HandleT kIndexMask = (HandleT { 1 } << kIndexBits) - 1;
  static constexpr HandleT kGenerationMask
    = (HandleT { 1 } << kGenerationBits) - 1;
  static constexpr HandleT kResourceTypeMask
    = (HandleT { 1 } << kResourceTypeBits) - 1;
  static constexpr HandleT kCustomMask = (HandleT { 1 } << kCustomBits) - 1;
  static constexpr HandleT kReservedMask = (HandleT { 1 } << kReservedBits) - 1;

public:
  // NOLINTBEGIN(*-magic-numbers)
  using GenerationT = std::conditional_t<kGenerationBits <= 16,
    std::conditional_t<kGenerationBits <= 8, uint8_t, uint16_t>, uint32_t>;
  using ResourceTypeT = std::conditional_t<kResourceTypeBits <= 16,
    std::conditional_t<kResourceTypeBits <= 8, uint8_t, uint16_t>, uint32_t>;
  using IndexT = std::conditional_t<kIndexBits <= 32, uint32_t, uint64_t>;
  using CustomT = std::conditional_t<kCustomBits <= 16,
    std::conditional_t<kCustomBits <= 8, uint8_t, uint16_t>, uint32_t>;
  using ReservedT = std::conditional_t<kReservedBits <= 16,
    std::conditional_t<kReservedBits <= 8, uint8_t, uint16_t>, uint32_t>;
  // NOLINTEND(*-magic-numbers)

  static constexpr GenerationT kGenerationMax = kGenerationMask;
  static constexpr ResourceTypeT kTypeNotInitialized = kResourceTypeMask;
  static constexpr ResourceTypeT kResourceTypeMax = kResourceTypeMask;
  static constexpr CustomT kCustomMax = kCustomMask;
  static constexpr ReservedT kReservedMax = kReservedMask;
  static constexpr IndexT kIndexMax = kIndexMask;
  static constexpr IndexT kInvalidIndex = kIndexMax;

protected:
  constexpr ResourceHandle();

public:
  explicit constexpr ResourceHandle(
    IndexT index, ResourceTypeT type = kTypeNotInitialized);

  ~ResourceHandle() = default;

  constexpr ResourceHandle(const ResourceHandle&) = default;
  constexpr auto operator=(const ResourceHandle& other) -> ResourceHandle&;

  constexpr ResourceHandle(ResourceHandle&& other) noexcept;
  constexpr auto operator=(ResourceHandle&& other) noexcept -> ResourceHandle&;

  constexpr auto operator==(const ResourceHandle& rhs) const -> bool;
  constexpr auto operator!=(const ResourceHandle& rhs) const -> bool;
  constexpr auto operator<(const ResourceHandle& rhs) const -> bool;

  [[nodiscard]] constexpr auto Handle() const -> HandleT;

  [[nodiscard]] constexpr auto IsValid() const noexcept -> bool;

  constexpr auto Invalidate() -> void;

  [[nodiscard]] constexpr auto Index() const -> IndexT;

  constexpr auto SetIndex(IndexT index) -> void;

  [[nodiscard]] constexpr auto Generation() const -> GenerationT;

  constexpr auto NewGeneration() -> void;

  [[nodiscard]] constexpr auto ResourceType() const -> ResourceTypeT;

  constexpr auto SetResourceType(ResourceTypeT type) -> void;

  [[nodiscard]] constexpr auto Custom() const -> CustomT;

  constexpr auto SetCustom(CustomT custom) -> void;

  [[nodiscard]] constexpr auto Reserved() const -> ReservedT;

  constexpr auto SetReserved(ReservedT reserved) -> void;

  [[nodiscard]] constexpr auto IsFree() const -> bool;

  constexpr auto SetFree(bool flag) -> void;

private:
  HandleT handle_ { kHandleMask };
  constexpr auto SetGeneration(GenerationT generation) -> void;
  static constexpr HandleT kCustomSetMask
    = ((HandleT { 1 } << (kIndexBits + kGenerationBits)) - 1)
    | kHandleMask << (kIndexBits + kGenerationBits + kCustomBits);

  static constexpr HandleT kResourceTypeSetMask
    = ((HandleT { 1 } << (kIndexBits + kGenerationBits + kCustomBits)) - 1)
    | kHandleMask << (kIndexBits + kGenerationBits + kCustomBits
        + kResourceTypeBits);

  static constexpr HandleT kGenerationSetMask
    = kHandleMask << (kIndexBits + kGenerationBits) | kIndexMask;

  static constexpr HandleT kIndexSetMask = kHandleMask << kIndexBits;

  static constexpr HandleT kReservedSetMask
    = (HandleT { 1 } << (kHandleBits - kReservedBits)) - 1;

  //=== Static Assertions ===-------------------------------------------------//

  static_assert(kReservedBits + kFreeBits + kCustomBits + kResourceTypeBits
        + kGenerationBits + kIndexBits
      == kHandleBits,
    "Bit allocation must sum to total handle bits");

  // NOLINTBEGIN(*-magic-numbers)
  static_assert(sizeof(GenerationT) * 8 >= kGenerationBits,
    "GenerationT size is insufficient for kGenerationBits");

  static_assert(sizeof(ResourceTypeT) * 8 >= kResourceTypeBits,
    "ResourceTypeT size is insufficient for kResourceTypeBits");

  static_assert(sizeof(IndexT) * 8 >= kIndexBits,
    "IndexT size is insufficient for kIndexBits");

  static_assert(sizeof(CustomT) * 8 >= kCustomBits,
    "CustomT size is insufficient for kCustomBits");

  static_assert(sizeof(ReservedT) * 8 >= kReservedBits,
    "ReservedT size is insufficient for kReservedBits");
  // NOLINTEND(*-magic-numbers)
};

//------------------------------------------------------------------------------
// Implementation of ResourceHandle methods
//------------------------------------------------------------------------------

constexpr ResourceHandle::ResourceHandle(
  const IndexT index, const ResourceTypeT type)
{
  SetIndex(index);
  SetResourceType(type);
  SetGeneration(0);
  SetCustom(0);
  SetReserved(0);
  SetFree(false);
}

constexpr ResourceHandle::ResourceHandle()
{
  SetIndex(kInvalidIndex);
  SetResourceType(kTypeNotInitialized);
  SetGeneration(0);
  SetCustom(0);
  SetReserved(0);
  SetFree(false);
}

constexpr ResourceHandle::ResourceHandle(ResourceHandle&& other) noexcept
  : handle_(other.handle_)

{
  other.handle_ = kInvalidIndex; // Reset the other handle to an invalid state
}

constexpr auto ResourceHandle::operator=(const ResourceHandle& other)
  -> ResourceHandle&
{
  if (this == &other) {
    return *this;
  }
  handle_ = other.handle_;
  return *this;
}

constexpr auto ResourceHandle::operator=(ResourceHandle&& other) noexcept
  -> ResourceHandle&
{
  if (this == &other) {
    return *this;
  }
  handle_ = other.handle_;
  other.handle_ = kInvalidIndex; // Reset the other handle to an invalid state
  return *this;
}

constexpr auto ResourceHandle::operator==(const ResourceHandle& rhs) const
  -> bool
{
  return handle_ == rhs.handle_;
}

constexpr auto ResourceHandle::operator!=(const ResourceHandle& rhs) const
  -> bool
{
  return handle_ != rhs.handle_;
}

constexpr auto ResourceHandle::operator<(const ResourceHandle& rhs) const
  -> bool
{
  return handle_ < rhs.handle_;
}

constexpr auto ResourceHandle::IsValid() const noexcept -> bool
{
  return Index() != kInvalidIndex;
}

constexpr auto ResourceHandle::Invalidate() -> void
{
  this->handle_ = kHandleMask;
}

constexpr auto ResourceHandle::Handle() const -> HandleT { return handle_; }

constexpr auto ResourceHandle::Index() const -> IndexT
{
  return handle_ & kIndexMask;
}

constexpr auto ResourceHandle::SetIndex(const IndexT index) -> void
{
  assert(index <= kIndexMax); // max value is invalid
  handle_ = (handle_ & kIndexSetMask) |
    // NOLINTNEXTLINE(clang-diagnostic-tautological-type-limit-compare)
    (index <= kIndexMax ? index : kInvalidIndex);
}

constexpr auto ResourceHandle::ResourceType() const -> ResourceTypeT
{
  return static_cast<ResourceTypeT>(
    handle_ >> (kIndexBits + kGenerationBits + kCustomBits)
    & kResourceTypeMask);
}

constexpr auto ResourceHandle::SetResourceType(const ResourceTypeT type) -> void
{
  assert(type <= kResourceTypeMax); // max value is not-initialized
  handle_ = (handle_ & kResourceTypeSetMask)
    | static_cast<HandleT>(type)
      << (kIndexBits + kGenerationBits + kCustomBits);
}

constexpr auto ResourceHandle::Custom() const -> CustomT
{
  return static_cast<CustomT>(
    handle_ >> (kIndexBits + kGenerationBits) & kCustomMask);
}

constexpr auto ResourceHandle::SetCustom(const CustomT custom) -> void
{
  assert(custom <= kCustomMax);
  handle_ = (handle_ & kCustomSetMask)
    | static_cast<HandleT>(custom) << (kIndexBits + kGenerationBits);
}

constexpr auto ResourceHandle::Reserved() const -> ReservedT
{
  return static_cast<ReservedT>(
    handle_ >> (kHandleBits - kReservedBits) & kReservedMask);
}

constexpr auto ResourceHandle::SetReserved(const ReservedT reserved) -> void
{
  assert(reserved <= kReservedMax);
  handle_ = (handle_ & kReservedSetMask)
    | static_cast<HandleT>(reserved) << (kHandleBits - kReservedBits);
}

constexpr auto ResourceHandle::Generation() const -> GenerationT
{
  return handle_ >> kIndexBits & kGenerationMask;
}

constexpr auto ResourceHandle::SetGeneration(GenerationT generation) -> void
{
  assert(generation <= kGenerationMax);
  // Wrap around
  // NOLINTNEXTLINE(clang-diagnostic-tautological-type-limit-compare)
  if (generation > kGenerationMax) {
    generation = 0;
  }
  handle_ = (handle_ & kGenerationSetMask)
    | static_cast<HandleT>(generation) << kIndexBits;
}

constexpr auto ResourceHandle::NewGeneration() -> void
{
  const auto current_generation = Generation();
  assert(current_generation <= kGenerationMax);
  const auto new_generation {
    current_generation == kGenerationMax ? 0 : current_generation + 1U
  };
  // Wrap around
  handle_ = (handle_ & kGenerationSetMask)
    | static_cast<HandleT>(new_generation) << kIndexBits;
}

constexpr auto ResourceHandle::IsFree() const -> bool
{
  return (handle_ & HandleT { 1 } << (kHandleBits - kReservedBits - 1)) != 0;
}

constexpr auto ResourceHandle::SetFree(const bool flag) -> void
{
  constexpr auto free_bit_position = kHandleBits - kReservedBits - 1;
  handle_ &= ~(HandleT { 1 } << free_bit_position);
  if (flag) {
    handle_ |= HandleT { 1 } << free_bit_position;
  }
}

constexpr auto to_string(const ResourceHandle& value) noexcept -> std::string
{
  try {
    if (!value.IsValid()) {
      return std::string { "RH(Invalid)" };
    }

    std::string result;
    result += "RH(Index: ";
    result += std::to_string(value.Index());
    result += ", ResourceType: ";
    result += std::to_string(value.ResourceType());
    result += ", Generation: ";
    result += std::to_string(value.Generation());

    if (value.Custom() != 0) {
      result += ", Custom: ";
      result += std::to_string(value.Custom());
    }
    if (value.Reserved() != 0) {
      result += ", Reserved: ";
      result += std::to_string(value.Reserved());
    }

    result += ", IsFree: ";
    result += value.IsFree() ? "true" : "false";
    result += ")";
    return result;
  } catch (...) {
    return std::string {};
  }
}

constexpr auto to_string_compact(const ResourceHandle& value) noexcept
{
  try {
    if (!value.IsValid()) {
      return std::string { "RH(Invalid)" };
    }

    std::string result;
    result += "RH(i:";
    result += std::to_string(value.Index());
    result += ", t:";
    result += std::to_string(value.ResourceType());
    result += ", g:";
    result += std::to_string(value.Generation());
    result += ", c:";
    result += std::to_string(value.Custom());
    result += ", r:";
    result += std::to_string(value.Reserved());
    result += ", f:";
    result += value.IsFree() ? "1" : "0";
    result += ")";
    return result;
  } catch (...) {
    return std::string {};
  }
}

} // namespace nova

template <> struct std::hash<nova::ResourceHandle> {
  auto operator()(const nova::ResourceHandle& handle) const noexcept -> size_t
  {
    return std::hash<nova::ResourceHandle::HandleT> {}(handle.Handle());
  }
};
