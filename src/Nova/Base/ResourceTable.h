//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <span>
#include <stdexcept>
#include <vector>

#include <Nova/Base/Logging.h>
#include <Nova/Base/Macros.h>
#include <Nova/Base/ResourceHandle.h>

namespace nova {

/*!
 @class ResourceTable

 Lookup table for resources indexed with a resource handle.

 The same resource handle data type is used in two different ways: as an
 externally visible handle, also used as an index into a sparse set, and as an
 internal index into a dense set storing the actual objects.

 The sparse set is an array with holes. The holes are created when items are
 removed from the set. Each slot in the sparse set stores an internal index that
 points to the location of the actual data in the dense set. The dense set is a
 packed array that can be moved around without impacting the externally visible
 handles.

 The sparse set also maintains a free list of available slots. The free list
 links the holes together using the reserved free bit in the handle. Gaps in the
 sparse set resulting from deletions will be filled by subsequent insertions.
 The sparse set will always fill gaps before growing, and thus will itself try
 to remain dense.

 The dense set stores the data itself, tightly packed to ensure best cache
 locality during traversals.

 Constant time lookups are achieved because handles contain information about
 their location in the sparse set. This makes lookups into the sparse set
 nothing more than an array lookup. The information contained in the sparse set
 knows the item's exact location in the dense set, so again just an array
 lookup.

 Constant time insertions are done by pushing the new item to the back of the
 dense set, and using the first available slot in the sparse set's freelist for
 the index.

 Constant time removal is made possible by using the "swap and pop" trick in the
 dense set. That is, we swap the item being removed with the last item in the
 array, and then pop_back to remove it. We also have to update the index of the
 last item referenced in the sparse set. We have to discover the location in the
 sparse set of the corresponding inner id for the (former) last item that we are
 swapping with. The index field must be changed to reference its new location in
 the dense set.

 Constant time removal is achieved by swapping the item being removed with the
 last item in the dense set and then popping the last item off of the dense set.
 The index of the last item referenced in the sparse set must also be updated.
 To do this, we first need to find the location of the corresponding internal
 handle for the (former) last item that we are swapping with in the sparse set.
 Once we have found the location, we can change the index field to reference the
 new location of the item in the dense set.

 We introduce a third storage set, called the meta set, which contains an index
 back to the sparse set. This gives us a two-way link between the dense set and
 the sparse set. We do not combine the meta information directly into the dense
 set because it is currently only used for deletion. There is no need to harm
 cache locality and alignment of the main object store for infrequently used
 data.

 @warning IMPORTANT: References, pointers, and iterators to items become invalid
 when the table grows beyond the initial reserve_count capacity. This happens
 because the underlying std::vector containers may reallocate memory during
 insertion operations. ResourceHandle objects themselves remain valid as they
 use indices rather than memory addresses. Always re-acquire references after
 insertions that might cause reallocation.

 Inspired by ID Lookup in the stingray core.
 http://bitsquid.blogspot.com/2011/09/managing-decoupling-part-4-id-lookup.html

 Uses reverse lookup from dense set to sparse set from
 https://github.com/y2kiah/griffin-containers.

 @copyright The MIT License (MIT), Copyright (c) 2015 Jeff Kiah.
*/

using HandleSet = std::vector<ResourceHandle>;

template <typename T> class ResourceTable {
public:
  struct Meta {
    ResourceHandle::IndexT dense_to_sparse;
  };

  using DenseSet = std::vector<T>;
  using MetaSet = std::vector<Meta>;

  ResourceTable(
    const ResourceHandle::ResourceTypeT item_type, size_t reserve_count)
    : item_type_(item_type)
  {
    assert(reserve_count < ResourceHandle::kIndexMax);
    sparse_table_.reserve(reserve_count);
    items_.reserve(reserve_count);
    meta_.reserve(reserve_count);
  }

  virtual ~ResourceTable() = default;

  NOVA_MAKE_NON_COPYABLE(ResourceTable)
  NOVA_MAKE_NON_MOVABLE(ResourceTable)

  [[nodiscard]] auto GetItemType() const -> ResourceHandle::ResourceTypeT
  {
    return item_type_;
  }

  //=== Element Access ===----------------------------------------------------//

  [[nodiscard]] auto Contains(const ResourceHandle& handle) const noexcept
    -> bool;
  [[nodiscard]] auto ItemAt(const ResourceHandle& handle) -> T&;
  [[nodiscard]] auto ItemAt(const ResourceHandle& handle) const -> const T&;
  [[nodiscard]] auto TryGet(const ResourceHandle& handle) noexcept -> T*;
  [[nodiscard]] auto TryGet(const ResourceHandle& handle) const noexcept
    -> const T*;

  /*
  Direct access to items set for iterating over them with no modification of the
  set or its items.
   */
  [[nodiscard]] auto Items() const -> std::span<const T> { return items_; }

  //=== Capacity ===----------------------------------------------------------//

  [[nodiscard]] auto Size() const noexcept { return items_.size(); }
  [[nodiscard]] auto IsEmpty() const noexcept { return items_.empty(); }
  [[nodiscard]] auto Capacity() const noexcept { return items_.capacity(); }
  auto Reserve(size_t reserve_count) -> void;

  //=== Modifiers ===---------------------------------------------------------//

  template <typename URef = T>
    requires std::is_same_v<std::remove_cvref_t<URef>, T>
  auto Insert(URef&& item) -> ResourceHandle;
  /*!
   * Inserts an item in the table, constructing the item in place at the
   * position chosen by the table. Prefer to use this instead of Insert when
   * adding an item on the fly, passing its properties as arguments.
   */
  template <typename... Params>
  auto Emplace(Params&&... args) -> ResourceHandle;

  // Return 1 if item was found and erased; 0 otherwise.
  auto Erase(const ResourceHandle& handle) -> size_t;

  // Return count of items that were removed.
  auto EraseItems(const HandleSet& handles) -> size_t;

  /*
  Removes all items, leaving the sparse set intact by adding each entry to the
  freelist and incrementing its generation.

  This operation is slower than `Reset()`, but safer for the detection of
  stale handles later. Complexity is linear.
  */
  auto Clear() noexcept -> void;

  /*
  Removes all items, destroying the sparse set. Leaves the container's
  capacity, but otherwise equivalent to a default-constructed container.

  This is faster than `Clear()`, but cannot safely detect lookups by stale
  handles obtained before the reset. Complexity is constant.
  */
  auto Reset() noexcept -> void;

  /*
  De-fragmentation uses the comparison function `comp` to establish an ideal
  order or the dense set to maximum cache locality for traversals.

  The dense set can become fragmented over time due to removal operations.
  This can be an expensive operation; use the `max_swaps` parameter to limit
  the number of swaps that will occur before the function returns or use `0`to
  run until completion.

  The comparison function object (std::function, lambda or function pointer)
  should return true if the first argument is less than (i.e. is ordered
  before) the second. The signature of the comparison function should be
  equivalent to the following:

    `bool cmp(const T& a, const T& b);`

  While the signature does not need to have const&, the function must not
  modify the objects passed to it and must be able to accept all values of
  type (possibly const) T.

  Returns the number of swaps that occurred;
  */
  template <typename Compare>
  auto Defragment(Compare comp, size_t max_swaps = 0) -> size_t;

private:
  [[nodiscard]] auto GetInnerIndex(const ResourceHandle& handle) const
    -> ResourceHandle::IndexT;

  // Common implementation for insertion that prepares the handle and metadata
  // but delegates item creation to the provided functor
  template <typename ItemCreator>
  auto InsertImpl(ItemCreator&& item_creator) -> ResourceHandle;

  [[nodiscard]] auto IsFreeListEmpty() const
  {
    // Having the front at the max index value, means the freelist is empty.
    // The back will implicitly be equal to the front.
    return freelist_front_ == ResourceHandle::kIndexMax;
  }

  // Index of the first item in the freelist
  ResourceHandle::IndexT freelist_front_ = ResourceHandle::kInvalidIndex;
  // Index of the last item in the freelist
  ResourceHandle::IndexT freelist_back_ = ResourceHandle::kInvalidIndex;

  // Resource type of handles produced when inserting items into this table.
  // All items in a table have the same type. Multiple tables need to be used
  // to store different resource types.
  ResourceHandle::ResourceTypeT item_type_;

  // Stores the `inner` handles, used as internal indices into the dense set
  // and to form the freelist of available slots (holes in the array).
  HandleSet sparse_table_;

  // Stores the actual `items` inserted into the table.
  DenseSet items_;

  // Stores `meta` information (extra information, not in the item data) for
  // each item. Currently used for the reverse indexing from the dense set to
  // the sparse set.
  MetaSet meta_;

  // Fragmentation state of the dense set. Set to `true` when items are
  // inserted into or removed from this table. Reset to `false` after
  // de-fragmentation.
  bool fragmented_ { false };
};

//------------------------------------------------------------------------------
// Implementation of ResourceTable methods
//------------------------------------------------------------------------------

namespace detail {

  template <typename T>
  concept InternalSet = requires(T set) {
    { set.size() } -> std::convertible_to<size_t>;
  };

  auto NewIndex(const InternalSet auto& set)
  {
    return static_cast<ResourceHandle::IndexT>(set.size());
  }

} // namespace detail

template <typename T>
auto ResourceTable<T>::ItemAt(const ResourceHandle& handle) -> T&
{
  return items_[GetInnerIndex(handle)];
}

template <typename T>
auto ResourceTable<T>::ItemAt(const ResourceHandle& handle) const -> const T&
{
  return items_[GetInnerIndex(handle)];
}

template <typename T>
auto ResourceTable<T>::TryGet(const ResourceHandle& handle) noexcept -> T*
{
  if (handle.Index() >= sparse_table_.size()
    || handle.ResourceType() != item_type_) {
    return nullptr;
  }
  const ResourceHandle inner_id = sparse_table_[handle.Index()];
  if (inner_id.IsFree()) {
    return nullptr;
  }
  if (handle.Generation() != inner_id.Generation()) {
    return nullptr;
  }
  assert(inner_id.Index() < items_.size());
  return &items_[inner_id.Index()];
}

template <typename T>
auto ResourceTable<T>::TryGet(const ResourceHandle& handle) const noexcept
  -> const T*
{
  if (handle.Index() >= sparse_table_.size()
    || handle.ResourceType() != item_type_) {
    return nullptr;
  }
  const ResourceHandle inner_id = sparse_table_[handle.Index()];
  if (inner_id.IsFree()) {
    return nullptr;
  }
  if (handle.Generation() != inner_id.Generation()) {
    return nullptr;
  }
  assert(inner_id.Index() < items_.size());
  return &items_[inner_id.Index()];
}

template <typename T>
auto ResourceTable<T>::Contains(const ResourceHandle& handle) const noexcept
  -> bool
{
  // quick bailout before starting the lookup
  if (handle.Index() >= sparse_table_.size()
    || handle.ResourceType() != item_type_) {
    return false;
  }

  const ResourceHandle inner_id = sparse_table_[handle.Index()];

  if (inner_id.IsFree()) {
    return false;
  }

  assert(inner_id.Index() < items_.size());
  return handle.Generation() == inner_id.Generation();
}

template <typename T>
auto ResourceTable<T>::GetInnerIndex(const ResourceHandle& handle) const
  -> uint32_t
{
  if (!handle.IsValid()) {
    throw std::invalid_argument("invalid handle");
  }
  if (handle.Index() >= detail::NewIndex(sparse_table_)) {
    throw std::out_of_range("bad handle, index out of range");
  }
  if (handle.ResourceType() != item_type_) {
    throw std::invalid_argument("item type mismatch, using wrong table?");
  }

  const ResourceHandle inner_handle = sparse_table_[handle.Index()];
  if (inner_handle.IsFree()) {
    throw std::invalid_argument("bad handle, item already erased");
  }
  if (handle.Generation() != inner_handle.Generation()) {
    throw std::invalid_argument(
      "external handle is stale (obsolete generation)");
  }

  assert(inner_handle.Index() < detail::NewIndex(items_)
    && "corrupted table, inner index is out of range");

  return inner_handle.Index();
}

// template <typename T, typename UR>
// ResourceHandle ResourceTable<T>::Insert(T&& item)
template <typename T>
template <typename URef>
  requires std::is_same_v<std::remove_cvref_t<URef>, T>
auto ResourceTable<T>::Insert(URef&& item) -> ResourceHandle
{
  return InsertImpl(
    [&]() -> auto { items_.push_back(std::forward<URef>(item)); });
}

template <typename T>
template <typename... Params>
auto ResourceTable<T>::Emplace(Params&&... args) -> ResourceHandle
{
  return InsertImpl([&]() -> auto {
    // Use perfect forwarding to construct the item in place
    items_.emplace_back(std::forward<Params>(args)...);
  });
}

template <typename T>
template <typename ItemCreator>
auto ResourceTable<T>::InsertImpl(ItemCreator&& item_creator) -> ResourceHandle
{
  // We never fill the table beyond the maximum valid index value. This is very
  // unlikely, so we just assert for it and not test it in production.
  assert(Size() < ResourceHandle::kIndexMax
    && "index will be out of range, increase bit size of the index values");

  // Check if we're about to exceed capacity and log a warning
  const bool will_reallocate_items = items_.size() == items_.capacity();
  const bool will_reallocate_sparse
    = IsFreeListEmpty() && sparse_table_.size() == sparse_table_.capacity();

  if (will_reallocate_items || will_reallocate_sparse) {
    LOG_F(WARNING,
      "ResourceTable reallocation detected! Current size: {}, capacity: {}. "
      "All references, pointers, and iterators to items will be invalidated. "
      "Consider increasing reserve_count in constructor to avoid performance "
      "impact.",
      items_.size(), items_.capacity());
  }

  ResourceHandle handle(ResourceHandle::kInvalidIndex, item_type_);
  fragmented_ = true;

  if (IsFreeListEmpty()) {
    handle.SetIndex(detail::NewIndex(sparse_table_));
    handle.SetResourceType(item_type_);
    handle.SetFree(false);
    sparse_table_.push_back(handle);
  } else {
    const uint32_t outer_index = freelist_front_;
    ResourceHandle& inner_handle = sparse_table_[outer_index];

    // the index of a free slot refers to the next free slot
    freelist_front_ = inner_handle.Index();
    if (IsFreeListEmpty()) {
      freelist_back_ = freelist_front_;
    }

    // convert the index from freelist to inner index
    inner_handle.SetFree(false);
    inner_handle.SetIndex(detail::NewIndex(items_));

    handle = inner_handle;
    handle.SetIndex(outer_index);
  }

  // Delegate item creation to the provided functor
  const auto& item_creator_ref = std::forward<ItemCreator>(item_creator);
  item_creator_ref();
  meta_.push_back({ handle.Index() });

  return handle;
}

template <typename T>
auto ResourceTable<T>::Erase(const ResourceHandle& handle) -> size_t
{
  try {
    if (!Contains(handle)) {
      return 0;
    }
  } catch (const std::exception&) {
    return 0;
  }

  fragmented_ = true;

  ResourceHandle inner_handle = sparse_table_[handle.Index()];
  ResourceHandle::IndexT inner_index = inner_handle.Index();

  // push this slot to the back of the freelist
  inner_handle.SetFree(true);
  // increment generation so remaining outer ids go stale
  inner_handle.NewGeneration();
  // max value represents the end of the freelist
  inner_handle.SetIndex(ResourceHandle::kIndexMax);
  // write outer id changes back to the array
  sparse_table_[handle.Index()] = inner_handle;

  if (IsFreeListEmpty()) {
    // if the freelist was empty, it now starts (and ends) at this index
    freelist_front_ = handle.Index();
    freelist_back_ = freelist_front_;
  } else {
    // previous back of the freelist points to new back
    sparse_table_[freelist_back_].SetIndex(handle.Index());
    // new freelist back is stored
    freelist_back_ = handle.Index();
  }

  // remove the component by swapping with the last element, then pop_back
  if (inner_index != items_.size() - 1) {
    std::swap(items_[inner_index], items_.back());
    std::swap(meta_[inner_index], meta_.back());

    // fix the ComponentId index of the swapped component
    sparse_table_[meta_[inner_index].dense_to_sparse].SetIndex(inner_index);
  }

  items_.pop_back();
  meta_.pop_back();

  return 1;
}

template <typename T>
auto ResourceTable<T>::EraseItems(const HandleSet& handles) -> size_t
{
  size_t count = 0;
  for (const auto& handle : handles) {
    count += Erase(handle);
  }
  return count;
}

template <typename T> auto ResourceTable<T>::Clear() noexcept -> void
{
  if (const uint32_t size = detail::NewIndex(sparse_table_); size > 0) {
    items_.clear();
    meta_.clear();

    freelist_front_ = 0;
    freelist_back_ = size - 1;
    fragmented_ = false;

    for (uint32_t index = 0; index < size; ++index) {
      auto& handle = sparse_table_[index];
      handle.SetFree(true);
      handle.NewGeneration();
      handle.SetIndex(index + 1);
    }
    sparse_table_[size - 1].SetIndex(ResourceHandle::kInvalidIndex);
  }
}

template <typename T> auto ResourceTable<T>::Reset() noexcept -> void
{
  freelist_front_ = ResourceHandle::kIndexMax;
  freelist_back_ = ResourceHandle::kIndexMax;
  fragmented_ = false;

  items_.clear();
  meta_.clear();
  sparse_table_.clear();
}

template <typename T>
auto ResourceTable<T>::Reserve(const size_t reserve_count) -> void
{
  assert(reserve_count < ResourceHandle::kIndexMax);
  if (reserve_count <= Capacity()) {
    return;
  }

  // Grow all backing sets together so sparse, dense, and metadata stay aligned
  // in capacity planning.
  sparse_table_.reserve(reserve_count);
  items_.reserve(reserve_count);
  meta_.reserve(reserve_count);
}

template <typename T>
template <typename Compare>
auto ResourceTable<T>::Defragment(Compare comp, const size_t max_swaps)
  -> size_t
{
  if (!fragmented_) {
    return 0;
  }
  size_t swaps = 0;

  ResourceHandle::IndexT index = 1;
  for (; index < items_.size() && (max_swaps == 0 || swaps < max_swaps);
    ++index) {
    T tmp = items_[index];
    Meta tmp_meta = meta_[index];

    std::ptrdiff_t i = static_cast<std::ptrdiff_t>(index) - 1;
    ResourceHandle::IndexT j1 = index;

    // trivially copyable implementation
    if (std::is_trivially_copyable_v<T>) {
      while (i >= 0 && comp(items_[i], tmp)) {
        sparse_table_[meta_[i].dense_to_sparse].SetIndex(j1);
        --i;
        --j1;
      }
      if (j1 != index) {
        std::memmove(&items_[j1 + 1], &items_[j1], sizeof(T) * (index - j1));
        std::memmove(&meta_[j1 + 1], &meta_[j1], sizeof(Meta) * (index - j1));
        ++swaps;

        items_[j1] = std::move(tmp);
        meta_[j1] = tmp_meta;
        sparse_table_[meta_[j1].dense_to_sparse].SetIndex(j1);
      }
    }
    // standard implementation
    else {
      while (i >= 0 && (max_swaps == 0 || swaps < max_swaps)
        && comp(items_[i], tmp)) {
        items_[j1] = std::move(items_[i]);
        meta_[j1] = std::move(meta_[i]);
        sparse_table_[meta_[j1].dense_to_sparse].SetIndex(j1);
        --i;
        --j1;
        ++swaps;
      }

      if (j1 != index) {
        items_[j1] = std::move(tmp);
        meta_[j1] = std::move(tmp_meta);
        sparse_table_[meta_[j1].dense_to_sparse].SetIndex(j1);
      }
    }
  }

  if (index == items_.size()) {
    fragmented_ = false;
  }

  return swaps;
}

} // namespace nova
