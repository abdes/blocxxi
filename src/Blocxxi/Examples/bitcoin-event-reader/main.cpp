//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <iostream>
#include <map>
#include <memory>
#include <string>

#include <Blocxxi/Bitcoin/ingestion.h>
#include <Blocxxi/Core/event_record.h>
#include <Blocxxi/Crypto/keypair.h>
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
    { "getblockcount", R"({"result":512})" },
    { "getbestblockhash", R"({"result":"000000bead"})" },
    { "getmempoolinfo", R"({"result":{"size":1,"bytes":128}})" },
    { "getnetworkinfo",
      R"({"result":{"connections":4,"relayfee":0.00001000,"warnings":"quiet"}})" },
    { "getblockchaininfo", R"({"result":{"initialblockdownload":false}})" },
    { "getrawmempool",
      R"({"result":{"tx-read":{"vsize":160,"weight":640,"fees":{"base":0.00015000}}}})" },
    { "getblockstats", R"({"result":{"txs":12,"totalfee":0.00080000}})" },
  };
};

} // namespace

auto main() -> int
{
  auto adapter = blocxxi::bitcoin::BitcoinCoreRpcAdapter(
    blocxxi::bitcoin::BitcoinCoreRpcConfig {
      .network = blocxxi::bitcoin::Network::Mainnet,
      .max_mempool_transactions = 4,
    },
    std::make_shared<ScriptedTransport>());
  auto batch = blocxxi::bitcoin::ObservationBatch {};
  if (!adapter.Poll(batch).ok()) {
    return 1;
  }
  if (!batch.latest_block.has_value() || !batch.network_health.has_value()) {
    return 2;
  }

  auto dht = blocxxi::p2p::MemoryEventDht {};
  auto const key_pair = blocxxi::crypto::KeyPair();
  auto const record = blocxxi::core::SignEventRecord(
    blocxxi::core::EventEnvelope {
      .event_type = "bitcoin.reader.snapshot",
      .taxonomy = "reader",
      .source = batch.source_name,
      .producer = "blocxxi.reader",
      .window = {
        .start_utc = batch.observed_at_utc,
        .end_utc = batch.observed_at_utc,
      },
      .observed_at_utc = batch.observed_at_utc,
      .published_at_utc = batch.observed_at_utc,
      .identifiers = { "height:" + std::to_string(batch.latest_block->height) },
      .attributes =
        {
          { .key = "connections",
            .value = std::to_string(batch.network_health->connection_count) },
          { .key = "mempool_txs",
            .value = std::to_string(batch.network_health->mempool_transaction_count) },
        },
      .summary = "reader-side snapshot",
    },
    key_pair, "reader");
  if (!dht.Publish(record).ok()) {
    return 3;
  }

  auto const matches = dht.Query(blocxxi::p2p::EventQuery {
    .taxonomy = std::string { "reader" },
    .limit = 4,
  });
  std::cout << "reader-events=" << matches.size() << '\n';
  return matches.size() == 1U ? 0 : 4;
}
