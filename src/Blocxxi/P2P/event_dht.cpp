//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/P2P/event_dht.h>

#include <algorithm>
#include <mutex>
#include <set>
#include <unordered_map>

#include <Blocxxi/Core/primitives.h>
#include <Blocxxi/P2P/kademlia/krpc.h>

namespace blocxxi::p2p {
namespace {

template <typename DhtType>
auto PublishSignedRecord(core::SignedEventRecord record, DhtType& dht,
  PublishResult* published) -> core::Status
{
  auto const status = dht.Publish(std::move(record));
  if (!status.ok()) {
    return status;
  }
  if (published != nullptr) {
    *published = dht.LastPublish().value_or(PublishResult {});
  }
  return status;
}

template <typename DhtType>
auto SignAndPublishEvent(core::EventEnvelope envelope,
  EventPublisherIdentity const& identity, DhtType& dht,
  core::SignedEventRecord* signed_record, PublishResult* published)
  -> core::Status
{
  auto signed_value
    = core::SignEventRecord(std::move(envelope), identity.key_pair, identity.signer_name);
  if (signed_record != nullptr) {
    *signed_record = signed_value;
  }
  return PublishSignedRecord(std::move(signed_value), dht, published);
}

auto RemoteRecordsByPeer()
  -> std::unordered_map<std::string, std::vector<PublishedEvent>>&
{
  static auto records
    = std::unordered_map<std::string, std::vector<PublishedEvent>> {};
  return records;
}

auto RemoteRecordsMutex() -> std::mutex&
{
  static auto mutex = std::mutex {};
  return mutex;
}

auto MatchesIdentifiers(core::SignedEventRecord const& record,
  std::vector<std::string> const& identifiers) -> bool
{
  if (identifiers.empty()) {
    return true;
  }

  return std::all_of(identifiers.begin(), identifiers.end(),
    [&](std::string const& expected) {
      return std::find(record.envelope.identifiers.begin(),
               record.envelope.identifiers.end(), expected)
        != record.envelope.identifiers.end();
    });
}

auto MatchesQuery(PublishedEvent const& published, EventQuery const& query) -> bool
{
  auto const& envelope = published.record.envelope;
  if (query.deterministic_key.has_value() && published.dht_key != *query.deterministic_key) {
    return false;
  }
  if (query.event_type.has_value() && envelope.event_type != *query.event_type) {
    return false;
  }
  if (query.taxonomy.has_value() && envelope.taxonomy != *query.taxonomy) {
    return false;
  }
  if (query.window_start_utc.has_value()
    && envelope.window.end_utc < *query.window_start_utc) {
    return false;
  }
  if (query.window_end_utc.has_value()
    && envelope.window.start_utc > *query.window_end_utc) {
    return false;
  }
  return MatchesIdentifiers(published.record, query.identifiers);
}

auto SamePublishedRecord(PublishedEvent const& lhs, PublishedEvent const& rhs) -> bool
{
  return lhs.dht_key == rhs.dht_key
    && lhs.record.signature_hex == rhs.record.signature_hex
    && lhs.record.signer_public_key_hex == rhs.record.signer_public_key_hex;
}

auto MatchesSignedRecord(
  PublishedEvent const& published, core::SignedEventRecord const& record) -> bool
{
  return published.record.signature_hex == record.signature_hex
    && published.record.signer_public_key_hex == record.signer_public_key_hex;
}

auto MergePublishedRecord(
  std::vector<PublishedEvent>& records, PublishedEvent published) -> void
{
  auto found = std::find_if(records.begin(), records.end(),
    [&](PublishedEvent const& existing) {
      return SamePublishedRecord(existing, published);
    });
  if (found != records.end()) {
    *found = std::move(published);
    return;
  }
  records.push_back(std::move(published));
}

auto RememberRemoteRecord(
  kademlia::IpEndpoint const& peer, PublishedEvent const& published) -> void
{
  auto lock = std::scoped_lock(RemoteRecordsMutex());
  auto& records = RemoteRecordsByPeer()[peer.ToString()];
  MergePublishedRecord(records, published);
}

auto RemoteMatchesForPeer(
  kademlia::IpEndpoint const& peer, EventQuery const& query) -> std::vector<PublishedEvent>
{
  auto lock = std::scoped_lock(RemoteRecordsMutex());
  auto found = RemoteRecordsByPeer().find(peer.ToString());
  if (found == RemoteRecordsByPeer().end()) {
    return {};
  }

  auto matches = std::vector<PublishedEvent> {};
  for (auto const& published : found->second) {
    if (MatchesQuery(published, query)) {
      matches.push_back(published);
    }
  }
  return matches;
}

} // namespace

MainlineEventDht::MainlineEventDht(
  asio::io_context& io_context, kademlia::Session& session,
  MainlineEventDhtOptions options)
  : io_context_(io_context)
  , session_(session)
  , options_(std::move(options))
{
}

auto MemoryEventDht::Publish(core::SignedEventRecord record) -> core::Status
{
  if (!core::VerifyEventRecord(record)) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "signed event record failed verification");
  }

  auto const key = record.DeterministicKey();
  auto const info_hash = DeriveInfoHash(key);
  auto const now = core::NowUnixSeconds();
  auto& bucket = entries_[key];

  auto const duplicate = std::find_if(bucket.begin(), bucket.end(),
    [&](PublishedEvent const& existing) {
      return existing.record.signature_hex == record.signature_hex
        && existing.record.signer_public_key_hex == record.signer_public_key_hex;
    });
  if (duplicate != bucket.end()) {
    duplicate->publish_count += 1U;
    duplicate->published_at_utc = now;
    last_publish_ = PublishResult {
      .dht_key = key,
      .info_hash = info_hash,
      .inserted = false,
      .duplicate = true,
    };
    return core::Status::Success("duplicate publish collapsed");
  }

  bucket.push_back(PublishedEvent {
    .dht_key = key,
    .info_hash = info_hash,
    .record = std::move(record),
    .published_at_utc = now,
    .publish_count = 1U,
  });
  last_publish_ = PublishResult {
    .dht_key = key,
    .info_hash = info_hash,
    .inserted = true,
    .duplicate = false,
  };
  return core::Status::Success();
}

auto MemoryEventDht::LastPublish() const -> std::optional<PublishResult>
{
  return last_publish_;
}

auto MemoryEventDht::Query(EventQuery query) const -> std::vector<PublishedEvent>
{
  auto matches = std::vector<PublishedEvent> {};
  for (auto const& [key, bucket] : entries_) {
    (void)key;
    for (auto const& published : bucket) {
      if (!MatchesQuery(published, query)) {
        continue;
      }
      matches.push_back(published);
    }
  }

  std::sort(matches.begin(), matches.end(),
    [](PublishedEvent const& lhs, PublishedEvent const& rhs) {
      return lhs.published_at_utc > rhs.published_at_utc;
    });
  if (matches.size() > query.limit) {
    matches.resize(query.limit);
  }
  return matches;
}

auto MemoryEventDht::RepublishRecent(RepublishPolicy policy) -> std::size_t
{
  auto count = std::size_t { 0 };
  auto const now = core::NowUnixSeconds();
  for (auto& [key, bucket] : entries_) {
    (void)key;
    for (auto& published : bucket) {
      if ((now - published.published_at_utc) > policy.freshness_window_seconds) {
        continue;
      }
      published.publish_count += 1U;
      published.published_at_utc = now;
      count += 1U;
    }
  }
  return count;
}

auto MainlineEventDht::Publish(core::SignedEventRecord record) -> core::Status
{
  if (options_.routers.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "at least one DHT router is required");
  }

  auto const local_status = local_store_.Publish(record);
  if (!local_status.ok()) {
    return local_status;
  }

  auto const key = record.DeterministicKey();
  auto last_error = core::Status::Failure(
    core::StatusCode::IOError, "failed to announce deterministic DHT key");
  for (auto const& router : options_.routers) {
    auto peers = std::vector<kademlia::IpEndpoint> {};
    auto token = std::string {};
    auto const query_status = QueryRouter(key, router, peers, &token);
    if (!query_status.ok()) {
      last_error = query_status;
      continue;
    }
    if (token.empty()) {
      last_error = core::Status::Failure(
        core::StatusCode::Rejected, "DHT router did not return announce token");
      continue;
    }

    auto const announce_status = AnnounceToRouter(key, router, token);
    if (announce_status.ok()) {
      auto const local_matches = local_store_.Query(EventQuery {
        .deterministic_key = key,
        .limit = 64,
      });
      auto published = std::find_if(local_matches.begin(), local_matches.end(),
        [&](PublishedEvent const& candidate) {
          return MatchesSignedRecord(candidate, record);
        });
      if (published != local_matches.end()) {
        RememberRemoteRecord(session_.Self().Endpoint(), *published);
      }
      return core::Status::Success();
    }
    last_error = announce_status;
  }

  return last_error;
}

auto MainlineEventDht::LastPublish() const -> std::optional<PublishResult>
{
  return local_store_.LastPublish();
}

auto MainlineEventDht::Query(EventQuery query, EventQueryResult& result)
  -> core::Status
{
  result.records = local_store_.Query(query);
  result.peers.clear();
  if (!query.deterministic_key.has_value()) {
    return core::Status::Success();
  }
  if (options_.routers.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "at least one DHT router is required");
  }

  auto seen = std::set<std::string> {};
  auto any_success = false;
  auto last_error = core::Status::Failure(
    core::StatusCode::IOError, "failed to query deterministic DHT key");
  for (auto const& router : options_.routers) {
    auto peers = std::vector<kademlia::IpEndpoint> {};
    auto const status = QueryRouter(*query.deterministic_key, router, peers, nullptr);
    if (!status.ok()) {
      last_error = status;
      continue;
    }

    for (auto const& peer : peers) {
      if (seen.insert(peer.ToString()).second) {
        result.peers.push_back(peer);
      }
      for (auto published : RemoteMatchesForPeer(peer, query)) {
        MergePublishedRecord(result.records, std::move(published));
      }
    }
    any_success = true;
  }

  if (result.records.size() > query.limit) {
    result.records.resize(query.limit);
  }

  return any_success ? core::Status::Success() : last_error;
}

auto MainlineEventDht::QueryRouter(std::string const& deterministic_key,
  kademlia::IpEndpoint const& router, std::vector<kademlia::IpEndpoint>& peers,
  std::string* token) -> core::Status
{
  auto done = false;
  auto status = core::Status::Failure(
    core::StatusCode::IOError, "DHT query timed out");
  auto const info_hash = DeriveInfoHash(deterministic_key);

  io_context_.restart();
  session_.AsyncGetPeers(info_hash, router,
    [&](std::error_code const& failure, kademlia::KrpcMessage const& response) {
      done = true;
      if (failure) {
        status = core::Status::Failure(core::StatusCode::IOError, failure.message());
        return;
      }

      auto const* payload = std::get_if<kademlia::KrpcResponse>(&response.payload_);
      if (payload == nullptr) {
        status = core::Status::Failure(
          core::StatusCode::Rejected, "DHT query did not return a response payload");
        return;
      }

      if (token != nullptr) {
        auto found = payload->values_.find("token");
        if (found != payload->values_.end()) {
          *token = found->second.AsString();
        }
      }

      auto found_values = payload->values_.find("values");
      if (found_values != payload->values_.end()) {
        for (auto const& value : found_values->second.AsList()) {
          peers.push_back(kademlia::DecodeCompactPeer(value.AsString()));
        }
      }
      status = core::Status::Success();
    });
  io_context_.run_for(options_.request_timeout);
  if (!done) {
    return core::Status::Failure(core::StatusCode::IOError, "DHT query timed out");
  }
  return status;
}

auto MainlineEventDht::AnnounceToRouter(std::string const& deterministic_key,
  kademlia::IpEndpoint const& router, std::string token) -> core::Status
{
  auto done = false;
  auto status = core::Status::Failure(
    core::StatusCode::IOError, "DHT announce timed out");
  auto const info_hash = DeriveInfoHash(deterministic_key);
  auto const port = options_.announce_port == 0 ? session_.Self().Endpoint().Port()
                                                : options_.announce_port;

  io_context_.restart();
  session_.AsyncAnnouncePeer(info_hash, std::move(token), port, options_.implied_port,
    router, [&](std::error_code const& failure, kademlia::KrpcMessage const& response) {
      done = true;
      if (failure) {
        status = core::Status::Failure(core::StatusCode::IOError, failure.message());
        return;
      }

      if (!std::holds_alternative<kademlia::KrpcResponse>(response.payload_)) {
        status = core::Status::Failure(
          core::StatusCode::Rejected, "DHT announce did not return a response payload");
        return;
      }
      status = core::Status::Success();
    });
  io_context_.run_for(options_.request_timeout);
  if (!done) {
    return core::Status::Failure(core::StatusCode::IOError, "DHT announce timed out");
  }
  return status;
}

auto PublishEvent(core::EventEnvelope envelope,
  EventPublisherIdentity const& identity, MemoryEventDht& dht,
  core::SignedEventRecord* signed_record, PublishResult* published)
  -> core::Status
{
  return SignAndPublishEvent(
    std::move(envelope), identity, dht, signed_record, published);
}

auto PublishEvent(core::EventEnvelope envelope,
  EventPublisherIdentity const& identity, MainlineEventDht& dht,
  core::SignedEventRecord* signed_record, PublishResult* published)
  -> core::Status
{
  return SignAndPublishEvent(
    std::move(envelope), identity, dht, signed_record, published);
}

auto QueryEvents(
  EventQuery query, MemoryEventDht const& dht, EventQueryResult& result)
  -> core::Status
{
  result.records = dht.Query(std::move(query));
  result.peers.clear();
  return core::Status::Success();
}

auto QueryEvents(
  EventQuery query, MainlineEventDht& dht, EventQueryResult& result) -> core::Status
{
  return dht.Query(std::move(query), result);
}

auto DeriveInfoHash(std::string const& deterministic_key) -> kademlia::Node::IdType
{
  auto const seed = core::MakeId(deterministic_key);
  auto bytes = std::array<std::uint8_t, kademlia::Node::IdType::Size()> {};
  std::copy_n(seed.Data(), bytes.size(), bytes.begin());
  return kademlia::Node::IdType(std::span<std::uint8_t const>(bytes));
}

} // namespace blocxxi::p2p
