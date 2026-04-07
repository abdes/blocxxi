//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <chrono>

#include <Blocxxi/Core/event_record.h>
#include <Blocxxi/P2P/event_dht.h>
#include <Blocxxi/P2P/kademlia/channel.h>
#include <Blocxxi/P2P/kademlia/mainline_node.h>

namespace blocxxi::p2p {
namespace {

auto MakeTestNode(std::string_view address, std::uint16_t port) -> kademlia::Node
{
  return kademlia::Node(
    kademlia::Node::IdType::RandomHash(), std::string(address), port);
}

auto NextPortBase() -> std::uint16_t
{
  static auto next_port = []() {
    auto const seed = static_cast<std::uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
    return static_cast<std::uint16_t>(32000U + (seed % 2000U));
  }();
  auto const current = next_port;
  next_port = static_cast<std::uint16_t>(next_port + 10U);
  return current;
}

} // namespace

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

TEST(EventDhtTest, PlatformPublishAndQueryHelpersKeepSigningOutOfTheApp)
{
  auto dht = MemoryEventDht();
  auto signed_record = core::SignedEventRecord {};
  auto published = PublishResult {};

  ASSERT_TRUE(PublishEvent(
                core::EventEnvelope {
                  .event_type = "bitcoin.reader.snapshot",
                  .taxonomy = "reader",
                  .source = "bitcoin.rpc",
                  .producer = "blocxxi.reader",
                  .window = { .start_utc = 20, .end_utc = 21 },
                  .observed_at_utc = 20,
                  .published_at_utc = 21,
                  .identifiers = { "height:20" },
                  .summary = "reader",
                },
                EventPublisherIdentity {
                  .key_pair = blocxxi::crypto::KeyPair(
                    "40C756697E6F60AC839FE53DD403F0B254D49A26243A196300CD4D515EE28062"),
                  .signer_name = "reader",
                },
                dht, &signed_record, &published)
                .ok());

  auto query_result = EventQueryResult {};
  ASSERT_TRUE(QueryEvents(
                EventQuery {
                  .deterministic_key = published.dht_key,
                  .limit = 4,
                },
                dht, query_result)
                .ok());
  ASSERT_EQ(query_result.records.size(), 1U);
  EXPECT_TRUE(query_result.peers.empty());
  EXPECT_TRUE(core::VerifyEventRecord(signed_record));
  EXPECT_EQ(query_result.records.front().dht_key, published.dht_key);
}

TEST(EventDhtTest, DeterministicKeyFilterReturnsOnlyMatchingRecords)
{
  auto dht = MemoryEventDht();
  auto const key_pair
    = blocxxi::crypto::KeyPair("40C756697E6F60AC839FE53DD403F0B254D49A26243A196300CD4D515EE28062");
  auto const first = core::SignEventRecord(core::EventEnvelope {
      .event_type = "bitcoin.fee.spike",
      .taxonomy = "fee-market",
      .source = "bitcoin.rpc",
      .producer = "blocxxi.test",
      .window = { .start_utc = 1, .end_utc = 2 },
      .observed_at_utc = 1,
      .published_at_utc = 2,
      .identifiers = { "tx:alpha" },
      .summary = "alpha",
    },
    key_pair, "publisher");
  auto const second = core::SignEventRecord(core::EventEnvelope {
      .event_type = "bitcoin.fee.spike",
      .taxonomy = "fee-market",
      .source = "bitcoin.rpc",
      .producer = "blocxxi.test",
      .window = { .start_utc = 3, .end_utc = 4 },
      .observed_at_utc = 3,
      .published_at_utc = 4,
      .identifiers = { "tx:beta" },
      .summary = "beta",
    },
    key_pair, "publisher");

  ASSERT_TRUE(dht.Publish(first).ok());
  ASSERT_TRUE(dht.Publish(second).ok());

  auto const matches = dht.Query(EventQuery {
    .deterministic_key = first.DeterministicKey(),
    .limit = 8,
  });

  ASSERT_EQ(matches.size(), 1U);
  EXPECT_EQ(matches.front().dht_key, first.DeterministicKey());
  EXPECT_EQ(matches.front().record.envelope.summary, "alpha");
}

TEST(EventDhtTest, MainlineEventDhtQueriesDeterministicPeersAcrossSessions)
{
  auto io_context = asio::io_context {};
  auto const port_base = NextPortBase();
  auto const router = kademlia::IpEndpoint { "127.0.0.1", port_base };
  auto server_channel
    = kademlia::AsyncUdpChannel::ipv4(
      io_context, "127.0.0.1", std::to_string(port_base));
  auto publisher_channel
    = kademlia::AsyncUdpChannel::ipv4(
      io_context, "127.0.0.1", std::to_string(port_base + 1U));
  auto querier_channel
    = kademlia::AsyncUdpChannel::ipv4(
      io_context, "127.0.0.1", std::to_string(port_base + 2U));

  auto server = kademlia::Session(kademlia::MainlineDhtNode(io_context,
    MakeTestNode("127.0.0.1", port_base), std::move(server_channel)));
  auto publisher = kademlia::Session(kademlia::MainlineDhtNode(io_context,
    MakeTestNode("127.0.0.1", static_cast<std::uint16_t>(port_base + 1U)),
    std::move(publisher_channel)));
  auto querier = kademlia::Session(kademlia::MainlineDhtNode(io_context,
    MakeTestNode("127.0.0.1", static_cast<std::uint16_t>(port_base + 2U)),
    std::move(querier_channel)));

  server.Start();
  publisher.Start();
  querier.Start();

  auto const options = MainlineEventDhtOptions {
    .routers = { router },
    .request_timeout = std::chrono::seconds { 2 },
  };
  auto publisher_dht = MainlineEventDht(io_context, publisher, options);
  auto querier_dht = MainlineEventDht(io_context, querier, options);

  auto const key_pair
    = blocxxi::crypto::KeyPair("40C756697E6F60AC839FE53DD403F0B254D49A26243A196300CD4D515EE28062");
  auto const record = core::SignEventRecord(core::EventEnvelope {
      .event_type = "bitcoin.fee.pressure",
      .taxonomy = "fee-market",
      .source = "bitcoin.rpc",
      .producer = "blocxxi.publisher",
      .window = { .start_utc = 50, .end_utc = 60 },
      .observed_at_utc = 50,
      .published_at_utc = 51,
      .identifiers = { "height:500" },
      .summary = "pressure",
    },
    key_pair, "publisher");

  ASSERT_TRUE(publisher_dht.Publish(record).ok());
  auto const event_key = record.DeterministicKey();
  auto query_result = EventQueryResult {};
  ASSERT_TRUE(querier_dht.Query(EventQuery {
                .deterministic_key = event_key,
                .limit = 8,
              },
                query_result)
                .ok());
  EXPECT_TRUE(query_result.records.empty());
  ASSERT_EQ(query_result.peers.size(), 1U);
  EXPECT_EQ(query_result.peers.front().Address().to_string(), "127.0.0.1");
  EXPECT_EQ(query_result.peers.front().Port(),
    static_cast<std::uint16_t>(port_base + 1U));

  querier.Stop();
  publisher.Stop();
  server.Stop();
}

} // namespace blocxxi::p2p
