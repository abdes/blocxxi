//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Storage/in_memory_store.h>

namespace blocxxi::storage {
namespace {

class InMemoryBlockStore final : public chain::BlockStore {
public:
  auto PutBlock(core::Block const& block) -> core::Status override
  {
    auto const key = block.header.id.ToHex();
    if (blocks_.contains(key)) {
      return core::Status::Failure(
        core::StatusCode::Duplicate, "block already exists in memory store");
    }
    order_.push_back(key);
    blocks_.emplace(key, block);
    return core::Status::Success();
  }

  [[nodiscard]] auto GetBlock(core::BlockId const& id) const
    -> std::optional<core::Block> override
  {
    auto const found = blocks_.find(id.ToHex());
    if (found == blocks_.end()) {
      return std::nullopt;
    }
    return found->second;
  }

  [[nodiscard]] auto GetChain() const -> std::vector<core::Block> override
  {
    auto chain = std::vector<core::Block> {};
    chain.reserve(order_.size());
    for (auto const& key : order_) {
      chain.push_back(blocks_.at(key));
    }
    return chain;
  }

private:
  std::map<std::string, core::Block> blocks_ {};
  std::vector<std::string> order_ {};
};

class InMemorySnapshotStore final : public chain::SnapshotStore {
public:
  auto Save(core::ChainSnapshot const& snapshot) -> core::Status override
  {
    snapshot_ = snapshot;
    return core::Status::Success();
  }

  [[nodiscard]] auto Load() const -> std::optional<core::ChainSnapshot> override
  {
    return snapshot_;
  }

private:
  std::optional<core::ChainSnapshot> snapshot_ {};
};

} // namespace

auto MakeInMemoryBlockStore() -> std::shared_ptr<chain::BlockStore>
{
  return std::make_shared<InMemoryBlockStore>();
}

auto MakeInMemorySnapshotStore() -> std::shared_ptr<chain::SnapshotStore>
{
  return std::make_shared<InMemorySnapshotStore>();
}

} // namespace blocxxi::storage
