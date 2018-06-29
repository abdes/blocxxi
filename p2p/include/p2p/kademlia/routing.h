//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <chrono>
#include <deque>
#include <forward_list>
#include <set>      // for std::set (neighbors)
#include <utility>  // for std::pair

#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/iterator/reverse_iterator.hpp>

#include <common/logging.h>
#include <p2p/kademlia/kbucket.h>
#include <p2p/kademlia/node.h>
#include <p2p/kademlia/parameters.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

/*!
 * @brief A routing table to manage contacts for a node in a Kademlia
 * distributed hash table.
 *
 * This uses the Kademlia mechanism for routing messages in a peer-to-peer
 * network. Kademlia routing tables are structured so that nodes will have
 * detailed knowledge of the network close to them, and exponentially decreasing
 * knowledge further away.
 *
 * This implementation the Kademlia routing table as specified in section 2.4
 * of the paper along with the optimization in lookup and contact management.
 * Buckets are one-bit buckets with a total of 160 as in the initial
 * specification. Instead the routing table starts with a single initial bucket
 * covering the entire 160 bit range. Buckets are then split as specified in
 * section 4.2 (Accelerated lookups) to generate finer grained buckets as
 * needed.
 *
 * @see https://en.wikipedia.org/wiki/Kademlia
 * @see https://pdos.csail.mit.edu/~petar/papers/maymounkov-kademlia-lncs.pdf
 */
class RoutingTable : asap::logging::Loggable<asap::logging::Id::P2P_KADEMLIA> {
 public:
  /*!
   * Represents an iterator over the buckets in the routing table.
   *
   * @tparam TValue value type.
   * @tparam TIterator Underlying bucket collection iterator type.
   */
  template <typename TValue, typename TIterator>
  class BucketIterator
      : public boost::iterator_adaptor<BucketIterator<TValue, TIterator>,
                                       TIterator, TValue,
                                       std::bidirectional_iterator_tag> {
   public:
    BucketIterator() : BucketIterator::iterator_adaptor_() {}
    explicit BucketIterator(TIterator node)
        : BucketIterator::iterator_adaptor_(node) {}

    /// Conversion constructor mainly between const and non-const iterators.
    template <class OtherValue>
    BucketIterator(
        BucketIterator<OtherValue, TIterator> const &other,
        typename std::enable_if<
            std::is_convertible<OtherValue *, TValue *>::value>::type)
        : BucketIterator::iterator_adaptor_(other.base()) {}
  };

  using iterator = BucketIterator<KBucket, std::deque<KBucket>::iterator>;
  using const_iterator =
      BucketIterator<KBucket const, std::deque<KBucket>::const_iterator>;
  using reverse_iterator = boost::reverse_iterator<iterator>;
  using const_reverse_iterator = boost::reverse_iterator<const_iterator>;

 public:
  /// @name Constructors etc.
  //@{
  /*!
   * Create a routing table and initialize it with the given arguments.
   *
   * @param [in] node this routing node.
   * @param [in] ksize the maximum number of nodes a bucket in this routing
   * table can hold.
   */
  RoutingTable(Node node, std::size_t ksize);

  /// Not copyable
  RoutingTable(const RoutingTable &) = delete;
  /// Not assignable
  RoutingTable &operator=(const RoutingTable &) = delete;
  /// Movable
  RoutingTable(RoutingTable &&) = default;
  /// Move assignable
  RoutingTable &operator=(RoutingTable &&) = default;
  /// Default destructor
  ~RoutingTable() = default;
  //@}

  /// @name Iteration
  //@{
  iterator begin() noexcept { return iterator(buckets_.begin()); };
  const_iterator begin() const noexcept {
    return const_iterator(buckets_.cbegin());
  }
  iterator end() noexcept { return iterator(buckets_.end()); }
  const_iterator end() const noexcept {
    return const_iterator(buckets_.cend());
  }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(cend());
  }
  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(cbegin());
  }

  const_iterator cbegin() const noexcept {
    return const_iterator(buckets_.cbegin());
  }
  const_iterator cend() const noexcept {
    return const_iterator(buckets_.cend());
  }
  const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(cend());
  }
  const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator(cbegin());
  }
  //@}

  /// @name Observers
  //@{

  /// Return this routing table routing node.
  const Node &ThisNode() const { return my_node_; }

  /// Return the number of nodes in the routing table (excluding any nodes
  /// stored in buckets as replacements).
  std::size_t NodesCount() const;

  /// Return the number of buckets in this routing table.
  std::size_t BucketsCount() const;

  /// Check if this routing table is empty (does not contain any node).
  bool Empty() const;
  //@}

  /// @name Contact management
  //@{

  /*!
   * @brief Add a peer to the routing table.
   *
   * @param [in] peer the peer to be added.
   * @return true if the peer has been successfully added as an active node in
   * one of the routing table buckets; otherwise false. If the returned value
   * if false, the peer is guaranteed to have been added to a bucket
   * replacement list.
   */
  bool AddPeer(Node &&peer);

  /// Remove a contact from the routing table.
  void RemovePeer(const Node &peer);

  /*!
   * @brief Called when a communication with a peer has timed out to give a
   * chance to the routing table to do its housekeeping.
   *
   * Nodes in a routing table are not always good. In particular a nodes that
   * successively fails a certain number of times to respond to communication
   * requests becomes stale and is removed from the routing table.
   *
   * @param [in] peer the peer that just failed ro respond timely to a
   * communication request.
   *
   * @return true if the failing peer became stale and has been removed from the
   * routing table.
   */
  bool PeerTimedOut(Node const &peer);

  /// Find the closest \em k nodes to the given id. If the routing table does
  /// not have enough nodes to satisfy k, all nodes will be returned.
  std::vector<Node> FindNeighbors(Node::IdType const &id) const {
    return FindNeighbors(id, ksize_);
  };

  /*!
   * Find up to max_number neighbors to the given id in the routing table.
   *
   * Up to the requested max_number neighbors are collected from the closest
   * bucket to the given id. If the bucket does not have enough nodes, then more
   * neighbors are taking from the surrounding buckets (left and right) until
   * enough neighbors are collected. If the routing table does not have enough
   * nodes to provide max_number of neighbors, then it will return all of its
   * known nodes.
   *
   * @param [in] id the id for which neighbors are to be found.
   * @param [in] max_number the number of neigbors to find, unless the routing
   * table does not have enough nodes.
   *
   * @return up to max_number nodes representing the closest nodes to the given
   * id known by this routing table.
   */
  std::vector<Node> FindNeighbors(Node::IdType const &id,
                                  std::size_t max_number) const;

  // TODO: refactor this
  size_t GetBucketIndexFor(const Node::IdType &node) const;

  /// Return a string representation of this routing table for debugging.
  std::string ToString() const;

  /// Dump the contents of the routing table to logs.
  void DumpToLog() const;

 private:
  /// Create the initial bucket (empty) for this routing table.
  void AddInitialBucket() { buckets_.push_front(KBucket(my_node_, 0, ksize_)); }

  /// This routing node.
  Node my_node_;

  /// The maximum number of nodes a bucket in this routing table can hold. This
  /// corresponds to the Kademlia k constant.
  std::size_t ksize_;

  /// k-buckets used in this routing table, kept sorted from low id range to
  /// high id range. The last bucket in the list is always the bucket to which
  /// this routing node belongs.
  std::deque<KBucket> buckets_;
};

/// Output this routing table to the given stream.
std::ostream &operator<<(std::ostream &out, RoutingTable const &rt);

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi
