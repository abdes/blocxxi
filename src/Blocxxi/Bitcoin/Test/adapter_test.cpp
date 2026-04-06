//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <vector>

#include <Blocxxi/Bitcoin/adapter.h>

namespace blocxxi::bitcoin {

TEST(BitcoinAdapterTest, HeaderSyncAdapterUsesPublicNodeApi)
{
  auto node = node::Node();
  auto events = std::vector<blocxxi::core::ChainEvent> {};
  node.Subscribe([&](blocxxi::core::ChainEvent const& event) {
    events.push_back(event);
  });
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
  ASSERT_GE(events.size(), 4U);
  EXPECT_EQ(events[2].type, blocxxi::core::EventType::DiscoveryAttached);
  EXPECT_EQ(events[2].message, "bitcoin.peer:seed.signet.example");
  EXPECT_EQ(events[3].type, blocxxi::core::EventType::AdapterAttached);
  EXPECT_EQ(events[3].message, "bitcoin.header-sync:signet:headers-only");
  EXPECT_EQ(node.Blocks().back().transactions.front().metadata,
    "network=signet;mode=headers-only;peer_hint=seed.signet.example");
}

TEST(BitcoinAdapterTest, HeaderSyncAdapterRejectsUnboundOrIncompleteHeaders)
{
  auto adapter = HeaderSyncAdapter({
    .network = Network::Regtest,
    .header_sync_only = true,
  });

  auto status = adapter.SubmitHeader({
    .height = 1,
    .hash_hex = "01",
    .previous_hash_hex = "00",
    .version = 1,
  });
  EXPECT_EQ(status.code, blocxxi::core::StatusCode::Rejected);

  auto node = node::Node();
  ASSERT_TRUE(node.Start().ok());
  ASSERT_TRUE(adapter.Bind(node).ok());

  status = adapter.SubmitHeader({
    .height = 1,
    .hash_hex = "",
    .previous_hash_hex = "00",
    .version = 1,
  });
  EXPECT_EQ(status.code, blocxxi::core::StatusCode::InvalidArgument);

  status = adapter.SubmitHeader({
    .height = 2,
    .hash_hex = "02",
    .previous_hash_hex = "",
    .version = 1,
  });
  EXPECT_EQ(status.code, blocxxi::core::StatusCode::InvalidArgument);
}

} // namespace blocxxi::bitcoin
