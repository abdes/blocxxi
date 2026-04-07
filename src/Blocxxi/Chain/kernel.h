//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/Chain/api_export.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <Blocxxi/Core/primitives.h>
#include <Blocxxi/Core/result.h>

namespace blocxxi::chain {

class BlockStore {
public:
  virtual ~BlockStore() = default;
  virtual auto PutBlock(core::Block const& block) -> core::Status = 0;
  [[nodiscard]] virtual auto GetBlock(core::BlockId const& id) const
    -> std::optional<core::Block> = 0;
  [[nodiscard]] virtual auto GetChain() const -> std::vector<core::Block> = 0;
};

class SnapshotStore {
public:
  virtual ~SnapshotStore() = default;
  virtual auto Save(core::ChainSnapshot const& snapshot) -> core::Status = 0;
  [[nodiscard]] virtual auto Load() const -> std::optional<core::ChainSnapshot>
    = 0;
};

class BlockValidator {
public:
  virtual ~BlockValidator() = default;
  [[nodiscard]] virtual auto Validate(core::Block const& block,
    core::ChainSnapshot const& snapshot) const -> core::Status = 0;
};

class BasicBlockValidator final : public BlockValidator {
public:
  [[nodiscard]] BLOCXXI_CHAIN_API auto Validate(core::Block const& block,
    core::ChainSnapshot const& snapshot) const -> core::Status override;
};

class Kernel {
public:
  BLOCXXI_CHAIN_API Kernel(core::ChainConfig config,
    std::shared_ptr<BlockStore> block_store,
    std::shared_ptr<SnapshotStore> snapshot_store,
    std::shared_ptr<BlockValidator> validator = nullptr);

  [[nodiscard]] auto Config() const -> core::ChainConfig const&
  {
    return config_;
  }

  BLOCXXI_CHAIN_API auto Bootstrap() -> core::Status;
  BLOCXXI_CHAIN_API auto SubmitTransaction(core::Transaction transaction)
    -> core::Status;
  BLOCXXI_CHAIN_API auto CommitBlock(core::Block block) -> core::Status;
  BLOCXXI_CHAIN_API auto CommitPending(std::string source = "local")
    -> core::Status;

  [[nodiscard]] auto Snapshot() const -> core::ChainSnapshot const&
  {
    return snapshot_;
  }

  [[nodiscard]] auto PendingTransactions() const
    -> std::vector<core::Transaction>
  {
    return pending_transactions_;
  }

  [[nodiscard]] BLOCXXI_CHAIN_API auto Head() const
    -> std::optional<core::Block>;
  [[nodiscard]] BLOCXXI_CHAIN_API auto Chain() const
    -> std::vector<core::Block>;

private:
  core::ChainConfig config_;
  std::shared_ptr<BlockStore> block_store_;
  std::shared_ptr<SnapshotStore> snapshot_store_;
  std::shared_ptr<BlockValidator> validator_;
  core::ChainSnapshot snapshot_ {};
  std::vector<core::Transaction> pending_transactions_ {};
};

} // namespace blocxxi::chain
