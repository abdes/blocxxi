//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <Blocxxi/Core/event_record.h>
#include <Blocxxi/P2P/event_dht.h>

namespace blocxxi::p2p {

TEST(EventDhtTest, PublishQueryAndDuplicateRepublishAreDeterministic)
{
  auto dht = MemoryEventDht();
  auto const key_pair
    = blocxxi::crypto::KeyPair("40C756697E6F60AC839FE53DD403F0B254D49A26243A196300CD4D515EE28062");
  auto record = core::SignEventRecord(core::EventEnvelope {
      .event_type = "bitcoin.fee.spike",
      .taxonomy = "fee-market",
      .source = "bitcoin.rpc",
      .producer = "blocxxi.test",
      .window = { .start_utc = 10, .end_utc = 20 },
      .observed_at_utc = 10,
      .published_at_utc = 11,
      .identifiers = { "tx:1" },
      .summary = "spike",
    },
    key_pair, "publisher");

  ASSERT_TRUE(dht.Publish(record).ok());
  ASSERT_TRUE(dht.LastPublish().has_value());
  EXPECT_TRUE(dht.LastPublish()->inserted);
  auto const query = dht.Query(EventQuery {
    .event_type = std::string { "bitcoin.fee.spike" },
    .window_start_utc = 5,
    .window_end_utc = 25,
    .identifiers = { "tx:1" },
    .limit = 8,
  });
  ASSERT_EQ(query.size(), 1U);
  EXPECT_EQ(query.front().dht_key, record.DeterministicKey());

  ASSERT_TRUE(dht.Publish(record).ok());
  ASSERT_TRUE(dht.LastPublish().has_value());
  EXPECT_TRUE(dht.LastPublish()->duplicate);

  EXPECT_EQ(dht.RepublishRecent(RepublishPolicy { .freshness_window_seconds = 3600 }),
    1U);
  EXPECT_EQ(DeriveInfoHash(record.DeterministicKey()).ToHex(),
    query.front().info_hash.ToHex());
}

TEST(EventDhtTest, PublishRejectsInvalidSignature)
{
  auto dht = MemoryEventDht();
  auto const key_pair = blocxxi::crypto::KeyPair();
  auto record = core::SignEventRecord(core::EventEnvelope {
      .event_type = "bitcoin.reorg",
      .taxonomy = "network-health",
      .source = "bitcoin.rpc",
    },
    key_pair);
  record.signature_hex = "deadbeef";

  auto const status = dht.Publish(record);
  EXPECT_EQ(status.code, core::StatusCode::Rejected);
}

} // namespace blocxxi::p2p
