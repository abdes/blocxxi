//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <p2p/kademlia/routing.h>

#include <sstream> // for ToString() implementation

#include <common/compilers.h>
ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_GNUC_VERSION)
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#if defined(ASAP_CLANG_VERSION)
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif
#include <boost/multiprecision/cpp_int.hpp>
ASAP_DIAGNOSTIC_POP

namespace blocxxi::p2p::kademlia {

using boost::multiprecision::cpp_int_backend;
using boost::multiprecision::number;
using boost::multiprecision::unchecked;
using boost::multiprecision::unsigned_magnitude;

using uint161_t = number<cpp_int_backend<KEYSIZE_BITS + 1, KEYSIZE_BITS + 1,
    unsigned_magnitude, unchecked, void>>;

RoutingTable::RoutingTable(Node node, std::size_t ksize)
    : my_node_(std::move(node)), ksize_(ksize) {
  AddInitialBucket();
}

auto RoutingTable::NodesCount() const -> std::size_t {
  std::size_t total = 0;
  for (const auto &bucket : buckets_) {
    total += bucket.Size().first;
  }
  return total;
}

auto RoutingTable::BucketsCount() const -> std::size_t {
  return buckets_.size();
}

auto RoutingTable::Empty() const -> bool {
  return buckets_.front().Empty();
}

// Calculate the index of the bucket to receive the new node.
// Theoretically, node is supposed to go into the bucket covering the distance
// range [2^i, 2^i+1) which corresponds to index i which is also the same
// value returned by LogDistance().
//
// However, as we are splitting buckets as they become full instead of having
// 160 buckets, the target bucket index will be calculated as the min(i,
// num_buckets - 1), favoring to push new nodes to the bucket with the biggest
// distance from our node unless they are actually closer.
auto RoutingTable::GetBucketIndexFor(const Node::IdType &node_id) const
    -> std::size_t {
  // When we get here, the routing table should have been properly initialized
  // and thus has at least the initial bucket.
  const auto num_buckets = buckets_.size();
  // ASAP_ASSERT(num_buckets > 0);

  auto bucket = buckets_.begin();
  while (bucket != buckets_.end()) {
    if (bucket->CanHoldNode(node_id)) {
      auto bucket_index =
          static_cast<std::size_t>(std::distance(buckets_.begin(), bucket));
      ASLOG(trace, "{} belongs to bucket index={}", node_id.ToBitStringShort(),
          bucket_index);
      return bucket_index;
    }
    ++bucket;
  }
  // ASAP_ASSERT_FAIL();
  return num_buckets - 1;
}

auto RoutingTable::AddPeer(Node &&peer) -> bool {
  ASLOG(trace, "ADD CONTACT [ {} ]: {} / logdist: {}", this->ToString(),
      peer.Id().ToBitStringShort(), my_node_.LogDistanceTo(peer));
  // Don't add our node
  if (my_node_ == peer) {
    ASLOG(debug, "Unexpected attempt to add our node to the routing table.");
    return true;
  }
  const auto bucket_index = GetBucketIndexFor(peer.Id());
  auto bucket = buckets_.begin();
  std::advance(bucket, bucket_index);
  // bucket->DumpBucketToLog();

  // This will return true unless the bucket is full
  if (bucket->AddNode(std::move(peer))) {
    ASLOG(trace, "Buckets [ {}]", this->ToString());
    return true;
  }

  // Section 4.2
  // General rule for split: if the bucket has this routing table's node in its
  // range or if the depth is not congruent to 0 mod 5

  // Can we split?
  auto can_split = false;

  // auto bucket_depth_check = bucket->Depth() < DEPTH_B;
  auto shared_prefix_test =
      (bucket->Depth() < DEPTH_B) && ((bucket->Depth() % DEPTH_B) != 0);

  // Only the last bucket has in range the routing node (my_node_)
  auto bucket_has_in_range_my_node = (bucket_index == (buckets_.size() - 1));
  can_split |= bucket_has_in_range_my_node;
  ASLOG(trace, "|| bucket has in range my node? {} --> split={}",
      bucket_has_in_range_my_node, can_split);
  can_split |= shared_prefix_test;
  ASLOG(trace, "|| shared prefix size % {} != 0? {} --> split={}", DEPTH_B,
      shared_prefix_test, can_split);
  can_split &= (buckets_.size() < 160);
  ASLOG(trace, "&& we have {} less than 160 buckets? {} --> split={}",
      buckets_.size(), buckets_.size() < 160, can_split);

  // Should we split?
  // Don't split the first bucket unless it's the first split ever
  can_split &= !(buckets_.size() > 1 && bucket_index == 0);
  ASLOG(trace, "&& not the first bucket? {} --> split={}",
      !(buckets_.size() > 1 && bucket_index == 0), can_split);

  if (can_split) {
    std::pair<KBucket, KBucket> pair = bucket->Split();
    bucket = buckets_.insert(bucket, std::move(pair.second));
    bucket = buckets_.insert(bucket, std::move(pair.first));
    buckets_.erase(bucket + 2);
    DumpToLog();
    return true;
  }

  return false;
}

void RoutingTable::RemovePeer(const Node &peer) {
  const auto bucket_index = GetBucketIndexFor(peer.Id());
  auto bucket = buckets_.begin();
  std::advance(bucket, bucket_index);
  bucket->RemoveNode(peer);
}

auto RoutingTable::PeerTimedOut(Node const &peer) -> bool {
  // Find the peer in the routing table, starting from the last bucket as it is
  // most likely to contain the peer
  for (auto bucket = buckets_.rbegin(); bucket != buckets_.rend(); ++bucket) {
    for (auto bn = bucket->begin(); bn != bucket->end(); ++bn) {
      if (bn->Id() == peer.Id()) {
        bn->IncFailuresCount();
        ASLOG(debug, "node {} failed to respond for {} times", bn->ToString(),
            bn->FailuresCount());
        if (bn->IsStale()) {
          bucket->RemoveNode(bn);
          return true;
        }
        return false;
      }
    }
  }
  return false;
}

auto RoutingTable::FindNeighbors(Node::IdType const &node_id,
    std::size_t max_number) const -> std::vector<Node> {
  ASLOG(trace, "try to find up to {} neighbors for {}", max_number,
      node_id.ToBitStringShort());
  auto cmp = [&node_id](Node const &lhs, Node const &rhs) {
    return lhs.DistanceTo(node_id) < rhs.DistanceTo(node_id);
  };
  auto neighbors = std::set<Node, decltype(cmp)>(cmp);
  auto count = 0U;

  const auto bucket_index = GetBucketIndexFor(node_id);
  auto bucket = buckets_.begin();
  std::advance(bucket, bucket_index);
  auto left = bucket;
  auto right = (bucket != buckets_.end()) ? bucket + 1 : bucket;

  auto current_bucket = bucket;

  bool use_left = true;
  bool has_more = true;
  while (has_more) {
    has_more = false;
    for (auto const &neighbor : *current_bucket) {
      // Exclude the node
      if (neighbor.Id() != node_id) {
        ++count;
        ASLOG(debug, "found neighbor(count={}) {}", count,
            neighbor.Id().ToBitStringShort());
        neighbors.insert(neighbor);
        if (count == max_number) {
          goto Done;
        }
      } else {
        ASLOG(debug, "skip caller node from neighbors list");
      }
    }

    // If one bucket does not have enough, fill by alternating left and right
    // buckets until we collect the requested number of neighbors
    if (right == buckets_.end()) {
      use_left = true;
    }
    if (left != buckets_.begin()) {
      has_more = true;
      if (use_left) {
        --left;
        current_bucket = left;
        use_left = false;
        continue;
      }
    }
    if (right != buckets_.end()) {
      has_more = true;
      current_bucket = right;
      ++right;
    }
    use_left = true;
  }
Done:
  ASLOG(debug, "found {} neighbors out of {} non-replacement nodes I know",
      neighbors.size(), this->NodesCount());
  return {neighbors.begin(), neighbors.end()};
}

auto RoutingTable::ToString() const -> std::string {
  auto out = std::stringstream();
  for (const auto &bucket : buckets_) {
    out << bucket.Size().first << " ";
  }
  return out.str();
}

void RoutingTable::DumpToLog() const {
  ASLOG(trace, "START----------------------------------------------------");
  for (const auto &bucket : buckets_) {
    bucket.DumpBucketToLog();
  }
  ASLOG(trace, "END------------------------------------------------------");
}

auto operator<<(std::ostream &out, RoutingTable const &routing_table)
    -> std::ostream & {
  out << "[ ";
  for (const auto &bucket : routing_table) {
    out << bucket << " ";
  }
  out << "]";
  return out;
}

} // namespace blocxxi::p2p::kademlia
