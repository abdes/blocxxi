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
  std::string host { "seed.signet.bitcoin.sprovoost.nl" };
  std::uint16_t port { 38333 };
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
    } else if (current == "--signet-host") {
      args.host = std::string(require_value(current));
    } else if (current == "--signet-port") {
      args.port = static_cast<std::uint16_t>(std::stoul(std::string(require_value(current))));
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
    auto node = blocxxi::node::Node();
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
