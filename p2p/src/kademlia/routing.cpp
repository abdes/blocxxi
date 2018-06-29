//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <sstream>  // for ToString() implementation

#include <boost/multiprecision/cpp_int.hpp>

#include <p2p/kademlia/routing.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

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

std::size_t RoutingTable::NodesCount() const {
  std::size_t total = 0;
  for (auto &kb : buckets_) total += kb.Size().first;
  return total;
}

std::size_t RoutingTable::BucketsCount() const { return buckets_.size(); }

bool RoutingTable::Empty() const { return buckets_.front().Empty(); }

// Calculate the index of the bucket to receive the new node.
// Theoretically, node is supposed to go into the bucket covering the distance
// range [2^i, 2^i+1) which corresponds to index i which is also the same
// value returned by LogDistance().
//
// However, as we are splitting buckets as they become full instead of having
// 160 buckets, the target bucket index will be calculated as the min(i,
// num_buckets - 1), favoring to push new nodes to the bucket with the biggest
// distance from our node unless they are actually closer.
std::size_t RoutingTable::GetBucketIndexFor(const Node::IdType &id) const {
  // When we get here, the routing table should have been properly initialized
  // and thus has at least the initial bucket.
  auto num_buckets = buckets_.size();
  ASAP_ASSERT(num_buckets > 0);

  auto bucket = buckets_.begin();
  while (bucket != buckets_.end()) {
    if (bucket->CanHoldNode(id)) {
      std::size_t bucket_index =
          static_cast<std::size_t>(std::distance(buckets_.begin(), bucket));
      ASLOG(trace, "{} belongs to bucket index={}", id.ToBitStringShort(),
            bucket_index);
      return bucket_index;
    }
    ++bucket;
  }
  ASAP_ASSERT_FAIL();
  return num_buckets - 1;
}

bool RoutingTable::AddPeer(Node &&node) {
  ASLOG(trace, "ADD CONTACT [ {} ]: {} / logdist: {}", this->ToString(),
        node.Id().ToBitStringShort(), my_node_.LogDistanceTo(node));
  // Don't add our node
  if (my_node_ == node) {
    ASLOG(debug, "Unexpected attempt to add our node to the routing table.");
    return true;
  }
  auto bucket_index = GetBucketIndexFor(node.Id());
  auto bucket = buckets_.begin();
  std::advance(bucket, bucket_index);
  // bucket->DumpBucketToLog();

  // This will return true unless the bucket is full
  if (bucket->AddNode(std::move(node))) {
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
  // Don't split the first bucket unles it's the first split ever
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

void RoutingTable::RemovePeer(const Node &node) {
  auto bucket_index = GetBucketIndexFor(node.Id());
  auto bucket = buckets_.begin();
  std::advance(bucket, bucket_index);
  bucket->RemoveNode(node);
}

bool RoutingTable::PeerTimedOut(Node const &peer) {
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
        } else {
          return false;
        }
      }
    }
  }
  return false;
}

std::vector<Node> RoutingTable::FindNeighbors(Node::IdType const &id,
                                              std::size_t k) const {
  ASLOG(trace, "try to find up to {} neighbors for {}", k,
        id.ToBitStringShort());
  auto cmp = [&id](Node const &a, Node const &b) {
    return a.DistanceTo(id) < b.DistanceTo(id);
  };
  auto neighbors = std::set<Node, decltype(cmp)>(cmp);
  auto count = 0U;

  auto bucket_index = GetBucketIndexFor(id);
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
      if (neighbor.Id() != id) {
        ++count;
        ASLOG(debug, "found neighbor(count={}) {}", count,
              neighbor.Id().ToBitStringShort());
        neighbors.insert(neighbor);
        if (count == k) goto Done;
      } else {
        ASLOG(debug, "skip caller node from neighbors list");
      }
    }

    // If one bucket does not have enough, fill by alternating left and right
    // buckets until we collect the requested number of neighbors
    if (right == buckets_.end()) use_left = true;
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
  return std::vector<Node>(neighbors.begin(), neighbors.end());
}

std::string RoutingTable::ToString() const {
  auto os = std::stringstream();
  for (auto &b : buckets_) {
    os << b.Size().first << " ";
  }
  return os.str();
}

void RoutingTable::DumpToLog() const {
  ASLOG(trace, "START----------------------------------------------------");
  for (auto &b : buckets_) {
    b.DumpBucketToLog();
  }
  ASLOG(trace, "END------------------------------------------------------");
}

std::ostream &operator<<(std::ostream &out, RoutingTable const &rt) {
  out << "[ ";
  for (auto &kb : rt) {
    out << kb << " ";
  }
  out << "]";
  return out;
}

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi
