//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#include <Blocxxi/Bitcoin/ingestion.h>

namespace blocxxi::bitcoin {
namespace {

class ScriptedTransport final : public RpcTransport {
public:
  auto Call(std::string_view method, std::string_view, std::string& response_json)
    -> core::Status override
  {
    auto const found = responses_.find(std::string(method));
    if (found == responses_.end()) {
      return core::Status::Failure(core::StatusCode::NotFound, "missing rpc method");
    }
    response_json = found->second;
    return core::Status::Success();
  }

  std::map<std::string, std::string> responses_ {
    { "getblockcount", R"({"result":101})" },
    { "getbestblockhash", R"({"result":"000000abc"})" },
    { "getmempoolinfo", R"({"result":{"size":2,"bytes":512}})" },
    { "getnetworkinfo",
      R"({"result":{"connections":8,"relayfee":0.00001000,"warnings":"ok"}})" },
    { "getblockchaininfo", R"({"result":{"initialblockdownload":false}})" },
    { "getrawmempool",
      R"({"result":{"tx1":{"vsize":200,"weight":800,"fees":{"base":0.00020000}},"tx2":{"vsize":300,"weight":1200,"fees":{"base":0.00030000}}}})" },
    { "getblockstats", R"({"result":{"txs":10,"totalfee":0.00150000}})" },
  };
};

} // namespace

TEST(BitcoinIngestionTest, CoreRpcAdapterNormalizesMempoolAndNetworkObservations)
{
  auto transport = std::make_shared<ScriptedTransport>();
  auto adapter = BitcoinCoreRpcAdapter(BitcoinCoreRpcConfig {
    .network = Network::Mainnet,
    .max_mempool_transactions = 8,
  }, transport);

  auto batch = ObservationBatch {};
  auto const status = adapter.Poll(batch);

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(batch.source_name, "bitcoin.core-rpc");
  ASSERT_TRUE(batch.latest_block.has_value());
  EXPECT_EQ(batch.latest_block->height, 101U);
  EXPECT_EQ(batch.latest_block->block_hash_hex, "000000abc");
  EXPECT_EQ(batch.latest_block->transaction_count, 10U);
  EXPECT_EQ(batch.latest_block->total_fee_satoshis, 150000U);

  ASSERT_TRUE(batch.network_health.has_value());
  EXPECT_EQ(batch.network_health->connection_count, 8U);
  EXPECT_EQ(batch.network_health->mempool_transaction_count, 2U);
  EXPECT_EQ(batch.network_health->mempool_bytes, 512U);
  EXPECT_FALSE(batch.network_health->initial_block_download);

  ASSERT_EQ(batch.mempool_transactions.size(), 2U);
  EXPECT_EQ(batch.mempool_transactions[0].txid, "tx1");
  EXPECT_EQ(batch.mempool_transactions[0].vsize, 200U);
  EXPECT_DOUBLE_EQ(batch.mempool_transactions[1].base_fee_btc, 0.0003);
}

TEST(BitcoinIngestionTest, CoreRpcAdapterRejectsMissingResponses)
{
  auto transport = std::make_shared<ScriptedTransport>();
  transport->responses_.erase("getrawmempool");

  auto adapter = BitcoinCoreRpcAdapter(BitcoinCoreRpcConfig {}, transport);
  auto batch = ObservationBatch {};
  auto const status = adapter.Poll(batch);

  EXPECT_EQ(status.code, core::StatusCode::NotFound);
}

TEST(BitcoinIngestionTest, ResolveBitcoinCoreRpcConfigReadsBitcoinConf)
{
  auto const root
    = std::filesystem::temp_directory_path() / "blocxxi-bitcoin-conf-test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  auto config = std::ofstream(root / "bitcoin.conf");
  config << "rpcconnect=10.0.0.2\n";
  config << "rpcport=18443\n";
  config << "rpcuser=blocxxi\n";
  config << "rpcpassword=secret\n";
  config.close();

  auto resolved = ResolveBitcoinCoreRpcConfig(
    BitcoinCoreRpcConfig { .network = Network::Regtest },
    root / "bitcoin.conf");

  EXPECT_EQ(resolved.connection.host, "10.0.0.2");
  EXPECT_EQ(resolved.connection.port, 18443U);
  EXPECT_EQ(resolved.connection.username, "blocxxi");
  EXPECT_EQ(resolved.connection.password, "secret");
  std::filesystem::remove_all(root);
}

TEST(BitcoinIngestionTest, ResolveBitcoinCoreRpcConfigFallsBackToCookie)
{
  auto const root
    = std::filesystem::temp_directory_path() / "blocxxi-bitcoin-cookie-test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  auto config = std::ofstream(root / "bitcoin.conf");
  config << "datadir=" << root.string() << "\n";
  config.close();
  auto cookie = std::ofstream(root / ".cookie");
  cookie << "__cookie__:secret-cookie";
  cookie.close();

  auto resolved = ResolveBitcoinCoreRpcConfig(
    BitcoinCoreRpcConfig { .network = Network::Mainnet },
    root / "bitcoin.conf");

  EXPECT_EQ(resolved.connection.username, "__cookie__");
  EXPECT_EQ(resolved.connection.password, "secret-cookie");
  std::filesystem::remove_all(root);
}

TEST(BitcoinIngestionTest, ResolveBitcoinCoreRpcConfigDetailsReportsSources)
{
  auto const root
    = std::filesystem::temp_directory_path() / "blocxxi-bitcoin-source-test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  auto config = std::ofstream(root / "bitcoin.conf");
  config << "rpcconnect=127.0.0.2\n";
  config << "rpcport=18332\n";
  config << "rpcuser=blocxxi\n";
  config << "rpcpassword=secret\n";
  config.close();

  auto resolved = ResolveBitcoinCoreRpcConfigDetails(
    BitcoinCoreRpcConfig { .network = Network::Mainnet },
    root / "bitcoin.conf");

  EXPECT_TRUE(resolved.config_file_loaded);
  EXPECT_EQ(resolved.host_source,
    "bitcoin.conf:" + (root / "bitcoin.conf").string());
  EXPECT_EQ(resolved.username_source,
    "bitcoin.conf:" + (root / "bitcoin.conf").string());
  std::filesystem::remove_all(root);
}

} // namespace blocxxi::bitcoin
