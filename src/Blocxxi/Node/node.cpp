//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Node/node.h>

#include <utility>

#include <Blocxxi/Chain/kernel.h>
#include <Blocxxi/Storage/file_store.h>
#include <Blocxxi/Storage/in_memory_store.h>

namespace blocxxi::node {

struct Node::Impl {
  explicit Impl(NodeOptions node_options)
    : options(std::move(node_options))
  {
    if (options.storage_mode == StorageMode::FileSystem) {
      auto root = options.storage_root;
      if (root.empty()) {
        root = options.chain.data_directory.empty() ? std::filesystem::path(".blocxxi")
                                                    : options.chain.data_directory;
      }
      block_store = storage::MakeFileBlockStore(root);
      snapshot_store = storage::MakeFileSnapshotStore(root);
    } else {
      block_store = storage::MakeInMemoryBlockStore();
      snapshot_store = storage::MakeInMemorySnapshotStore();
    }
    kernel = std::make_unique<chain::Kernel>(
      options.chain, block_store, snapshot_store);
  }

  void Emit(core::ChainEvent event)
  {
    for (auto const& plugin : plugins) {
      if (plugin) {
        plugin->OnEvent(event);
      }
    }
    for (auto const& handler : handlers) {
      if (handler) {
        handler(event);
      }
    }
  }

  [[nodiscard]] auto SnapshotNow() const -> core::ChainSnapshot
  {
    return kernel->Snapshot();
  }

  NodeOptions options {};
  bool running { false };
  std::shared_ptr<chain::BlockStore> block_store {};
  std::shared_ptr<chain::SnapshotStore> snapshot_store {};
  std::unique_ptr<chain::Kernel> kernel {};
  std::vector<core::EventHandler> handlers {};
  std::vector<std::shared_ptr<core::Plugin>> plugins {};
  std::vector<std::string> attached_services {};
};

Node::Node(NodeOptions options)
  : impl_(std::make_unique<Impl>(std::move(options)))
{
}

Node::~Node() = default;
Node::Node(Node&&) noexcept = default;
auto Node::operator=(Node&&) noexcept -> Node& = default;

auto Node::Start() -> core::Status
{
  if (impl_->running) {
    return core::Status::Success("node already running");
  }

  impl_->Emit(core::ChainEvent {
    .type = core::EventType::NodeStarting,
    .message = "starting node",
    .snapshot = impl_->SnapshotNow(),
  });

  if (auto status = impl_->kernel->Bootstrap(); !status.ok()) {
    return status;
  }

  impl_->running = true;
  if (impl_->options.start_discovery) {
    auto discovery_status = AttachDiscovery(impl_->options.discovery_name);
    if (!discovery_status.ok()) {
      return discovery_status;
    }
  }

  impl_->Emit(core::ChainEvent {
    .type = core::EventType::NodeStarted,
    .message = "node running",
    .snapshot = impl_->SnapshotNow(),
  });
  return core::Status::Success();
}

auto Node::Stop() -> core::Status
{
  if (!impl_->running) {
    return core::Status::Success("node already stopped");
  }
  impl_->running = false;
  impl_->Emit(core::ChainEvent {
    .type = core::EventType::NodeStopped,
    .message = "node stopped",
    .snapshot = impl_->SnapshotNow(),
  });
  return core::Status::Success();
}

auto Node::IsRunning() const -> bool
{
  return impl_->running;
}

auto Node::Options() const -> NodeOptions const&
{
  return impl_->options;
}

auto Node::Snapshot() const -> core::ChainSnapshot
{
  return impl_->SnapshotNow();
}

auto Node::Blocks() const -> std::vector<core::Block>
{
  return impl_->kernel->Chain();
}

auto Node::Subscribe(core::EventHandler handler) -> std::size_t
{
  impl_->handlers.push_back(std::move(handler));
  return impl_->handlers.size();
}

auto Node::RegisterPlugin(std::shared_ptr<core::Plugin> plugin) -> std::size_t
{
  impl_->plugins.push_back(std::move(plugin));
  return impl_->plugins.size();
}

auto Node::SubmitTransaction(core::Transaction transaction) -> core::Status
{
  if (!impl_->running) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "node must be started before submitting transactions");
  }
  auto status = impl_->kernel->SubmitTransaction(transaction);
  if (status.ok()) {
    impl_->Emit(core::ChainEvent {
      .type = core::EventType::TransactionAccepted,
      .message = "transaction accepted",
      .snapshot = impl_->SnapshotNow(),
      .transaction = std::move(transaction),
    });
  }
  return status;
}

auto Node::CommitPending(std::string source) -> core::Status
{
  if (!impl_->running) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "node must be started before committing blocks");
  }
  auto status = impl_->kernel->CommitPending(std::move(source));
  if (status.ok()) {
    auto const head = impl_->kernel->Head();
    impl_->Emit(core::ChainEvent {
      .type = core::EventType::BlockCommitted,
      .message = "pending transactions committed",
      .snapshot = impl_->SnapshotNow(),
      .block = head,
    });
  }
  return status;
}

auto Node::SubmitBlock(core::Block block) -> core::Status
{
  if (!impl_->running) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "node must be started before submitting blocks");
  }
  auto status = impl_->kernel->CommitBlock(block);
  if (status.ok()) {
    impl_->Emit(core::ChainEvent {
      .type = core::EventType::BlockCommitted,
      .message = "block committed",
      .snapshot = impl_->SnapshotNow(),
      .block = std::move(block),
    });
  }
  return status;
}

auto Node::AttachDiscovery(std::string service_name) -> core::Status
{
  if (service_name.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "discovery service name is required");
  }
  impl_->attached_services.push_back(service_name);
  impl_->Emit(core::ChainEvent {
    .type = core::EventType::DiscoveryAttached,
    .message = std::move(service_name),
    .snapshot = impl_->SnapshotNow(),
  });
  return core::Status::Success();
}

auto Node::AttachAdapter(std::string adapter_name) -> core::Status
{
  if (adapter_name.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "adapter name is required");
  }
  impl_->Emit(core::ChainEvent {
    .type = core::EventType::AdapterAttached,
    .message = std::move(adapter_name),
    .snapshot = impl_->SnapshotNow(),
  });
  return core::Status::Success();
}

} // namespace blocxxi::node
