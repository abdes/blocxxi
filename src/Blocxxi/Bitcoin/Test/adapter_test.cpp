//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <Blocxxi/Bitcoin/adapter.h>

namespace blocxxi::bitcoin {

TEST(BitcoinAdapterTest, HeaderSyncAdapterUsesPublicNodeApi)
{
  auto node = node::Node();
  ASSERT_TRUE(node.Start().ok());

  auto adapter = HeaderSyncAdapter({
    .network = Network::Signet,
    .peer_hint = "seed.signet.example",
    .header_sync_only = true,
  });
  ASSERT_TRUE(adapter.Bind(node).ok());

  auto const status = adapter.SubmitHeader({
    .height = 1,
    .hash_hex = "0001",
    .previous_hash_hex = "0000",
    .version = 0x20000000,
  });

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(adapter.ImportedHeights().size(), 1U);
  EXPECT_EQ(node.Blocks().size(), 2U);
  EXPECT_EQ(node.Snapshot().height, 1);
}

} // namespace blocxxi::bitcoin
