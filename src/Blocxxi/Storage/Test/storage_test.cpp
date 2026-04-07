//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <filesystem>

#include <Blocxxi/Core/primitives.h>
#include <Blocxxi/Storage/file_store.h>
#include <Blocxxi/Storage/in_memory_store.h>

namespace blocxxi::storage {

TEST(StorageTest, InMemoryStorePreservesBlockOrder)
{
  auto store = MakeInMemoryBlockStore();
  auto genesis = core::Block::MakeNext(
    core::BlockId {}, 0, { core::Transaction::FromText("genesis", "seed") }, "genesis");
  auto next = core::Block::MakeNext(
    genesis.header.id, 1, { core::Transaction::FromText("demo.tx", "payload") }, "demo");

  ASSERT_TRUE(store->PutBlock(genesis).ok());
  ASSERT_TRUE(store->PutBlock(next).ok());

  auto const chain = store->GetChain();
  ASSERT_EQ(chain.size(), 2U);
  EXPECT_EQ(chain.front().header.height, 0);
  EXPECT_EQ(chain.back().header.height, 1);
}

TEST(StorageTest, FileStoresRoundTripBlocksAndSnapshots)
{
  auto const root = std::filesystem::temp_directory_path() / "blocxxi-storage-test";
  std::filesystem::remove_all(root);

  auto block_store = MakeFileBlockStore(root);
  auto snapshot_store = MakeFileSnapshotStore(root);
  auto block = core::Block::MakeNext(
    core::BlockId {}, 0, { core::Transaction::FromText("genesis", "seed") }, "genesis");
  auto snapshot = core::ChainSnapshot {
    .height = 0,
    .head_id = block.header.id,
    .block_count = 1,
    .accepted_transactions = 1,
    .bootstrapped = true,
  };

  ASSERT_TRUE(block_store->PutBlock(block).ok());
  ASSERT_TRUE(snapshot_store->Save(snapshot).ok());

  auto const reloaded_block = block_store->GetBlock(block.header.id);
  auto const reloaded_snapshot = snapshot_store->Load();

  ASSERT_TRUE(reloaded_block.has_value());
  ASSERT_TRUE(reloaded_snapshot.has_value());
  EXPECT_EQ(reloaded_block->header.id, block.header.id);
  EXPECT_EQ(reloaded_snapshot->head_id, block.header.id);

  std::filesystem::remove_all(root);
}

} // namespace blocxxi::storage
