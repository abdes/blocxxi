//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <random>
#include <string>
#include <type_traits>

#include <Nova/Testing/GTest.h>

#include <Nova/Base/Macros.h>
#include <Nova/Base/ResourceHandle.h>
#include <Nova/Base/ResourceTable.h>

using nova::HandleSet;
using nova::ResourceHandle;
using nova::ResourceTable;

namespace {

struct Item {
  std::string value;

  bool constructed { false };
  bool copy_constructed { false };
  bool move_constructed { false };

  explicit Item(std::string str)
    : value(std::move(str))
    , constructed(true)
  {
  }

  ~Item() = default;

  Item(const Item& o)
    : value(o.value)
    , constructed(o.constructed)
    , copy_constructed(true)
    , move_constructed(o.move_constructed)
  {
  }

  Item(Item&& o) noexcept
    : value(std::move(o.value))
    , constructed(o.constructed)
    , copy_constructed(o.copy_constructed)
    , move_constructed(true)
  {
  }

  auto operator=(const Item& other) -> Item& = default;
  [[maybe_unused]] auto operator=(Item&& other) noexcept -> Item& = default;
};

} // namespace

namespace base::resources {

NOLINT_TEST(ResourceTableBasicTest, EmptyTable)
{
  static constexpr size_t kCapacity { 10 };
  static constexpr ResourceHandle::ResourceTypeT kItemType { 1 };

  const ResourceTable<Item> table(kItemType, kCapacity);
  EXPECT_EQ(table.GetItemType(), kItemType);
  EXPECT_TRUE(table.IsEmpty());
  EXPECT_EQ(table.Size(), 0);
  EXPECT_EQ(table.Capacity(), kCapacity);
}

NOLINT_TEST(ResourceTableBasicTest, ReserveGrowsAndDoesNotShrink)
{
  static constexpr size_t kCapacity { 4 };
  static constexpr ResourceHandle::ResourceTypeT kItemType { 1 };

  ResourceTable<Item> table(kItemType, kCapacity);
  EXPECT_EQ(table.Capacity(), kCapacity);

  table.Reserve(64);
  EXPECT_GE(table.Capacity(), 64U);

  const auto grown_capacity = table.Capacity();
  table.Reserve(8);
  EXPECT_EQ(table.Capacity(), grown_capacity);
}

NOLINT_TEST(ResourceTableBasicTest, HandlesRemainValidAcrossGrowth)
{
  static constexpr size_t kCapacity { 2 };
  static constexpr ResourceHandle::ResourceTypeT kItemType { 1 };

  ResourceTable<Item> table(kItemType, kCapacity);
  const auto first = table.Emplace("first");
  const auto second = table.Emplace("second");
  ASSERT_TRUE(table.Contains(first));
  ASSERT_TRUE(table.Contains(second));

  // Force backing vectors to grow beyond initial reserve.
  table.Reserve(64);
  for (size_t i = 0; i < 32; ++i) {
    table.Emplace("extra-" + std::to_string(i));
  }

  EXPECT_TRUE(table.Contains(first));
  EXPECT_TRUE(table.Contains(second));
  EXPECT_EQ(table.ItemAt(first).value, "first");
  EXPECT_EQ(table.ItemAt(second).value, "second");
}

NOLINT_TEST(ResourceTableBasicTest, InsertItem)
{
  static constexpr size_t kCapacity { 10 };
  static constexpr ResourceHandle::ResourceTypeT kItemType { 1 };

  ResourceTable<Item> table(kItemType, kCapacity);

  {
    Item item("Copied");
    auto handle = table.Insert(item);
    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(table.Size(), 1);
    EXPECT_EQ(handle.ResourceType(), kItemType);
    auto& item_in_table = table.ItemAt(handle);
    EXPECT_TRUE(item_in_table.constructed);
    EXPECT_TRUE(item_in_table.copy_constructed);
    EXPECT_FALSE(item_in_table.move_constructed);
  }

  {
    const Item const_item("Const Copied");
    auto handle = table.Insert(const_item);
    EXPECT_EQ(table.Size(), 2);
    auto& item_in_table = table.ItemAt(handle);
    EXPECT_TRUE(item_in_table.constructed);
    EXPECT_TRUE(item_in_table.copy_constructed);
    EXPECT_FALSE(item_in_table.move_constructed);
  }

  {
    Item moved_item("Moved");
    auto handle = table.Insert(std::move(moved_item));
    EXPECT_EQ(table.Size(), 3);
    auto& item_in_table = table.ItemAt(handle);
    EXPECT_TRUE(item_in_table.constructed);
    EXPECT_FALSE(item_in_table.copy_constructed);
    EXPECT_TRUE(item_in_table.move_constructed);
  }
}

NOLINT_TEST(ResourceTableBasicTest, EmplaceItem)
{
  static constexpr size_t kCapacity { 10 };
  static constexpr ResourceHandle::ResourceTypeT kItemType { 1 };

  ResourceTable<Item> table(kItemType, kCapacity);

  {
    auto handle = table.Emplace("Constructed");
    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(table.Size(), 1);
    EXPECT_EQ(handle.ResourceType(), kItemType);
    auto& item_in_table = table.ItemAt(handle);
    EXPECT_TRUE(item_in_table.constructed);
    EXPECT_FALSE(item_in_table.copy_constructed);
    EXPECT_FALSE(item_in_table.move_constructed);
  }
  {
    auto handle = table.Emplace(Item("Constructed"));
    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(table.Size(), 2);
    EXPECT_EQ(handle.ResourceType(), kItemType);
    auto& item_in_table = table.ItemAt(handle);
    EXPECT_TRUE(item_in_table.constructed);
    EXPECT_FALSE(item_in_table.copy_constructed);
    EXPECT_TRUE(item_in_table.move_constructed);
  }

  {
    const Item item("Copied");
    auto handle = table.Emplace(item);
    EXPECT_EQ(table.Size(), 3);
    auto& item_in_table = table.ItemAt(handle);
    EXPECT_TRUE(item_in_table.constructed);
    EXPECT_TRUE(item_in_table.copy_constructed);
  }

  {
    Item moved_item("Move Constructed");
    auto handle = table.Insert(std::move(moved_item));
    EXPECT_EQ(table.Size(), 4);
    auto& item_in_table = table.ItemAt(handle);
    EXPECT_TRUE(item_in_table.constructed);
    EXPECT_FALSE(item_in_table.copy_constructed);
    EXPECT_TRUE(item_in_table.move_constructed);
  }
}

NOLINT_TEST(ResourceTableBasicTest, EraseItemCallsItsDestructor)
{
  static constexpr size_t kCapacity { 10 };
  static constexpr ResourceHandle::ResourceTypeT kItemType { 1 };

  static auto item_destroyed { false };
  struct Item {
    explicit Item(std::string a_value = "value")
      : value(std::move(a_value))
    {
    }
    ~Item() { item_destroyed = true; }

    NOVA_DEFAULT_COPYABLE(Item)
    NOVA_DEFAULT_MOVABLE(Item)

    std::string value;
  };
  ResourceTable<Item> table(kItemType, kCapacity);

  const auto handle = table.Emplace();
  const auto erased = table.Erase(handle);
  EXPECT_EQ(erased, 1);
  EXPECT_TRUE(item_destroyed);
  EXPECT_EQ(table.Size(), 0);
}

// ---------------------------------------------------------------------------

class ResourceTableElementAccessTest : public testing::Test {
protected:
  static constexpr size_t kCapacity { 10 };
  static constexpr ResourceHandle::ResourceTypeT kItemType { 1 };

  ResourceTableElementAccessTest()
    : table_(kItemType, kCapacity)
  {
  }

  ResourceTable<Item> table_;
};

NOLINT_TEST_F(ResourceTableElementAccessTest, Contains_EmptyTable)
{
  const ResourceHandle handle(0, kItemType);
  ASSERT_TRUE(table_.IsEmpty());
  EXPECT_FALSE(table_.Contains(handle));
}

NOLINT_TEST_F(ResourceTableElementAccessTest, Contains_ValidHandle)
{
  const auto handle = table_.Emplace("Test Item");
  EXPECT_TRUE(table_.Contains(handle));
}

NOLINT_TEST_F(ResourceTableElementAccessTest, Contains_ValidHandleNoItem)
{
  const auto handle = table_.Emplace("Test Item");
  table_.Erase(handle);
  EXPECT_FALSE(table_.Contains(handle));
}

NOLINT_TEST_F(ResourceTableElementAccessTest, Contains_OutOfRangeHandle)
{
  auto handle = table_.Emplace("Test Item");
  handle.SetIndex(handle.Index() + 10);
  EXPECT_FALSE(table_.Contains(handle));
}

NOLINT_TEST_F(ResourceTableElementAccessTest, Contains_GenerationMismatch)
{
  const auto handle = table_.Emplace("Test Item");
  EXPECT_TRUE(table_.Contains(handle));

  // Manually increment the generation of the handle to simulate a mismatch
  ResourceHandle mismatched_handle = handle;
  mismatched_handle.NewGeneration();
  EXPECT_FALSE(table_.Contains(mismatched_handle));
}

NOLINT_TEST_F(ResourceTableElementAccessTest, Contains_InvalidHandle)
{
  auto handle = table_.Emplace("Test Item");
  EXPECT_TRUE(table_.Contains(handle));
  handle.Invalidate();
  EXPECT_FALSE(table_.Contains(handle));
}

NOLINT_TEST_F(ResourceTableElementAccessTest, Contains_HandleHasDifferentType)
{
  const auto good_handle = table_.Emplace("Test Item");
  EXPECT_TRUE(table_.Contains(good_handle));

  ResourceHandle bad_handle(good_handle);
  bad_handle.SetResourceType(good_handle.ResourceType() + 1);
  EXPECT_FALSE(table_.Contains(bad_handle));
}

NOLINT_TEST_F(ResourceTableElementAccessTest, ItemAt_ValidHandle)
{
  const auto handle = table_.Emplace("Test Item");
  const auto& item = table_.ItemAt(handle);
  EXPECT_EQ(item.value, "Test Item");
}

NOLINT_TEST_F(ResourceTableElementAccessTest, ItemAt_InvalidHandle)
{
  ResourceHandle handle(0, kItemType);
  handle.Invalidate();
  NOLINT_EXPECT_THROW(auto _ = table_.ItemAt(handle), std::invalid_argument);
}

NOLINT_TEST_F(ResourceTableElementAccessTest, ItemAt_HandleForErasedItem)
{
  const auto handle = table_.Emplace("Item to be freed");
  table_.Erase(handle);
  NOLINT_EXPECT_THROW(auto _ = table_.ItemAt(handle), std::invalid_argument);
}

NOLINT_TEST_F(ResourceTableElementAccessTest, ItemAt_GenerationMismatch)
{
  const auto handle = table_.Emplace("Test Item");
  NOLINT_EXPECT_NO_THROW(auto& item = table_.ItemAt(handle);
    EXPECT_EQ(item.value, "Test Item"));

  // Manually increment the generation of the handle to simulate a mismatch
  ResourceHandle mismatched_handle = handle;
  mismatched_handle.NewGeneration();
  NOLINT_EXPECT_THROW(
    [[maybe_unused]] auto _ = table_.ItemAt(mismatched_handle),
    std::invalid_argument);
}

NOLINT_TEST_F(ResourceTableElementAccessTest, Items_EmptyTable)
{
  const auto items = table_.Items();
  EXPECT_TRUE(items.empty());
}

NOLINT_TEST_F(ResourceTableElementAccessTest, Items_NonEmptyTable)
{
  table_.Emplace("Item 1");
  table_.Emplace("Item 2");
  const auto items = table_.Items();
  EXPECT_EQ(items.size(), 2);
  EXPECT_EQ(items[0].value, "Item 1");
  EXPECT_EQ(items[1].value, "Item 2");
}

// ---------------------------------------------------------------------------

class ResourceTableInsert : public testing::Test {
protected:
  static constexpr size_t kCapacity { 5 };
  static constexpr ResourceHandle::ResourceTypeT kItemType { 1 };

  ResourceTableInsert()
    : table_(kItemType, kCapacity)
  {
  }

  ResourceTable<Item> table_;
};

NOLINT_TEST_F(ResourceTableInsert, InsertSingleItem)
{
  const auto handle = table_.Emplace("Single Item");
  EXPECT_TRUE(handle.IsValid());
  EXPECT_EQ(table_.Size(), 1);
  EXPECT_EQ(handle.ResourceType(), kItemType);
  const auto& item = table_.ItemAt(handle);
  EXPECT_EQ(item.value, "Single Item");
}

NOLINT_TEST_F(ResourceTableInsert, InsertMultipleItems)
{
  const auto handle1 = table_.Emplace("Item 1");
  const auto handle2 = table_.Emplace("Item 2");
  const auto handle3 = table_.Emplace("Item 3");
  EXPECT_TRUE(handle1.IsValid());
  EXPECT_TRUE(handle2.IsValid());
  EXPECT_TRUE(handle3.IsValid());
  EXPECT_EQ(table_.Size(), 3);
  EXPECT_EQ(table_.ItemAt(handle1).value, "Item 1");
  EXPECT_EQ(table_.ItemAt(handle2).value, "Item 2");
  EXPECT_EQ(table_.ItemAt(handle3).value, "Item 3");
}

NOLINT_TEST_F(ResourceTableInsert, InsertWhenTableIsFull)
{
  for (size_t i = 0; i < kCapacity; ++i) {
    table_.Emplace("Item " + std::to_string(i + 1));
  }
  EXPECT_EQ(table_.Size(), kCapacity);
  NOLINT_EXPECT_NO_THROW(table_.Emplace("Overflow Item"));
  EXPECT_EQ(table_.Size(), kCapacity + 1);
}

NOLINT_TEST_F(ResourceTableInsert, InsertAndDeleteSameItemMultipleTimes)
{
  for (size_t i = 0; i < kCapacity * 2; ++i) {
    const auto handle = table_.Emplace("Item");
    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(table_.Size(), 1);
    table_.Erase(handle);
    EXPECT_EQ(table_.Size(), 0);
  }
}

// ---------------------------------------------------------------------------

class ResourceTableErase : public testing::Test {
protected:
  static constexpr size_t kCapacity { 5 };
  static constexpr ResourceHandle::ResourceTypeT kItemType { 1 };

  ResourceTableErase()
    : table_(kItemType, kCapacity)
  {
  }

  ResourceTable<Item> table_;
};

NOLINT_TEST_F(ResourceTableErase, EraseSingleItem)
{
  const auto handle = table_.Emplace("Single Item");
  EXPECT_TRUE(handle.IsValid());
  EXPECT_EQ(table_.Size(), 1);
  EXPECT_EQ(table_.Erase(handle), 1);
  EXPECT_EQ(table_.Size(), 0);
}

NOLINT_TEST_F(ResourceTableErase, EraseMultipleItems)
{
  const auto handle1 = table_.Emplace("Item 1");
  const auto handle2 = table_.Emplace("Item 2");
  const auto handle3 = table_.Emplace("Item 3");
  EXPECT_EQ(table_.Size(), 3);
  EXPECT_EQ(table_.Erase(handle1), 1);
  EXPECT_EQ(table_.Erase(handle2), 1);
  EXPECT_EQ(table_.Erase(handle3), 1);
  EXPECT_EQ(table_.Size(), 0);
}

NOLINT_TEST_F(ResourceTableErase, EraseWhenTableIsEmpty)
{
  const ResourceHandle handle(0, kItemType);
  EXPECT_EQ(table_.Erase(handle), 0);
}

NOLINT_TEST_F(ResourceTableErase, EraseSameItemTwice)
{
  const auto handle = table_.Emplace("Item");
  EXPECT_TRUE(handle.IsValid());
  EXPECT_EQ(table_.Size(), 1);
  EXPECT_EQ(table_.Erase(handle), 1);
  EXPECT_EQ(table_.Size(), 0);
  EXPECT_EQ(table_.Erase(handle), 0);
}

NOLINT_TEST_F(ResourceTableErase, EraseItemWhenContainsThrows)
{
  const auto handle = table_.Emplace("Item");
  EXPECT_TRUE(handle.IsValid());
  EXPECT_EQ(table_.Size(), 1);

  // Manually invalidate the handle to simulate a condition where Contains would
  // throw
  ResourceHandle invalid_handle = handle;
  invalid_handle.Invalidate();

  EXPECT_EQ(table_.Erase(invalid_handle), 0);
  EXPECT_EQ(table_.Size(), 1);
}

NOLINT_TEST(ResourceTableTest, SparseArrayWithHoles)
{
  static constexpr size_t kCapacity { 3 };
  static constexpr ResourceHandle::ResourceTypeT kItemType { 1 };

  ResourceTable<std::string> table(kItemType, kCapacity);
  table.Emplace("1");
  auto handle_2 = table.Emplace("2");
  table.Emplace("3");
  EXPECT_EQ(table.Size(), 3);
  EXPECT_EQ(table.Capacity(), 3);
  EXPECT_EQ(table.Erase(handle_2), 1);
  EXPECT_FALSE(table.Contains(handle_2));
  EXPECT_EQ(table.Capacity(), 3);
  handle_2 = table.Emplace("2");
  EXPECT_TRUE(table.Contains(handle_2));
  EXPECT_EQ(table.Size(), 3);
  EXPECT_EQ(table.Capacity(), 3);
  const auto handle_4 = table.Emplace("4");
  EXPECT_TRUE(table.Contains(handle_4));
  EXPECT_EQ(table.Size(), 4);
  EXPECT_GE(table.Capacity(), 4U);
}

NOLINT_TEST(ResourceTableTest, Defragment)
{
  static constexpr size_t kCapacity { 5 };
  static constexpr ResourceHandle::ResourceTypeT kItemType { 1 };

  ResourceTable<int> table(kItemType, kCapacity);

  const auto handle_43 = table.Emplace(43);
  const auto handle_42 = table.Emplace(42);
  table.Erase(handle_43);
  table.Emplace(41);
  table.Erase(handle_42);

  table.Emplace(45);
  table.Emplace(44);

  const size_t swaps = table.Defragment(
    [](const int& a, const int& b) -> bool { return a < b; });

  EXPECT_EQ(swaps, 2);
}

class ResourceTableTestPreFilled : public testing::Test {
public:
  static constexpr size_t kCapacity { 30 };
  static constexpr ResourceHandle::ResourceTypeT kItemType { 1 };

  ResourceTableTestPreFilled()
    : table_(kItemType, kCapacity)
  {
  }

protected:
  auto SetUp() -> void override
  {
    for (auto index = 1U; index <= kCapacity; index++) {
      handles_.push_back(table_.Emplace(std::to_string(index)));
    }
    std::ranges::for_each(handles_, [this](const auto& handle) -> auto {
      EXPECT_TRUE(table_.Contains(handle));
    });
  }

  ResourceTable<std::string> table_;
  HandleSet handles_;
};

NOLINT_TEST_F(ResourceTableTestPreFilled, EraseItems)
{
  table_.EraseItems(handles_);
  EXPECT_TRUE(table_.IsEmpty());
  EXPECT_EQ(table_.Size(), 0);
  EXPECT_EQ(table_.Capacity(), kCapacity);
}

NOLINT_TEST_F(ResourceTableTestPreFilled, Reset)
{
  table_.Reset();
  EXPECT_TRUE(table_.IsEmpty());
  EXPECT_EQ(table_.Size(), 0);
  EXPECT_EQ(table_.Capacity(), kCapacity);
  const auto handle = table_.Emplace("after_reset");
  EXPECT_EQ(handle.Generation(), 0);
}

NOLINT_TEST_F(ResourceTableTestPreFilled, Clear)
{
  table_.Clear();
  EXPECT_TRUE(table_.IsEmpty());
  EXPECT_EQ(table_.Size(), 0);
  EXPECT_EQ(table_.Capacity(), kCapacity);
  const auto handle = table_.Emplace("after_reset");
  EXPECT_GT(handle.Generation(), 0);
}

NOLINT_TEST(ResourceTableTest, RandomInsertEraseAndDefragment)
{
  static constexpr size_t kCapacity { 50 };
  static constexpr ResourceHandle::ResourceTypeT kItemType { 1 };
  static constexpr size_t kOperations { 200 };

  ResourceTable<Item> table(kItemType, kCapacity);
  std::vector<ResourceHandle> handles;
  using IndexType = std::vector<ResourceHandle>::difference_type;
  std::mt19937 rng(std::random_device {}());
  std::uniform_int_distribution dist(0, 10);

  for (size_t i = 0; i < kOperations; ++i) {
    if (dist(rng) < 5 && !handles.empty()) {
      // Erase a random item
      std::uniform_int_distribution<IndexType> erase_dist(
        0, static_cast<IndexType>(handles.size() - 1));
      const IndexType index = erase_dist(rng);
      table.Erase(handles[index]);
      handles.erase(handles.begin() + index);
    } else {
      // Insert a new item
      auto handle = table.Emplace("Item " + std::to_string(i));
      handles.push_back(handle);
    }
  }

  // Defragment the table
  table.Defragment(
    [](const Item& a, const Item& b) -> bool { return a.value < b.value; });

  // Check that the table is not corrupted and has the expected items
  EXPECT_EQ(table.Size(), handles.size());
  for (const auto& handle : handles) {
    EXPECT_TRUE(table.Contains(handle));
    const auto& item = table.ItemAt(handle);
    EXPECT_FALSE(item.value.empty());
  }
}

} // namespace
