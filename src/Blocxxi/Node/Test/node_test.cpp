//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>

#include <Blocxxi/Node/node.h>

namespace blocxxi::node {
namespace {

class RecordingPlugin final : public core::Plugin {
public:
  void OnEvent(core::ChainEvent const& event) override
  {
    events.push_back(event);
  }

  std::vector<core::ChainEvent> events {};
};

class CountingService final : public Service {
public:
  explicit CountingService(std::string checkpoint = {})
    : checkpoint_(std::move(checkpoint))
  {
  }

  [[nodiscard]] auto Name() const -> std::string override { return "counting.service"; }

  [[nodiscard]] auto Policy() const -> ServicePolicy override
  {
    return ServicePolicy {
      .interval = std::chrono::milliseconds { 1 },
      .retry_backoff = std::chrono::milliseconds { 1 },
      .max_retries = 2,
    };
  }

  auto Poll(Node& node) -> core::Status override
  {
    auto const next_value = checkpoint_.empty() ? 1 : std::stoi(checkpoint_) + 1;
    checkpoint_ = std::to_string(next_value);
    return node.SubmitTransaction(core::Transaction::FromText(
      "service.tick", checkpoint_, "service=counting.service"));
  }

  [[nodiscard]] auto Checkpoint() const -> std::string override { return checkpoint_; }

  auto RestoreCheckpoint(std::string_view checkpoint) -> core::Status override
  {
    checkpoint_ = std::string(checkpoint);
    return core::Status::Success();
  }

private:
  std::string checkpoint_ {};
};

class FlakyService final : public Service {
public:
  [[nodiscard]] auto Name() const -> std::string override { return "flaky.service"; }

  [[nodiscard]] auto Policy() const -> ServicePolicy override
  {
    return ServicePolicy {
      .interval = std::chrono::milliseconds { 1 },
      .retry_backoff = std::chrono::milliseconds { 1 },
      .max_retries = 4,
    };
  }

  auto Poll(Node&) -> core::Status override
  {
    if (!failed_once_) {
      failed_once_ = true;
      return core::Status::Failure(core::StatusCode::IOError, "temporary failure");
    }
    return core::Status::Success();
  }

private:
  bool failed_once_ { false };
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
  EXPECT_EQ(plugin->events.front().type, core::EventType::NodeStarting);
  EXPECT_EQ(plugin->events[1].type, core::EventType::NodeStarted);
  EXPECT_EQ(plugin->events[2].type, core::EventType::TransactionAccepted);
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

TEST(NodeTest, PublicApiSubscribersReceiveCommittedPayloadDetails)
{
  auto node = Node();
  auto plugin = std::make_shared<RecordingPlugin>();
  auto observed = std::vector<core::ChainEvent> {};
  node.RegisterPlugin(plugin);
  node.Subscribe([&](core::ChainEvent const& event) { observed.push_back(event); });

  ASSERT_TRUE(node.Start().ok());
  ASSERT_TRUE(node.SubmitTransaction(
    core::Transaction::FromText("demo.asset", "mint:42", "issuer=test")).ok());
  ASSERT_TRUE(node.CommitPending("plugin-proof").ok());

  auto const committed = std::find_if(observed.begin(), observed.end(),
    [](core::ChainEvent const& event) {
      return event.type == core::EventType::BlockCommitted && event.block.has_value();
    });
  ASSERT_NE(committed, observed.end());
  ASSERT_TRUE(committed->block.has_value());
  ASSERT_EQ(committed->block->transactions.size(), 1U);
  EXPECT_EQ(committed->block->transactions.front().PayloadText(), "mint:42");
  EXPECT_EQ(committed->block->transactions.front().metadata, "issuer=test");
  EXPECT_EQ(committed->message, "pending transactions committed");

  auto const plugin_block = std::find_if(plugin->events.begin(), plugin->events.end(),
    [](core::ChainEvent const& event) {
      return event.type == core::EventType::BlockCommitted && event.block.has_value();
    });
  ASSERT_NE(plugin_block, plugin->events.end());
  EXPECT_EQ(plugin_block->block->transactions.front().type, "demo.asset");
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

TEST(NodeTest, ServicesRunThroughNodeFacadeAndPersistCheckpoint)
{
  auto const root = std::filesystem::temp_directory_path() / "blocxxi-node-service-test";
  std::filesystem::remove_all(root);

  auto options = NodeOptions {};
  options.storage_mode = StorageMode::FileSystem;
  options.storage_root = root;
  options.chain.chain_id = "demo.services";

  {
    auto node = Node(options);
    auto service = std::make_shared<CountingService>();
    node.RegisterService(service);
    ASSERT_TRUE(node.Start().ok());
    ASSERT_TRUE(node.RunServicesOnce().ok());
    ASSERT_EQ(node.ServiceStates().size(), 1U);
    EXPECT_EQ(node.ServiceStates().front().run_count, 1U);
    ASSERT_TRUE(node.CommitPending("service-proof").ok());
    ASSERT_TRUE(node.Stop().ok());
  }

  {
    auto node = Node(options);
    auto service = std::make_shared<CountingService>();
    node.RegisterService(service);
    ASSERT_TRUE(node.Start().ok());
    ASSERT_TRUE(node.RunServicesOnce().ok());
    ASSERT_EQ(node.ServiceStates().size(), 1U);
    EXPECT_EQ(node.ServiceStates().front().checkpoint, "2");
    ASSERT_TRUE(node.Stop().ok());
  }

  std::filesystem::remove_all(root);
}

TEST(NodeTest, ServiceFailuresUseRetryStateInsteadOfAnalyzerLoops)
{
  auto node = Node();
  auto service = std::make_shared<FlakyService>();
  node.RegisterService(service);

  ASSERT_TRUE(node.Start().ok());
  auto const first = node.RunServicesOnce();
  EXPECT_TRUE(first.ok());
  ASSERT_EQ(node.ServiceStates().size(), 1U);
  EXPECT_EQ(node.ServiceStates().front().failure_count, 1U);
  EXPECT_FALSE(node.ServiceStates().front().last_error.empty());

  std::this_thread::sleep_for(std::chrono::milliseconds { 2 });
  auto const second = node.RunServicesOnce();
  EXPECT_TRUE(second.ok());
  EXPECT_EQ(node.ServiceStates().front().run_count, 1U);
  EXPECT_EQ(node.ServiceStates().front().failure_count, 0U);
}

} // namespace blocxxi::node
