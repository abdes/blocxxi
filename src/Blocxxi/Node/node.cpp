//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Node/node.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <thread>
#include <utility>

#include <Blocxxi/Chain/kernel.h>
#include <Blocxxi/Storage/file_store.h>
#include <Blocxxi/Storage/in_memory_store.h>

namespace blocxxi::node {
namespace {

template <typename ShouldContinue>
auto RunServiceLoop(
  Node& node, ShouldContinue&& should_continue, std::chrono::milliseconds idle_sleep)
  -> core::Status
{
  auto overall = core::Status::Success();
  while (std::forward<ShouldContinue>(should_continue)()) {
    auto const status = node.RunServicesOnce();
    if (!status.ok()) {
      overall = status;
    }
    if (idle_sleep.count() > 0) {
      std::this_thread::sleep_for(idle_sleep);
    }
  }
  return overall;
}

struct ManagedService {
  ServicePointer service {};
  ServiceState state {};
  std::chrono::steady_clock::time_point next_due {
    std::chrono::steady_clock::now()
  };
};

[[nodiscard]] auto ResolveServiceRoot(NodeOptions const& options) -> std::filesystem::path
{
  auto root = options.storage_root;
  if (root.empty()) {
    root = options.chain.data_directory.empty() ? std::filesystem::path(".blocxxi")
                                                : options.chain.data_directory;
  }
  return root / "services";
}

[[nodiscard]] auto SanitizeServiceName(std::string name) -> std::string
{
  for (auto& ch : name) {
    auto const safe = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
      || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
    if (!safe) {
      ch = '-';
    }
  }
  return name;
}

[[nodiscard]] auto CheckpointPath(
  NodeOptions const& options, std::string const& service_name) -> std::filesystem::path
{
  return ResolveServiceRoot(options) / (SanitizeServiceName(service_name) + ".checkpoint");
}

auto PersistCheckpoint(NodeOptions const& options, ManagedService const& entry) -> void
{
  if (options.storage_mode != StorageMode::FileSystem) {
    return;
  }

  std::filesystem::create_directories(ResolveServiceRoot(options));
  auto checkpoint = std::ofstream(CheckpointPath(options, entry.state.name));
  checkpoint << entry.state.checkpoint;
}

auto RestoreCheckpoint(NodeOptions const& options, ManagedService& entry) -> void
{
  if (options.storage_mode != StorageMode::FileSystem) {
    return;
  }

  auto checkpoint = std::ifstream(CheckpointPath(options, entry.state.name));
  if (!checkpoint.good()) {
    return;
  }

  auto data = std::string(
    std::istreambuf_iterator<char>(checkpoint), std::istreambuf_iterator<char>());
  entry.state.checkpoint = data;
  auto const restore = entry.service->RestoreCheckpoint(data);
  if (!restore.ok()) {
    entry.state.last_error = restore.message;
  }
}

template <typename SnapshotFn, typename EmitFn>
auto RunManagedServiceOnce(Node& node, ManagedService& entry,
  std::chrono::steady_clock::time_point now, NodeOptions const& options,
  SnapshotFn&& snapshot_now, EmitFn&& emit) -> core::Status
{
  if (!entry.service) {
    return core::Status::Success();
  }

  if (!entry.state.started) {
    auto const status = entry.service->Start(node);
    if (!status.ok()) {
      entry.state.last_error = status.message;
      emit(core::ChainEvent {
        .type = core::EventType::ServiceFailed,
        .message = entry.state.name + ": " + status.message,
        .snapshot = snapshot_now(),
      });
      return status;
    }
    entry.state.started = true;
    emit(core::ChainEvent {
      .type = core::EventType::ServiceStarted,
      .message = entry.state.name,
      .snapshot = snapshot_now(),
    });
  }

  if (now < entry.next_due) {
    return core::Status::Success();
  }

  auto const status = entry.service->Poll(node);
  auto const policy = entry.service->Policy();
  if (status.ok()) {
    entry.state.run_count += 1U;
    entry.state.failure_count = 0U;
    entry.state.last_success_utc = core::NowUnixSeconds();
    entry.state.last_error.clear();
    entry.state.checkpoint = entry.service->Checkpoint();
    entry.state.next_run_utc = entry.state.last_success_utc
      + std::chrono::duration_cast<std::chrono::seconds>(policy.interval).count();
    entry.next_due = now + policy.interval;
    PersistCheckpoint(options, entry);
    emit(core::ChainEvent {
      .type = core::EventType::ServiceCompleted,
      .message = entry.state.name,
      .snapshot = snapshot_now(),
    });
    return core::Status::Success();
  }

  entry.state.failure_count += 1U;
  entry.state.last_error = status.message;
  auto const backoff = policy.retry_backoff * entry.state.failure_count;
  entry.state.next_run_utc = core::NowUnixSeconds()
    + std::chrono::duration_cast<std::chrono::seconds>(backoff).count();
  entry.next_due = now + backoff;
  emit(core::ChainEvent {
    .type = core::EventType::ServiceFailed,
    .message = entry.state.name + ": " + status.message,
    .snapshot = snapshot_now(),
  });

  if (entry.state.failure_count > policy.max_retries) {
    return status;
  }
  return core::Status::Success();
}

} // namespace

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
  std::vector<ManagedService> services {};
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

  for (auto& entry : impl_->services) {
    if (!entry.service || !entry.state.started) {
      continue;
    }

    auto const status = entry.service->Stop(*this);
    if (!status.ok()) {
      impl_->Emit(core::ChainEvent {
        .type = core::EventType::ServiceFailed,
        .message = entry.state.name + ": " + status.message,
        .snapshot = impl_->SnapshotNow(),
      });
    }
    entry.state.started = false;
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

auto Node::RegisterService(ServicePointer service) -> std::size_t
{
  if (!service) {
    return impl_->services.size();
  }

  auto entry = ManagedService {};
  entry.state.name = service->Name();
  entry.state.next_run_utc = core::NowUnixSeconds();
  entry.service = std::move(service);
  RestoreCheckpoint(impl_->options, entry);
  impl_->services.push_back(std::move(entry));
  impl_->Emit(core::ChainEvent {
    .type = core::EventType::ServiceRegistered,
    .message = impl_->services.back().state.name,
    .snapshot = impl_->SnapshotNow(),
  });
  return impl_->services.size();
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

auto Node::RunServicesOnce() -> core::Status
{
  if (!impl_->running) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "node must be started before running services");
  }

  auto overall = core::Status::Success();
  auto const now = std::chrono::steady_clock::now();
  for (auto& entry : impl_->services) {
    auto const status = RunManagedServiceOnce(*this, entry, now, impl_->options,
      [this]() { return impl_->SnapshotNow(); },
      [this](core::ChainEvent event) { impl_->Emit(std::move(event)); });
    if (!status.ok()) {
      overall = status;
    }
  }

  return overall;
}

auto Node::RunServicesFor(
  std::chrono::milliseconds duration, std::chrono::milliseconds idle_sleep)
  -> core::Status
{
  auto const deadline = std::chrono::steady_clock::now() + duration;
  return RunServiceLoop(*this,
    [deadline]() { return std::chrono::steady_clock::now() < deadline; },
    idle_sleep);
}

auto Node::RunServicesUntil(
  std::function<bool()> keep_running, std::chrono::milliseconds idle_sleep)
  -> core::Status
{
  return RunServiceLoop(*this, [&keep_running]() { return keep_running(); },
    idle_sleep);
}

auto Node::ServiceStates() const -> std::vector<ServiceState>
{
  auto states = std::vector<ServiceState> {};
  states.reserve(impl_->services.size());
  for (auto const& entry : impl_->services) {
    states.push_back(entry.state);
  }
  return states;
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
