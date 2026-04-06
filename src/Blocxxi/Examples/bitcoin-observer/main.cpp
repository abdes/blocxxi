//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <Blocxxi/Bitcoin/adapter.h>

namespace {

struct Args {
  bool live_signet { false };
  bool live_import { false };
  bool live_block { false };
  bool live_witness_block { false };
  std::string host { "seed.signet.bitcoin.sprovoost.nl" };
  std::uint16_t port { 38333 };
  std::string state_root {};
  std::size_t search_limit { 256U };
};

auto ParseArgs(int argc, char** argv) -> Args
{
  auto args = Args {};
  for (auto index = 1; index < argc; ++index) {
    auto const current = std::string_view(argv[index]);
    auto require_value = [&](std::string_view flag) -> std::string_view {
      if (index + 1 >= argc) {
        throw std::invalid_argument("missing value for " + std::string(flag));
      }
      ++index;
      return argv[index];
    };

    if (current == "--live-signet") {
      args.live_signet = true;
    } else if (current == "--live-import") {
      args.live_import = true;
    } else if (current == "--live-block") {
      args.live_block = true;
    } else if (current == "--live-witness-block") {
      args.live_witness_block = true;
    } else if (current == "--signet-host") {
      args.host = std::string(require_value(current));
    } else if (current == "--signet-port") {
      args.port = static_cast<std::uint16_t>(std::stoul(std::string(require_value(current))));
    } else if (current == "--state-root") {
      args.state_root = std::string(require_value(current));
    } else if (current == "--search-limit") {
      args.search_limit = std::stoul(std::string(require_value(current)));
    } else {
      throw std::invalid_argument("unknown argument: " + std::string(current));
    }
  }
  return args;
}

} // namespace

auto main(int argc, char** argv) -> int
{
  auto const args = ParseArgs(argc, argv);
  if (args.live_signet) {
    auto client = blocxxi::bitcoin::SignetLiveClient({
      .host = args.host,
      .port = args.port,
      .state_root = args.state_root,
    });
    auto result = blocxxi::bitcoin::SignetHeadersResult {};
    auto const status = client.FetchHeaders(result);
    if (!status.ok()) {
      std::cerr << status.message << '\n';
      return 1;
    }

    std::cout << "live-peer=" << result.peer_address << '\n';
    std::cout << "live-protocol-version=" << result.protocol_version << '\n';
    std::cout << "live-headers=" << result.header_hashes.size() << '\n';
    if (!result.header_hashes.empty()) {
      std::cout << "live-first-header=" << result.header_hashes.front() << '\n';
      std::cout << "live-last-header=" << result.header_hashes.back() << '\n';
    }
    return 0;
  }

  if (args.live_import) {
    auto options = blocxxi::node::NodeOptions {};
    if (!args.state_root.empty()) {
      options.storage_mode = blocxxi::node::StorageMode::FileSystem;
      options.storage_root = args.state_root;
    }
    auto node = blocxxi::node::Node(options);
    auto status = node.Start();
    if (!status.ok()) {
      std::cerr << status.message << '\n';
      return 1;
    }

    auto adapter = blocxxi::bitcoin::HeaderSyncAdapter({
      .network = blocxxi::bitcoin::Network::Signet,
      .peer_hint = args.host,
      .header_sync_only = true,
    });
    status = adapter.Bind(node);
    if (!status.ok()) {
      std::cerr << status.message << '\n';
      return 1;
    }

    auto result = blocxxi::bitcoin::SignetHeadersResult {};
    status = adapter.ImportLiveSignetHeaders({
        .host = args.host,
        .port = args.port,
        .state_root = args.state_root,
      },
      &result);
    if (!status.ok()) {
      std::cerr << status.message << '\n';
      return 1;
    }

    std::cout << "live-import-peer=" << result.peer_address << '\n';
    std::cout << "live-imported-heights=" << adapter.ImportedHeights().size() << '\n';
    std::cout << "live-kernel-height=" << node.Snapshot().height << '\n';
    if (!result.headers.empty()) {
      std::cout << "live-import-first-header=" << result.headers.front().hash_hex
                << '\n';
      std::cout << "live-import-last-header=" << result.headers.back().hash_hex
                << '\n';
    }
    return 0;
  }

  if (args.live_block) {
    auto client = blocxxi::bitcoin::SignetLiveClient({
      .host = args.host,
      .port = args.port,
    });
    auto headers = blocxxi::bitcoin::SignetHeadersResult {};
    auto status = client.FetchHeaders(headers);
    if (!status.ok() || headers.header_hashes.empty()) {
      std::cerr << (status.ok() ? "no headers available" : status.message) << '\n';
      return 1;
    }

    auto blocks = blocxxi::bitcoin::SignetBlocksResult {};
    auto const first_hash = headers.header_hashes.front();
    status = client.FetchBlocks(
      std::span<std::string const>(&first_hash, 1U), blocks);
    if (!status.ok()) {
      std::cerr << status.message << '\n';
      return 1;
    }

    std::cout << "live-block-peer=" << blocks.peer_address << '\n';
    std::cout << "live-block-count=" << blocks.blocks.size() << '\n';
    if (!blocks.blocks.empty()) {
      std::cout << "live-block-hash=" << blocks.blocks.front().block_hash_hex << '\n';
      std::cout << "live-block-bytes=" << blocks.blocks.front().payload.size() << '\n';
      std::cout << "live-block-tx-count=" << blocks.metadata.front().transaction_count
                << '\n';
      std::cout << "live-block-prev=" << blocks.metadata.front().previous_hash_hex
                << '\n';
      if (!blocks.metadata.front().transaction_sizes.empty()) {
        std::cout << "live-block-first-tx-bytes="
                  << blocks.metadata.front().transaction_sizes.front() << '\n';
        std::cout << "live-block-first-tx-version="
                  << blocks.metadata.front().transaction_versions.front() << '\n';
        std::cout << "live-block-first-tx-vin="
                  << blocks.metadata.front().transaction_input_counts.front() << '\n';
        std::cout << "live-block-first-tx-vout="
                  << blocks.metadata.front().transaction_output_counts.front() << '\n';
        std::cout << "live-block-first-txid="
                  << blocks.metadata.front().transaction_ids.front() << '\n';
      }
    }
    return 0;
  }

  if (args.live_witness_block) {
    auto client = blocxxi::bitcoin::SignetLiveClient({
      .host = args.host,
      .port = args.port,
    });
    auto headers = blocxxi::bitcoin::SignetHeadersResult {};
    auto status = client.FetchHeaders(headers);
    if (!status.ok() || headers.header_hashes.empty()) {
      std::cerr << (status.ok() ? "no headers available" : status.message) << '\n';
      return 1;
    }

    for (auto index = std::size_t { 0 }; index < headers.header_hashes.size()
         && index < args.search_limit;
         ++index) {
      auto blocks = blocxxi::bitcoin::SignetBlocksResult {};
      auto const hash = headers.header_hashes[index];
      status = client.FetchBlocks(std::span<std::string const>(&hash, 1U), blocks);
      if (!status.ok() || blocks.metadata.empty()) {
        continue;
      }

      auto const& metadata = blocks.metadata.front();
      for (auto tx = std::size_t { 0 }; tx < metadata.transaction_has_witness.size(); ++tx) {
        if (!metadata.transaction_has_witness[tx]) {
          continue;
        }

        std::cout << "live-witness-block-peer=" << blocks.peer_address << '\n';
        std::cout << "live-witness-block-hash=" << blocks.blocks.front().block_hash_hex
                  << '\n';
        std::cout << "live-witness-tx-index=" << tx << '\n';
        std::cout << "live-witness-txid=" << metadata.transaction_ids[tx] << '\n';
        std::cout << "live-witness-wtxid=" << metadata.transaction_witness_ids[tx]
                  << '\n';
        return 0;
      }
    }

    std::cerr << "no witness-bearing block found within bounded search window\n";
    return 1;
  }

  auto node = blocxxi::node::Node();
  auto status = node.Start();
  if (!status.ok()) {
    std::cerr << status.message << '\n';
    return 1;
  }

  auto adapter = blocxxi::bitcoin::HeaderSyncAdapter({
    .network = blocxxi::bitcoin::Network::Signet,
    .peer_hint = "seed.signet.bitcoin.sprovoost.nl",
    .header_sync_only = true,
  });
  status = adapter.Bind(node);
  if (!status.ok()) {
    std::cerr << status.message << '\n';
    return 1;
  }

  auto const headers = std::array {
    blocxxi::bitcoin::Header {
      .height = 1,
      .hash_hex = "0000000000000000000000000000000000000000000000000000000000000001",
      .previous_hash_hex = "0000000000000000000000000000000000000000000000000000000000000000",
      .version = 0x20000000,
    },
    blocxxi::bitcoin::Header {
      .height = 2,
      .hash_hex = "0000000000000000000000000000000000000000000000000000000000000002",
      .previous_hash_hex = "0000000000000000000000000000000000000000000000000000000000000001",
      .version = 0x20000000,
    },
  };
  status = adapter.SubmitHeaders(headers);
  if (!status.ok()) {
    std::cerr << status.message << '\n';
    return 1;
  }

  std::cout << "imported-heights=" << adapter.ImportedHeights().size() << '\n';
  std::cout << "kernel-height=" << node.Snapshot().height << '\n';
  return 0;
}
