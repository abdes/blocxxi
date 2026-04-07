//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <csignal>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include <Blocxxi/Bitcoin/ingestion.h>
#include <Blocxxi/Core/event_record.h>
#include <Blocxxi/Crypto/keypair.h>
#include <Blocxxi/Node/node.h>
#include <Blocxxi/P2P/event_dht.h>

namespace {

auto g_keep_running = std::atomic_bool { true };

void HandleSignal(int) { g_keep_running.store(false); }

struct AnalyzerOptions {
  bool oneshot { false };
  bool scripted { false };
  bool rpc_host_overridden { false };
  bool rpc_port_overridden { false };
  bool rpc_user_overridden { false };
  bool rpc_password_overridden { false };
  bool rpc_path_overridden { false };
  std::chrono::milliseconds poll_interval { 1000 };
  std::optional<std::filesystem::path> bitcoin_conf {};
  blocxxi::bitcoin::BitcoinCoreRpcConfig rpc {};
};

auto OverrideSourcesFromCli(AnalyzerOptions const& options,
  blocxxi::bitcoin::ResolvedBitcoinCoreRpcConfig& resolved) -> void
{
  if (options.rpc_host_overridden) {
    resolved.config.connection.host = options.rpc.connection.host;
    resolved.host_source = "cli:--rpc-host";
  }
  if (options.rpc_port_overridden) {
    resolved.config.connection.port = options.rpc.connection.port;
    resolved.port_source = "cli:--rpc-port";
  }
  if (options.rpc_user_overridden) {
    resolved.config.connection.username = options.rpc.connection.username;
    resolved.username_source = "cli:--rpc-user";
  }
  if (options.rpc_password_overridden) {
    resolved.config.connection.password = options.rpc.connection.password;
    resolved.password_source = "cli:--rpc-password";
  }
  if (options.rpc_path_overridden) {
    resolved.config.connection.path = options.rpc.connection.path;
    resolved.path_source = "cli:--rpc-path";
  }
}

auto ParseOptions(int argc, char** argv) -> AnalyzerOptions
{
  auto options = AnalyzerOptions {};
  for (auto index = 1; index < argc; ++index) {
    auto const current = std::string_view(argv[index]);
    if (current == "--oneshot") {
      options.oneshot = true;
      continue;
    }
    if (current == "--scripted") {
      options.scripted = true;
      continue;
    }
    if (current == "--poll-interval-ms" && (index + 1) < argc) {
      options.poll_interval = std::chrono::milliseconds { std::stoi(argv[++index]) };
      continue;
    }
    if (current == "--rpc-host" && (index + 1) < argc) {
      options.rpc.connection.host = argv[++index];
      options.rpc_host_overridden = true;
      continue;
    }
    if (current == "--rpc-port" && (index + 1) < argc) {
      options.rpc.connection.port = static_cast<std::uint16_t>(std::stoi(argv[++index]));
      options.rpc_port_overridden = true;
      continue;
    }
    if (current == "--rpc-user" && (index + 1) < argc) {
      options.rpc.connection.username = argv[++index];
      options.rpc_user_overridden = true;
      continue;
    }
    if (current == "--rpc-password" && (index + 1) < argc) {
      options.rpc.connection.password = argv[++index];
      options.rpc_password_overridden = true;
      continue;
    }
    if (current == "--rpc-path" && (index + 1) < argc) {
      options.rpc.connection.path = argv[++index];
      options.rpc_path_overridden = true;
      continue;
    }
    if (current == "--bitcoin-conf" && (index + 1) < argc) {
      options.bitcoin_conf = std::filesystem::path(argv[++index]);
      continue;
    }
    if (current == "--help") {
      std::cout
        << "Usage: blocxxi-bitcoin-mempool-analyzer [--oneshot] [--scripted]\n"
        << "       [--poll-interval-ms N] [--rpc-host HOST] [--rpc-port PORT]\n"
        << "       [--rpc-user USER] [--rpc-password PASSWORD] [--rpc-path PATH]\n"
        << "       [--bitcoin-conf PATH]\n";
      std::exit(0);
    }
  }
  return options;
}

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
    blocxxi::p2p::MemoryEventDht& dht, blocxxi::crypto::KeyPair key_pair,
    std::function<void(blocxxi::core::SignedEventRecord const&,
      blocxxi::p2p::PublishResult const&)> on_publish)
    : adapter_(std::move(adapter))
    , dht_(dht)
    , key_pair_(std::move(key_pair))
    , on_publish_(std::move(on_publish))
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
      auto const published_record = record;
      auto publish_status = dht_.Publish(std::move(record));
      if (!publish_status.ok()) {
        return publish_status;
      }
      if (auto const published = dht_.LastPublish(); published.has_value()
        && on_publish_) {
        on_publish_(published_record, *published);
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
      auto const published_record = record;
      auto publish_status = dht_.Publish(std::move(record));
      if (!publish_status.ok()) {
        return publish_status;
      }
      if (auto const published = dht_.LastPublish(); published.has_value()
        && on_publish_) {
        on_publish_(published_record, *published);
      }
    }

    return blocxxi::core::Status::Success();
  }

private:
  blocxxi::bitcoin::BitcoinCoreRpcAdapter adapter_;
  blocxxi::p2p::MemoryEventDht& dht_;
  blocxxi::crypto::KeyPair key_pair_;
  std::function<void(blocxxi::core::SignedEventRecord const&,
    blocxxi::p2p::PublishResult const&)>
    on_publish_ {};
};

} // namespace

auto main(int argc, char** argv) -> int
{
  auto options = ParseOptions(argc, argv);
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  auto resolved = blocxxi::bitcoin::ResolvedBitcoinCoreRpcConfig {
    .config = options.rpc,
  };
  if (!options.scripted) {
    resolved = blocxxi::bitcoin::ResolveBitcoinCoreRpcConfigDetails(
      blocxxi::bitcoin::BitcoinCoreRpcConfig {
        .network = options.rpc.network,
        .max_mempool_transactions = options.rpc.max_mempool_transactions,
      },
      options.bitcoin_conf);
    OverrideSourcesFromCli(options, resolved);
    options.rpc = resolved.config;
  }

  if (!options.scripted
    && (options.rpc.connection.username.empty()
      || options.rpc.connection.password.empty())) {
    std::cerr
      << "missing Bitcoin Core RPC credentials; provide --rpc-user/--rpc-password, "
      << "set BITCOIN_RPC_USER/BITCOIN_RPC_PASSWORD or BLOCXXI_RPC_USER/BLOCXXI_RPC_PASSWORD, "
      << "or point --bitcoin-conf at bitcoin.conf / cookie auth, or pass --scripted "
      << "for the bounded demo transport\n";
    return 4;
  }

  auto transport = std::shared_ptr<blocxxi::bitcoin::RpcTransport> {};
  if (options.scripted) {
    transport = std::make_shared<ScriptedTransport>();
  }

  auto const publish_printer
    = [](blocxxi::core::SignedEventRecord const& record,
        blocxxi::p2p::PublishResult const& published) {
        auto identifiers = std::ostringstream {};
        for (auto index = std::size_t { 0 }; index < record.envelope.identifiers.size();
             ++index) {
          if (index > 0U) {
            identifiers << ',';
          }
          identifiers << record.envelope.identifiers[index];
        }
        std::cout << "published-event"
                  << " type=" << record.envelope.event_type
                  << " taxonomy=" << record.envelope.taxonomy
                  << " key=" << published.dht_key
                  << " duplicate=" << (published.duplicate ? "true" : "false")
                  << " summary=\"" << record.envelope.summary << "\""
                  << " identifiers=[" << identifiers.str() << "]\n";
      };

  auto node = blocxxi::node::Node();
  auto dht = blocxxi::p2p::MemoryEventDht {};
  auto service = std::make_shared<RulesOnlyAnalyzerService>(
    blocxxi::bitcoin::BitcoinCoreRpcAdapter(
      options.rpc, std::move(transport)),
    dht, blocxxi::crypto::KeyPair(), publish_printer);

  if (!node.Start().ok()) {
    return 1;
  }
  node.RegisterService(service);
  std::cout << "analyzer-started mode=" << (options.scripted ? "scripted" : "bitcoin-core-rpc")
            << " poll_interval_ms=" << options.poll_interval.count() << '\n';
  if (!options.scripted) {
    std::cout << "analyzer-rpc endpoint=" << options.rpc.connection.host << ':'
              << options.rpc.connection.port << " path=" << options.rpc.connection.path
              << " host-source=" << resolved.host_source
              << " port-source=" << resolved.port_source
              << " user-source=" << resolved.username_source
              << " password-source=" << resolved.password_source << '\n';
  }
  auto cycle = std::size_t { 0 };
  while (g_keep_running.load()) {
    if (!node.RunServicesOnce().ok()) {
      return 2;
    }

    auto const events = dht.Query(blocxxi::p2p::EventQuery {
      .taxonomy = std::string { "transaction" },
      .limit = 8,
    });
    std::cout << "analyzer-cycle=" << cycle << " analyzer-events=" << events.size()
              << '\n';
    if (events.empty()) {
      return 3;
    }

    cycle += 1U;
    if (options.oneshot) {
      break;
    }
    std::this_thread::sleep_for(options.poll_interval);
  }

  return 0;
}
