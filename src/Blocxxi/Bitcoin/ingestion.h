//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/Bitcoin/api_export.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Blocxxi/Bitcoin/adapter.h>
#include <Blocxxi/Core/result.h>

namespace blocxxi::bitcoin {

struct RpcConnectionConfig {
  std::string host { "127.0.0.1" };
  std::uint16_t port { 8332 };
  std::string username {};
  std::string password {};
  std::string path { "/" };
};

struct BitcoinCoreRpcConfig {
  Network network { Network::Mainnet };
  RpcConnectionConfig connection {};
  std::size_t max_mempool_transactions { 64 };
};

struct ResolvedBitcoinCoreRpcConfig {
  BitcoinCoreRpcConfig config {};
  std::filesystem::path config_path {};
  bool config_file_loaded { false };
  std::string host_source { "default" };
  std::string port_source { "default" };
  std::string username_source { "unset" };
  std::string password_source { "unset" };
  std::string path_source { "default" };
};

struct TransactionObservation {
  std::string txid {};
  double base_fee_btc { 0.0 };
  std::uint64_t vsize { 0 };
  std::uint64_t weight { 0 };
};

struct BlockObservation {
  std::uint64_t height { 0 };
  std::string block_hash_hex {};
  std::uint64_t transaction_count { 0 };
  std::uint64_t total_fee_satoshis { 0 };
};

struct NetworkHealthObservation {
  std::uint64_t connection_count { 0 };
  std::uint64_t mempool_transaction_count { 0 };
  std::uint64_t mempool_bytes { 0 };
  double relay_fee_btc { 0.0 };
  bool initial_block_download { false };
  std::string warning {};
};

struct ObservationBatch {
  std::string source_name {};
  Network network { Network::Mainnet };
  std::int64_t observed_at_utc { 0 };
  std::vector<TransactionObservation> mempool_transactions {};
  std::optional<BlockObservation> latest_block {};
  std::optional<NetworkHealthObservation> network_health {};
};

class BLOCXXI_BITCOIN_API RpcTransport {
public:
  virtual ~RpcTransport();

  virtual auto Call(std::string_view method, std::string_view params_json,
    std::string& response_json) -> core::Status
    = 0;
};

class BLOCXXI_BITCOIN_API DataSourceAdapter {
public:
  virtual ~DataSourceAdapter();

  [[nodiscard]] virtual auto Name() const -> std::string = 0;
  virtual auto Poll(ObservationBatch& batch) -> core::Status = 0;
};

class BLOCXXI_BITCOIN_API HttpRpcTransport final : public RpcTransport {
public:
  explicit BLOCXXI_BITCOIN_API HttpRpcTransport(RpcConnectionConfig config);
  BLOCXXI_BITCOIN_API ~HttpRpcTransport() override;

  BLOCXXI_BITCOIN_API auto Call(std::string_view method,
    std::string_view params_json, std::string& response_json) -> core::Status override;

private:
  RpcConnectionConfig config_ {};
};

class BLOCXXI_BITCOIN_API BitcoinCoreRpcAdapter final : public DataSourceAdapter {
public:
  explicit BLOCXXI_BITCOIN_API BitcoinCoreRpcAdapter(
    BitcoinCoreRpcConfig config,
    std::shared_ptr<RpcTransport> transport = nullptr);
  BLOCXXI_BITCOIN_API ~BitcoinCoreRpcAdapter() override;

  [[nodiscard]] BLOCXXI_BITCOIN_API auto Name() const -> std::string override;
  BLOCXXI_BITCOIN_API auto Poll(ObservationBatch& batch) -> core::Status override;

private:
  BitcoinCoreRpcConfig config_ {};
  std::shared_ptr<RpcTransport> transport_ {};
};

[[nodiscard]] BLOCXXI_BITCOIN_API auto DefaultBitcoinCoreConfigPath(
  Network network = Network::Mainnet) -> std::filesystem::path;

[[nodiscard]] BLOCXXI_BITCOIN_API auto ResolveBitcoinCoreRpcConfig(
  BitcoinCoreRpcConfig base = {},
  std::optional<std::filesystem::path> config_path = std::nullopt)
  -> BitcoinCoreRpcConfig;

[[nodiscard]] BLOCXXI_BITCOIN_API auto ResolveBitcoinCoreRpcConfigDetails(
  BitcoinCoreRpcConfig base = {},
  std::optional<std::filesystem::path> config_path = std::nullopt)
  -> ResolvedBitcoinCoreRpcConfig;

} // namespace blocxxi::bitcoin
