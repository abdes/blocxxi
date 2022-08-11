//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <random> // for random selection of one node in a bucket
#include <set>    // for std::set (sorted nodes)

#include <p2p/kademlia/kbucket.h>

ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_GNUC_VERSION)
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include <boost/multiprecision/cpp_int.hpp>
ASAP_DIAGNOSTIC_POP

namespace blocxxi::p2p::kademlia {

using uint161_t = boost::multiprecision::number<
    boost::multiprecision::cpp_int_backend<KEYSIZE_BITS + 1, KEYSIZE_BITS + 1,
        boost::multiprecision::unsigned_magnitude,
        boost::multiprecision::unchecked, void>>;

KBucket::KBucket(Node node, unsigned int depth, std::size_t ksize)
    : my_node_(std::move(node)), depth_(depth), ksize_(ksize) {
  TouchLastUpdated();
}

auto KBucket::AddNode(Node &&node) -> bool {
  // If the bucket already has a node with the same id or with the same
  // endpoint, the new node replaces it and is placed at the front of the list
  // (most recently seen node)
  auto found = std::find_if(nodes_.begin(), nodes_.end(),
      [&node](Node const &bucket_node) { return (bucket_node == node); });
  if (found != nodes_.end()) {
    nodes_.erase(found);
    ASLOG(trace, "replacing existing node");
    // Section 2.2: Most recently seen node always at the tail of the list
    nodes_.emplace_back(std::move(node));
  } else if (!Full()) {
    // If this is a new node and we still have space available in the bucket,
    // push to tail (Most recently seen node)
    nodes_.emplace_back(std::move(node));
    ASLOG(trace, "previously unseen node added to the bucket");
  } else {
    // If no space available, store as a replacement node, at the tail (Most
    // recently seen node)
    // TODO(Abdessattar): Do we need to limit the size of replacement nodes list
    replacement_nodes_.emplace_back(std::move(node));
    ASLOG(trace, "bucket is full, node added to replacements");
    return false;
  }
  TouchLastUpdated();
  return true;
}

void KBucket::RemoveNode(Node const &node) {
  TouchLastUpdated();

  // There can only be one node at most matching the one we are looking for
  auto node_found =
      std::find_if(begin(), end(), [&node](Node const &bucket_node) {
        return (bucket_node.Id() == node.Id());
      });
  if (node_found != end()) {
    RemoveNode(node_found);
  } else {
    ASLOG(
        trace, "node being removed is not in the bucket, perhaps replacement");
    // Maybe it's a replacement node..
    auto replacement_found = std::find_if(replacement_nodes_.begin(),
        replacement_nodes_.end(), [&node](Node const &replacement_node) {
          return (replacement_node.Id() == node.Id());
        });
    if (replacement_found != replacement_nodes_.end()) {
      replacement_nodes_.erase(replacement_found);
      ASLOG(trace, "node is a replacement and it has been removed");
    } else {
      ASLOG(warn,
          "Node requested to be removed is not in the bucket and is not a "
          "replacement url={}",
          node.ToString());
    }
  }
}

void KBucket::RemoveNode(KBucket::iterator &node_iter) {
  TouchLastUpdated();

  nodes_.erase(node_iter.base());
  ASLOG(trace, "node removed from bucket");
  if (!replacement_nodes_.empty()) {
    ASLOG(trace, "moving one replacement node to the bucket");
    nodes_.emplace_back(std::move(replacement_nodes_.back()));
    replacement_nodes_.pop_back();
  }
}

auto KBucket::CanHoldNode(const Node::IdType &node_id) const -> bool {
  // check the node has the same prefix as the bucket
  auto id_bits = node_id.ToBitSet();
  auto last_bit = KEYSIZE_BITS - prefix_size_;
  for (auto bit = KEYSIZE_BITS - 1; bit >= last_bit; --bit) {
    if (prefix_[bit] ^ id_bits[bit]) {
      return false;
    }
  }
  return true;
}

auto KBucket::Split() -> std::pair<KBucket, KBucket> {
  ASLOG(trace,
      "Splitting bucket prefix= {} depth={} entries={} replacements={}",
      prefix_.to_string().substr(0, prefix_size_), depth_, nodes_.size(),
      replacement_nodes_.size());

  auto my_id = my_node_.Id().ToBitSet();
  auto one = KBucket(my_node_, depth_ + 1, ksize_);
  one.prefix_ = prefix_;
  auto two = KBucket(my_node_, depth_ + 1, ksize_);
  two.prefix_ = prefix_;
  if (my_id.test(KEYSIZE_BITS - 1 - prefix_size_)) {
    one.prefix_.reset(KEYSIZE_BITS - 1 - prefix_size_);
    two.prefix_.set(KEYSIZE_BITS - 1 - prefix_size_);
  } else {
    one.prefix_.set(KEYSIZE_BITS - 1 - prefix_size_);
    two.prefix_.reset(KEYSIZE_BITS - 1 - prefix_size_);
  }
  one.prefix_size_ = prefix_size_ + 1;
  two.prefix_size_ = prefix_size_ + 1;

  ASLOG(trace, "distributing {} nodes over the two new buckets", nodes_.size());
  // Distribute nodes among the two buckets based on their ID prefix.
  // After completed, ensure that most recently seen nodes from the bucket being
  // split (tail) are placed at the tail of the destination bucket.
  for (Node &node : nodes_) {
    KBucket &bucket = one.CanHoldNode(node.Id()) ? one : two;
    bucket.nodes_.push_back(std::move(node));
  }
  // Clear the the bucket that was just split
  nodes_.clear();

  // Distribute the replacement nodes over the two buckets. Let the routing
  // table process the replacements as usual following the regular algorithm. No
  // need to put replacements into the buckets now. This would make a clean
  // split and avoid interfering with the replacements processing.
  if (HasReplacements()) {
    ASLOG(trace, "distributing {} replacement nodes over the two new buckets",
        replacement_nodes_.size());
    for (Node &node : replacement_nodes_) {
      KBucket &bucket = one.CanHoldNode(node.Id()) ? one : two;
      bucket.replacement_nodes_.push_back(std::move(node));
    }
    // Clear the the bucket that was just split
    replacement_nodes_.clear();
  } else {
    ASLOG(trace, "No replacement nodes to distribute");
  }

  return std::make_pair(std::move(one), std::move(two));
}

struct NodeDistanceComparator {
  const Node &ref_node_;
  explicit NodeDistanceComparator(const Node &node) : ref_node_(node) {
  }

  auto operator()(const Node &lhs, const Node &rhs) const -> bool {
    return ref_node_.DistanceTo(lhs) < ref_node_.DistanceTo(rhs);
  }
};

void KBucket::DumpBucketToLog() const {
  ASLOG(trace,
      "depth: {} / prefix: {} / entries: {} / replacements: {} / ksize: {}",
      depth_, prefix_.to_string().substr(0, prefix_size_), nodes_.size(),
      replacement_nodes_.size(), ksize_);
  ASLOG(trace, "my node : {}...",
      my_node_.Id().ToBitSet().to_string().substr(0, 32));
  // Sort node Ids by distance from my_node_
  std::set<Node, NodeDistanceComparator> sorted(
      nodes_.begin(), nodes_.end(), NodeDistanceComparator(my_node_));
  for (auto const &node : sorted) {
    ASLOG(trace, "          {} / logdist: {} / fails: {}",
        node.Id().ToBitStringShort(), my_node_.LogDistanceTo(node),
        node.FailuresCount());
  }
}

auto KBucket::SelectRandomNode() const -> Node const & {
  // ASAP_ASSERT(!nodes_.empty());

  auto node_iter = nodes_.cbegin();
  std::random_device rand; // only used once to initialise (seed) engine
  std::mt19937 rng(
      rand()); // random-number engine used (Mersenne-Twister in this case)
  std::uniform_int_distribution<std::size_t> uni(
      0, nodes_.size() - 1); // guaranteed unbiased

  auto random_index = uni(rng);
  std::advance(node_iter, random_index);
  return *node_iter;
}

auto KBucket::Empty() const -> bool {
  return nodes_.empty();
}
auto KBucket::Full() const -> bool {
  return nodes_.size() == ksize_;
}
auto KBucket::Size() const -> std::pair<size_t, size_t> {
  return std::make_pair(nodes_.size(), replacement_nodes_.size());
}
auto KBucket::TimeSinceLastUpdated() const -> std::chrono::seconds {
  return std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now() - last_updated_);
}
auto KBucket::HasReplacements() const -> bool {
  return !replacement_nodes_.empty();
}
void KBucket::TouchLastUpdated() const {
  last_updated_ = std::chrono::steady_clock::now();
}
auto KBucket::Depth() const -> unsigned int {
  return depth_;
}

auto operator<<(std::ostream &out, KBucket const &bucket) -> std::ostream & {
  out << "entries:" << bucket.Size().first
      << " replacements:" << bucket.Size().second;
  return out;
}
} // namespace blocxxi::p2p::kademlia
