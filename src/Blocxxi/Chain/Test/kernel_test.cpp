//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <Blocxxi/Chain/kernel.h>

namespace blocxxi::chain {
namespace {

class MemoryBlockStore final : public BlockStore {
public:
  auto PutBlock(core::Block const& block) -> core::Status override
  {
    blocks.push_back(block);
    return core::Status::Success();
  }

  auto GetBlock(core::BlockId const& id) const -> std::optional<core::Block> override
  {
    for (auto const& block : blocks) {
      if (block.header.id == id) {
        return block;
      }
    }
    return std::nullopt;
  }

  auto GetChain() const -> std::vector<core::Block> override { return blocks; }

  std::vector<core::Block> blocks {};
};

class MemorySnapshotStore final : public SnapshotStore {
public:
  auto Save(core::ChainSnapshot const& next) -> core::Status override
  {
    snapshot = next;
    return core::Status::Success();
  }

  auto Load() const -> std::optional<core::ChainSnapshot> override
  {
    return snapshot;
  }

  std::optional<core::ChainSnapshot> snapshot {};
};

} // namespace

TEST(ChainKernelTest, BootstrapCreatesGenesisWithoutNetworking)
{
  auto blocks = std::make_shared<MemoryBlockStore>();
  auto snapshots = std::make_shared<MemorySnapshotStore>();
  auto kernel = Kernel(core::ChainConfig {}, blocks, snapshots);

  auto const status = kernel.Bootstrap();

  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(kernel.Snapshot().bootstrapped);
  EXPECT_EQ(kernel.Snapshot().height, 0);
  ASSERT_EQ(blocks->blocks.size(), 1U);
  EXPECT_EQ(blocks->blocks.front().header.source, "genesis");
}

TEST(ChainKernelTest, CommitPendingAdvancesTheLocalChain)
{
  auto blocks = std::make_shared<MemoryBlockStore>();
  auto snapshots = std::make_shared<MemorySnapshotStore>();
  auto kernel = Kernel(core::ChainConfig {}, blocks, snapshots);

  ASSERT_TRUE(kernel.Bootstrap().ok());
  ASSERT_TRUE(kernel.SubmitTransaction(
    core::Transaction::FromText("demo.tx", "payload")).ok());

  auto const commit_status = kernel.CommitPending("unit-test");

  ASSERT_TRUE(commit_status.ok());
  EXPECT_EQ(kernel.Snapshot().height, 1);
  EXPECT_EQ(kernel.Chain().size(), 2U);
  EXPECT_TRUE(kernel.PendingTransactions().empty());
}

} // namespace blocxxi::chain
