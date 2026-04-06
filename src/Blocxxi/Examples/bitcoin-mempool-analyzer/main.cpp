//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include <Blocxxi/Bitcoin/ingestion.h>
#include <Blocxxi/Core/event_record.h>
#include <Blocxxi/Crypto/keypair.h>
#include <Blocxxi/Node/node.h>
#include <Blocxxi/P2P/event_dht.h>

namespace {

class ScriptedTransport final : public blocxxi::bitcoin::RpcTransport {
public:
  auto Call(std::string_view method, std::string_view, std::string& response_json)
    -> blocxxi::core::Status override
  {
    auto const found = responses_.find(std::string(method));
    if (found == responses_.end()) {
      return blocxxi::core::Status::Failure(
        blocxxi::core::StatusCode::NotFound, "missing rpc response");
    }
    response_json = found->second;
    return blocxxi::core::Status::Success();
  }

private:
  std::map<std::string, std::string> responses_ {
    { "getblockcount", R"({"result":321})" },
    { "getbestblockhash", R"({"result":"000000feed"})" },
    { "getmempoolinfo", R"({"result":{"size":4,"bytes":2048}})" },
    { "getnetworkinfo",
      R"({"result":{"connections":12,"relayfee":0.00001000,"warnings":"steady"}})" },
    { "getblockchaininfo", R"({"result":{"initialblockdownload":false}})" },
    { "getrawmempool",
      R"({"result":{"tx-large":{"vsize":400,"weight":1600,"fees":{"base":0.00045000}},"tx-mid":{"vsize":180,"weight":720,"fees":{"base":0.00018000}}}})" },
    { "getblockstats", R"({"result":{"txs":24,"totalfee":0.00320000}})" },
  };
};

class RulesOnlyAnalyzerService final : public blocxxi::node::Service {
public:
  RulesOnlyAnalyzerService(blocxxi::bitcoin::BitcoinCoreRpcAdapter adapter,
    blocxxi::p2p::MemoryEventDht& dht, blocxxi::crypto::KeyPair key_pair)
    : adapter_(std::move(adapter))
    , dht_(dht)
    , key_pair_(std::move(key_pair))
  {
  }

  [[nodiscard]] auto Name() const -> std::string override
  {
    return "bitcoin.rules-only-analyzer";
  }

  [[nodiscard]] auto Policy() const -> blocxxi::node::ServicePolicy override
  {
    return {
      .interval = std::chrono::milliseconds { 1 },
      .retry_backoff = std::chrono::milliseconds { 1 },
      .max_retries = 1,
    };
  }

  auto Start(blocxxi::node::Node& node) -> blocxxi::core::Status override
  {
    return node.AttachAdapter(adapter_.Name());
  }

  auto Poll(blocxxi::node::Node&) -> blocxxi::core::Status override
  {
    auto batch = blocxxi::bitcoin::ObservationBatch {};
    auto const status = adapter_.Poll(batch);
    if (!status.ok()) {
      return status;
    }
    if (!batch.latest_block.has_value()) {
      return blocxxi::core::Status::Failure(
        blocxxi::core::StatusCode::Rejected, "missing latest block observation");
    }

    for (auto const& observation : batch.mempool_transactions) {
      if (observation.base_fee_btc < 0.0003) {
        continue;
      }
      auto record = blocxxi::core::SignEventRecord(
        blocxxi::core::EventEnvelope {
          .event_type = "bitcoin.transaction.high-fee",
          .taxonomy = "transaction",
          .source = batch.source_name,
          .producer = Name(),
          .window = {
            .start_utc = batch.observed_at_utc,
            .end_utc = batch.observed_at_utc,
          },
          .observed_at_utc = batch.observed_at_utc,
          .published_at_utc = batch.observed_at_utc,
          .identifiers = { "txid:" + observation.txid },
          .attributes =
            {
              { .key = "fee_btc", .value = std::to_string(observation.base_fee_btc) },
              { .key = "vsize", .value = std::to_string(observation.vsize) },
            },
          .summary = "high-fee mempool transaction",
        },
        key_pair_, "example-analyzer");
      auto publish_status = dht_.Publish(std::move(record));
      if (!publish_status.ok()) {
        return publish_status;
      }
    }

    if (batch.network_health.has_value()
      && batch.network_health->mempool_transaction_count >= 4U) {
      auto record = blocxxi::core::SignEventRecord(
        blocxxi::core::EventEnvelope {
          .event_type = "bitcoin.fee.mempool-pressure",
          .taxonomy = "fee-market",
          .source = batch.source_name,
          .producer = Name(),
          .window = {
            .start_utc = batch.observed_at_utc,
            .end_utc = batch.observed_at_utc,
          },
          .observed_at_utc = batch.observed_at_utc,
          .published_at_utc = batch.observed_at_utc,
          .identifiers = { "height:" + std::to_string(batch.latest_block->height) },
          .attributes =
            {
              { .key = "mempool_txs",
                .value
                = std::to_string(batch.network_health->mempool_transaction_count) },
              { .key = "connections",
                .value = std::to_string(batch.network_health->connection_count) },
            },
          .summary = "mempool pressure wave",
        },
        key_pair_, "example-analyzer");
      auto publish_status = dht_.Publish(std::move(record));
      if (!publish_status.ok()) {
        return publish_status;
      }
    }

    return blocxxi::core::Status::Success();
  }

private:
  blocxxi::bitcoin::BitcoinCoreRpcAdapter adapter_;
  blocxxi::p2p::MemoryEventDht& dht_;
  blocxxi::crypto::KeyPair key_pair_;
};

} // namespace

auto main() -> int
{
  auto node = blocxxi::node::Node();
  auto dht = blocxxi::p2p::MemoryEventDht {};
  auto service = std::make_shared<RulesOnlyAnalyzerService>(
    blocxxi::bitcoin::BitcoinCoreRpcAdapter(
      blocxxi::bitcoin::BitcoinCoreRpcConfig {
        .network = blocxxi::bitcoin::Network::Mainnet,
        .max_mempool_transactions = 8,
      },
      std::make_shared<ScriptedTransport>()),
    dht, blocxxi::crypto::KeyPair());

  if (!node.Start().ok()) {
    return 1;
  }
  node.RegisterService(service);
  if (!node.RunServicesOnce().ok()) {
    return 2;
  }

  auto const events = dht.Query(blocxxi::p2p::EventQuery {
    .taxonomy = std::string { "transaction" },
    .limit = 8,
  });
  std::cout << "analyzer-events=" << events.size() << '\n';
  return events.empty() ? 3 : 0;
}
