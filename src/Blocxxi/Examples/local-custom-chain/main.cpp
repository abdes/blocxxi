//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <iostream>

#include <Blocxxi/Node/node.h>

auto main() -> int
{
  auto options = blocxxi::node::NodeOptions {};
  options.chain.chain_id = "demo.custom";
  options.chain.display_name = "Demo Custom Chain";
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
    blocxxi::core::Transaction::FromText("custom.asset", "mint:1", "issuer=demo"));
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
  return 0;
}
