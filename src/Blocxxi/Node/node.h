//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/Node/api_export.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <Blocxxi/Core/primitives.h>
#include <Blocxxi/Core/result.h>
#include <Blocxxi/Node/service.h>

namespace blocxxi::node {

enum class StorageMode {
  InMemory,
  FileSystem,
};

struct NodeOptions {
  core::ChainConfig chain {};
  StorageMode storage_mode { StorageMode::InMemory };
  std::filesystem::path storage_root {};
  bool start_discovery { false };
  std::string discovery_name { "blocxxi.p2p" };
};

class Node {
public:
  explicit BLOCXXI_NODE_API Node(NodeOptions options = {});
  BLOCXXI_NODE_API ~Node();

  BLOCXXI_NODE_API Node(Node&&) noexcept;
  BLOCXXI_NODE_API auto operator=(Node&&) noexcept -> Node&;

  Node(Node const&) = delete;
  auto operator=(Node const&) -> Node& = delete;

  BLOCXXI_NODE_API auto Start() -> core::Status;
  BLOCXXI_NODE_API auto Stop() -> core::Status;

  [[nodiscard]] BLOCXXI_NODE_API auto IsRunning() const -> bool;
  [[nodiscard]] BLOCXXI_NODE_API auto Options() const -> NodeOptions const&;
  [[nodiscard]] BLOCXXI_NODE_API auto Snapshot() const -> core::ChainSnapshot;
  [[nodiscard]] BLOCXXI_NODE_API auto Blocks() const -> std::vector<core::Block>;

  BLOCXXI_NODE_API auto Subscribe(core::EventHandler handler) -> std::size_t;
  BLOCXXI_NODE_API auto RegisterPlugin(std::shared_ptr<core::Plugin> plugin)
    -> std::size_t;
  BLOCXXI_NODE_API auto RegisterService(ServicePointer service) -> std::size_t;
  BLOCXXI_NODE_API auto SubmitTransaction(core::Transaction transaction)
    -> core::Status;
  BLOCXXI_NODE_API auto CommitPending(std::string source = "local")
    -> core::Status;
  BLOCXXI_NODE_API auto SubmitBlock(core::Block block) -> core::Status;
  BLOCXXI_NODE_API auto RunServicesOnce() -> core::Status;
  BLOCXXI_NODE_API auto RunServicesFor(std::chrono::milliseconds duration,
    std::chrono::milliseconds idle_sleep = std::chrono::milliseconds { 10 })
    -> core::Status;
  BLOCXXI_NODE_API auto RunServicesUntil(std::function<bool()> keep_running,
    std::chrono::milliseconds idle_sleep = std::chrono::milliseconds { 10 })
    -> core::Status;
  [[nodiscard]] BLOCXXI_NODE_API auto ServiceStates() const
    -> std::vector<ServiceState>;
  BLOCXXI_NODE_API auto AttachDiscovery(std::string service_name)
    -> core::Status;
  BLOCXXI_NODE_API auto AttachAdapter(std::string adapter_name)
    -> core::Status;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace blocxxi::node
