//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <Blocxxi/Node/node.h>

namespace {

struct Args {
  std::optional<std::filesystem::path> storage_root {};
  std::string payload { "mint:1" };
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

    if (current == "--storage-root") {
      args.storage_root = std::filesystem::path(require_value(current));
    } else if (current == "--payload") {
      args.payload = std::string(require_value(current));
    } else {
      throw std::invalid_argument("unknown argument: " + std::string(current));
    }
  }
  return args;
}

} // namespace

auto main(int argc, char** argv) -> int
{
  auto args = ParseArgs(argc, argv);
  auto options = blocxxi::node::NodeOptions {};
  options.chain.chain_id = "demo.custom";
  options.chain.display_name = "Demo Custom Chain";
  if (args.storage_root.has_value()) {
    options.storage_mode = blocxxi::node::StorageMode::FileSystem;
    options.storage_root = *args.storage_root;
  }
  auto node = blocxxi::node::Node(options);

  auto status = node.Start();
  if (!status.ok()) {
    std::cerr << status.message << '\n';
    return 1;
  }

  node.Subscribe([](blocxxi::core::ChainEvent const& event) {
    std::cout << "event=" << static_cast<int>(event.type)
              << " height=" << event.snapshot.height << '\n';
  });

  status = node.SubmitTransaction(
    blocxxi::core::Transaction::FromText("custom.asset", args.payload, "issuer=demo"));
  if (!status.ok()) {
    std::cerr << status.message << '\n';
    return 1;
  }

  status = node.CommitPending("custom-chain");
  if (!status.ok()) {
    std::cerr << status.message << '\n';
    return 1;
  }

  auto const snapshot = node.Snapshot();
  std::cout << "chain=" << node.Options().chain.chain_id << '\n';
  std::cout << "height=" << snapshot.height << '\n';
  std::cout << "blocks=" << node.Blocks().size() << '\n';
  std::cout << "payload=" << node.Blocks().back().transactions.front().PayloadText()
            << '\n';
  return 0;
}
