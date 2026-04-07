//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/P2P/api_export.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <Blocxxi/Core/event_record.h>
#include <Blocxxi/Core/result.h>
#include <Blocxxi/P2P/kademlia/endpoint.h>
#include <Blocxxi/P2P/kademlia/node.h>
#include <Blocxxi/P2P/kademlia/session.h>

namespace blocxxi::p2p {

struct EventQuery {
  std::optional<std::string> deterministic_key {};
  std::optional<std::string> event_type {};
  std::optional<std::string> taxonomy {};
  std::optional<std::int64_t> window_start_utc {};
  std::optional<std::int64_t> window_end_utc {};
  std::vector<std::string> identifiers {};
  std::size_t limit { 64 };
};

struct RepublishPolicy {
  std::int64_t freshness_window_seconds { 3600 };
};

struct PublishedEvent {
  std::string dht_key {};
  kademlia::Node::IdType info_hash {};
  core::SignedEventRecord record {};
  std::int64_t published_at_utc { 0 };
  std::uint32_t publish_count { 0 };
};

struct PublishResult {
  std::string dht_key {};
  kademlia::Node::IdType info_hash {};
  bool inserted { false };
  bool duplicate { false };
};

struct EventPublisherIdentity {
  blocxxi::crypto::KeyPair key_pair {};
  std::string signer_name {};
};

struct EventQueryResult {
  std::vector<PublishedEvent> records {};
  std::vector<kademlia::IpEndpoint> peers {};
};

struct MainlineEventDhtOptions {
  std::vector<kademlia::IpEndpoint> routers {};
  std::chrono::milliseconds request_timeout { 1000 };
  std::uint16_t announce_port { 0 };
  bool implied_port { true };
};

class BLOCXXI_P2P_API MemoryEventDht {
public:
  auto Publish(core::SignedEventRecord record) -> core::Status;
  [[nodiscard]] auto LastPublish() const -> std::optional<PublishResult>;
  [[nodiscard]] auto Query(EventQuery query) const -> std::vector<PublishedEvent>;
  auto RepublishRecent(RepublishPolicy policy) -> std::size_t;

private:
  std::unordered_map<std::string, std::vector<PublishedEvent>> entries_ {};
  std::optional<PublishResult> last_publish_ {};
};

class BLOCXXI_P2P_API MainlineEventDht {
public:
  MainlineEventDht(asio::io_context& io_context,
    kademlia::Session& session, MainlineEventDhtOptions options = {});

  auto Publish(core::SignedEventRecord record) -> core::Status;
  [[nodiscard]] auto LastPublish() const -> std::optional<PublishResult>;
  auto Query(EventQuery query, EventQueryResult& result) -> core::Status;

private:
  auto QueryRouter(std::string const& deterministic_key,
    kademlia::IpEndpoint const& router, std::vector<kademlia::IpEndpoint>& peers,
    std::string* token) -> core::Status;
  auto AnnounceToRouter(std::string const& deterministic_key,
    kademlia::IpEndpoint const& router, std::string token) -> core::Status;

  asio::io_context& io_context_;
  kademlia::Session& session_;
  MainlineEventDhtOptions options_ {};
  MemoryEventDht local_store_ {};
};

auto BLOCXXI_P2P_API PublishEvent(core::EventEnvelope envelope,
  EventPublisherIdentity const& identity, MemoryEventDht& dht,
  core::SignedEventRecord* signed_record = nullptr,
  PublishResult* published = nullptr) -> core::Status;

auto BLOCXXI_P2P_API PublishEvent(core::EventEnvelope envelope,
  EventPublisherIdentity const& identity, MainlineEventDht& dht,
  core::SignedEventRecord* signed_record = nullptr,
  PublishResult* published = nullptr) -> core::Status;

auto BLOCXXI_P2P_API QueryEvents(
  EventQuery query, MemoryEventDht const& dht, EventQueryResult& result)
  -> core::Status;

auto BLOCXXI_P2P_API QueryEvents(
  EventQuery query, MainlineEventDht& dht, EventQueryResult& result)
  -> core::Status;

[[nodiscard]] BLOCXXI_P2P_API auto DeriveInfoHash(
  std::string const& deterministic_key) -> kademlia::Node::IdType;

} // namespace blocxxi::p2p
