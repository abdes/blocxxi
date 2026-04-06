//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/P2P/event_dht.h>

#include <algorithm>

#include <Blocxxi/Core/primitives.h>

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

auto DeriveInfoHash(std::string const& deterministic_key) -> kademlia::Node::IdType
{
  auto const seed = core::MakeId(deterministic_key);
  auto bytes = std::array<std::uint8_t, kademlia::Node::IdType::Size()> {};
  std::copy_n(seed.Data(), bytes.size(), bytes.begin());
  return kademlia::Node::IdType(std::span<std::uint8_t const>(bytes));
}

} // namespace blocxxi::p2p
