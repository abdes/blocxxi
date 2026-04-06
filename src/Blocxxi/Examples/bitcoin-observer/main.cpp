//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <iostream>

#include <Blocxxi/Bitcoin/adapter.h>

auto main() -> int
{
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

  status = adapter.SubmitHeader({
    .height = 1,
    .hash_hex = "0000000000000000000000000000000000000000000000000000000000000001",
    .previous_hash_hex = "0000000000000000000000000000000000000000000000000000000000000000",
    .version = 0x20000000,
  });
  if (!status.ok()) {
    std::cerr << status.message << '\n';
    return 1;
  }

  std::cout << "imported-heights=" << adapter.ImportedHeights().size() << '\n';
  std::cout << "kernel-height=" << node.Snapshot().height << '\n';
  return 0;
}
