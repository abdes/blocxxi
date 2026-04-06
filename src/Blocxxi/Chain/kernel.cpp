//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Chain/kernel.h>

#include <algorithm>

namespace blocxxi::chain {

auto BasicBlockValidator::Validate(core::Block const& block,
  core::ChainSnapshot const& snapshot) const -> core::Status
{
  if (block.header.source.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "block source is required");
  }

  if (snapshot.bootstrapped && block.transactions.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "non-genesis blocks require transactions");
  }

  if (!snapshot.bootstrapped) {
    if (block.header.height != 0) {
      return core::Status::Failure(
        core::StatusCode::Rejected, "genesis block must start at height 0");
    }
    if (!(block.header.previous_id == core::BlockId {})) {
      return core::Status::Failure(
        core::StatusCode::Rejected, "genesis block must use an empty previous id");
    }
    return core::Status::Success();
  }

  if (block.header.height != snapshot.height + 1) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "block height does not extend the active head");
  }

  if (!(block.header.previous_id == snapshot.head_id)) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "block does not link to the active head");
  }

  return core::Status::Success();
}

Kernel::Kernel(core::ChainConfig config,
  std::shared_ptr<BlockStore> block_store,
  std::shared_ptr<SnapshotStore> snapshot_store,
  std::shared_ptr<BlockValidator> validator)
  : config_(std::move(config))
  , block_store_(std::move(block_store))
  , snapshot_store_(std::move(snapshot_store))
  , validator_(std::move(validator))
{
  if (!validator_) {
    validator_ = std::make_shared<BasicBlockValidator>();
  }
}

auto Kernel::Bootstrap() -> core::Status
{
  if (auto const snapshot = snapshot_store_->Load()) {
    snapshot_ = *snapshot;
    return core::Status::Success("loaded existing chain snapshot");
  }

  if (!config_.create_genesis) {
    return core::Status::Success("bootstrap skipped without genesis creation");
  }

  auto genesis = core::Block::MakeNext(core::BlockId {}, 0,
    { core::Transaction::FromText(
      "genesis", config_.genesis_payload, config_.display_name) },
    "genesis");
  return CommitBlock(std::move(genesis));
}

auto Kernel::SubmitTransaction(core::Transaction transaction) -> core::Status
{
  if (transaction.type.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "transaction type is required");
  }
  if (transaction.payload.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "transaction payload is required");
  }

  auto const duplicate = std::ranges::find_if(pending_transactions_,
    [&](auto const& pending) { return pending.id == transaction.id; });
  if (duplicate != pending_transactions_.end()) {
    return core::Status::Failure(
      core::StatusCode::Duplicate, "transaction already pending");
  }

  pending_transactions_.push_back(std::move(transaction));
  return core::Status::Success();
}

auto Kernel::CommitBlock(core::Block block) -> core::Status
{
  if (auto status = validator_->Validate(block, snapshot_); !status.ok()) {
    return status;
  }
  if (auto status = block_store_->PutBlock(block); !status.ok()) {
    return status;
  }

  snapshot_.height = block.header.height;
  snapshot_.head_id = block.header.id;
  snapshot_.block_count += 1;
  snapshot_.accepted_transactions += block.transactions.size();
  snapshot_.bootstrapped = true;

  pending_transactions_.erase(
    std::remove_if(pending_transactions_.begin(), pending_transactions_.end(),
      [&](auto const& pending) {
        return std::ranges::any_of(block.transactions,
          [&](auto const& committed) { return committed.id == pending.id; });
      }),
    pending_transactions_.end());

  if (auto status = snapshot_store_->Save(snapshot_); !status.ok()) {
    return status;
  }
  return core::Status::Success();
}

auto Kernel::CommitPending(std::string source) -> core::Status
{
  if (pending_transactions_.empty()) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "no pending transactions to commit");
  }

  auto block = core::Block::MakeNext(
    snapshot_.head_id, snapshot_.bootstrapped ? snapshot_.height + 1 : 0,
    pending_transactions_, std::move(source));
  return CommitBlock(std::move(block));
}

auto Kernel::Head() const -> std::optional<core::Block>
{
  if (!snapshot_.bootstrapped) {
    return std::nullopt;
  }
  return block_store_->GetBlock(snapshot_.head_id);
}

auto Kernel::Chain() const -> std::vector<core::Block>
{
  return block_store_->GetChain();
}

} // namespace blocxxi::chain
