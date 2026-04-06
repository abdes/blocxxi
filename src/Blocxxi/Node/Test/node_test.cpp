//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

#include <Blocxxi/Node/node.h>

namespace blocxxi::node {
namespace {

class RecordingPlugin final : public core::Plugin {
public:
  void OnEvent(core::ChainEvent const& event) override
  {
    events.push_back(event.type);
  }

  std::vector<core::EventType> events {};
};

} // namespace

TEST(NodeTest, NoDhtNodeCanStartAcceptTransactionsAndCommit)
{
  auto node = Node();
  auto plugin = std::make_shared<RecordingPlugin>();
  node.RegisterPlugin(plugin);

  ASSERT_TRUE(node.Start().ok());
  ASSERT_TRUE(node.SubmitTransaction(
    core::Transaction::FromText("demo.tx", "payload")).ok());
  ASSERT_TRUE(node.CommitPending("node-test").ok());

  EXPECT_TRUE(node.IsRunning());
  EXPECT_EQ(node.Snapshot().height, 1);
  EXPECT_EQ(node.Blocks().size(), 2U);
  EXPECT_GE(plugin->events.size(), 3U);
}

TEST(NodeTest, DiscoveryAttachmentIsOptionalAndExplicit)
{
  auto options = NodeOptions {};
  options.start_discovery = true;
  options.discovery_name = "blocxxi.p2p.mock";
  auto node = Node(options);

  ASSERT_TRUE(node.Start().ok());
  EXPECT_TRUE(node.IsRunning());
  EXPECT_EQ(node.Snapshot().height, 0);
}

TEST(NodeTest, FileSystemNodeRestartsFromPersistedSnapshotWithoutDht)
{
  auto const root
    = std::filesystem::temp_directory_path() / "blocxxi-node-restart-test";
  std::filesystem::remove_all(root);

  auto options = NodeOptions {};
  options.storage_mode = StorageMode::FileSystem;
  options.storage_root = root;
  options.chain.chain_id = "demo.restart";
  options.chain.display_name = "Restart Proof Chain";

  {
    auto node = Node(options);
    ASSERT_TRUE(node.Start().ok());
    ASSERT_TRUE(node.SubmitTransaction(
      core::Transaction::FromText("demo.tx", "first")).ok());
    ASSERT_TRUE(node.CommitPending("restart-proof").ok());
    ASSERT_TRUE(node.Stop().ok());
  }

  {
    auto node = Node(options);
    ASSERT_TRUE(node.Start().ok());
    EXPECT_EQ(node.Snapshot().height, 1);
    EXPECT_EQ(node.Snapshot().block_count, 2U);
    ASSERT_EQ(node.Blocks().size(), 2U);
    EXPECT_EQ(node.Blocks().back().transactions.front().PayloadText(), "first");

    ASSERT_TRUE(node.SubmitTransaction(
      core::Transaction::FromText("demo.tx", "second")).ok());
    ASSERT_TRUE(node.CommitPending("restart-proof").ok());
    EXPECT_EQ(node.Snapshot().height, 2);
    ASSERT_EQ(node.Blocks().size(), 3U);
    ASSERT_TRUE(node.Stop().ok());
  }

  std::filesystem::remove_all(root);
}

} // namespace blocxxi::node
