//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/Core/api_export.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Blocxxi/Crypto/hash.h>

namespace blocxxi::core {

using ByteVector = std::vector<std::uint8_t>;
using BlockId = blocxxi::crypto::Hash256;
using TransactionId = blocxxi::crypto::Hash256;
using Height = std::uint64_t;

struct ChainConfig {
  std::string chain_id { "blocxxi.local" };
  std::string display_name { "Blocxxi Local Chain" };
  std::filesystem::path data_directory {};
  bool create_genesis { true };
  std::string genesis_payload { "blocxxi:genesis" };
};

struct Transaction {
  TransactionId id {};
  std::string type { "message" };
  ByteVector payload {};
  std::string metadata {};

  static BLOCXXI_CORE_API auto FromText(
    std::string type, std::string payload, std::string metadata = {})
    -> Transaction;

  [[nodiscard]] BLOCXXI_CORE_API auto PayloadText() const -> std::string;

  friend auto operator==(Transaction const& lhs, Transaction const& rhs)
    -> bool = default;
};

struct BlockHeader {
  BlockId id {};
  BlockId previous_id {};
  Height height { 0 };
  std::int64_t timestamp_utc { 0 };
  std::string source { "local" };

  friend auto operator==(BlockHeader const& lhs, BlockHeader const& rhs)
    -> bool = default;
};

struct Block {
  BlockHeader header {};
  std::vector<Transaction> transactions {};

  static BLOCXXI_CORE_API auto MakeNext(
    BlockId previous_id,
    Height height,
    std::vector<Transaction> transactions,
    std::string source = "local") -> Block;

  friend auto operator==(Block const& lhs, Block const& rhs) -> bool = default;
};

struct ChainSnapshot {
  Height height { 0 };
  BlockId head_id {};
  std::size_t block_count { 0 };
  std::size_t accepted_transactions { 0 };
  bool bootstrapped { false };

  friend auto operator==(ChainSnapshot const& lhs, ChainSnapshot const& rhs)
    -> bool = default;
};

enum class EventType {
  NodeStarting,
  NodeStarted,
  NodeStopped,
  TransactionAccepted,
  BlockCommitted,
  DiscoveryAttached,
  AdapterAttached,
  ServiceRegistered,
  ServiceStarted,
  ServiceCompleted,
  ServiceFailed,
};

struct ChainEvent {
  EventType type { EventType::NodeStarted };
  std::string message {};
  ChainSnapshot snapshot {};
  std::optional<Transaction> transaction {};
  std::optional<Block> block {};
};

class Plugin {
public:
  virtual ~Plugin() = default;
  virtual void OnEvent(ChainEvent const& event) = 0;
};

using EventHandler = std::function<void(ChainEvent const& event)>;

BLOCXXI_CORE_NDAPI auto MakeId(std::string_view seed) -> blocxxi::crypto::Hash256;
BLOCXXI_CORE_NDAPI auto ToBytes(std::string_view text) -> ByteVector;
BLOCXXI_CORE_NDAPI auto ToString(ByteVector const& bytes) -> std::string;
BLOCXXI_CORE_NDAPI auto NowUnixSeconds() -> std::int64_t;

} // namespace blocxxi::core
