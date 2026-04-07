//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/P2P/event_dht.h>

#include <algorithm>
#include <set>

#include <Blocxxi/Core/primitives.h>
#include <Blocxxi/P2P/kademlia/krpc.h>

namespace blocxxi::p2p {
namespace {

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
    }
    any_success = true;
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

auto DeriveInfoHash(std::string const& deterministic_key) -> kademlia::Node::IdType
{
  auto const seed = core::MakeId(deterministic_key);
  auto bytes = std::array<std::uint8_t, kademlia::Node::IdType::Size()> {};
  std::copy_n(seed.Data(), bytes.size(), bytes.begin());
  return kademlia::Node::IdType(std::span<std::uint8_t const>(bytes));
}

} // namespace blocxxi::p2p
