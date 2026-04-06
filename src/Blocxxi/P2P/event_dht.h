//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/P2P/api_export.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <Blocxxi/Core/event_record.h>
#include <Blocxxi/Core/result.h>
#include <Blocxxi/P2P/kademlia/node.h>

namespace blocxxi::p2p {

struct EventQuery {
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

[[nodiscard]] BLOCXXI_P2P_API auto DeriveInfoHash(
  std::string const& deterministic_key) -> kademlia::Node::IdType;

} // namespace blocxxi::p2p
