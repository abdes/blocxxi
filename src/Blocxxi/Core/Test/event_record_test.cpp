//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <Blocxxi/Core/event_record.h>

namespace blocxxi::core {

TEST(EventRecordTest, CanonicalizationSortsIdentifiersAndAttributes)
{
  auto envelope = EventEnvelope {
    .event_type = "bitcoin.fee.spike",
    .taxonomy = "fee-market",
    .source = "bitcoin.rpc",
    .producer = "blocxxi.test",
    .window = { .start_utc = 10, .end_utc = 20 },
    .observed_at_utc = 42,
    .published_at_utc = 43,
    .identifiers = { "tx:2", "tx:1" },
    .attributes = {
      { .key = "b", .value = "2" },
      { .key = "a", .value = "1" },
    },
    .summary = "spike",
    .payload = ToBytes("payload"),
  };

  auto const canonical = envelope.CanonicalText();

  EXPECT_NE(canonical.find("identifiers=tx:1,tx:2"), std::string::npos);
  EXPECT_NE(canonical.find("attributes=a=1|b=2"), std::string::npos);
}

TEST(EventRecordTest, SignedEventRecordVerifiesAndHasStableKey)
{
  auto envelope = EventEnvelope {
    .event_type = "bitcoin.network.health",
    .taxonomy = "network-health",
    .source = "bitcoin.rpc",
    .producer = "blocxxi.node",
    .window = { .start_utc = 100, .end_utc = 160 },
    .observed_at_utc = 150,
    .published_at_utc = 151,
    .identifiers = { "height:100" },
    .attributes = { { .key = "connections", .value = "8" } },
    .summary = "healthy",
    .payload = ToBytes("healthy"),
  };

  auto const key_pair
    = blocxxi::crypto::KeyPair("40C756697E6F60AC839FE53DD403F0B254D49A26243A196300CD4D515EE28062");
  auto const first = SignEventRecord(envelope, key_pair, "test-node");
  auto const second = SignEventRecord(envelope, key_pair, "test-node");

  EXPECT_TRUE(VerifyEventRecord(first));
  EXPECT_EQ(first.DeterministicKey(), second.DeterministicKey());
}

TEST(EventRecordTest, VerifyRejectsSignatureMismatch)
{
  auto envelope = EventEnvelope {
    .event_type = "bitcoin.block.empty",
    .taxonomy = "block",
    .source = "bitcoin.rpc",
    .producer = "blocxxi.node",
    .window = { .start_utc = 1, .end_utc = 2 },
    .observed_at_utc = 3,
    .published_at_utc = 4,
    .summary = "empty block",
  };

  auto const key_pair = blocxxi::crypto::KeyPair();
  auto record = SignEventRecord(envelope, key_pair);
  record.signature_hex = "abcd";

  EXPECT_FALSE(VerifyEventRecord(record));
}

} // namespace blocxxi::core
