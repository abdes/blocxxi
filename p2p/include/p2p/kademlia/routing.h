//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <p2p/blocxxi_p2p_api.h>

#include <common/compilers.h>

#include <deque>
#include <set>     // for std::set (neighbors)
#include <utility> // for std::pair

#include <logging/logging.h>
#include <p2p/kademlia/kbucket.h>
#include <p2p/kademlia/node.h>

ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_GNUC_VERSION)
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <boost/iterator/reverse_iterator.hpp>
ASAP_DIAGNOSTIC_POP

namespace blocxxi::p2p::kademlia {

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
class BLOCXXI_P2P_API RoutingTable : asap::logging::Loggable<RoutingTable> {
public:
  /// The logger id used for logging within this class.
  static constexpr const char *LOGGER_NAME = "p2p-kademlia";

  /*!
   * Represents an iterator over the buckets in the routing table.
   *
   * @tparam TValue value type.
   * @tparam TIterator Underlying bucket collection iterator type.
   */
  template <typename TValue, typename TIterator>
  class BucketIterator
      : public boost::iterator_adaptor<BucketIterator<TValue, TIterator>,
            TIterator, TValue, std::bidirectional_iterator_tag> {
  public:
    BucketIterator() : BucketIterator::iterator_adaptor_() {
    }
    explicit BucketIterator(TIterator node)
        : BucketIterator::iterator_adaptor_(node) {
    }

    /// Conversion constructor mainly between const and non-const iterators.
    template <class OtherValue>
    BucketIterator(BucketIterator<OtherValue, TIterator> const &other,
        std::enable_if_t<
            std::is_convertible_v<OtherValue *, TValue *>> /*unused*/)
        : BucketIterator::iterator_adaptor_(other.base()) {
    }
  };

  using iterator = BucketIterator<KBucket, std::deque<KBucket>::iterator>;
  using const_iterator =
      BucketIterator<KBucket const, std::deque<KBucket>::const_iterator>;
  using reverse_iterator = boost::reverse_iterator<iterator>;
  using const_reverse_iterator = boost::reverse_iterator<const_iterator>;

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
  auto operator=(const RoutingTable &) -> RoutingTable & = delete;
  /// Movable
  RoutingTable(RoutingTable &&) = default;
  /// Move assignable
  auto operator=(RoutingTable &&) -> RoutingTable & = default;
  /// Default destructor
  ~RoutingTable() = default;
  //@}

  /// @name Iteration
  //@{
  auto begin() noexcept -> iterator {
    return iterator(buckets_.begin());
  }
  [[nodiscard]] auto begin() const noexcept -> const_iterator {
    return const_iterator(buckets_.cbegin());
  }
  auto end() noexcept -> iterator {
    return iterator(buckets_.end());
  }
  [[nodiscard]] auto end() const noexcept -> const_iterator {
    return const_iterator(buckets_.cend());
  }

  auto rbegin() noexcept -> reverse_iterator {
    return reverse_iterator(end());
  }
  [[nodiscard]] auto rbegin() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator(cend());
  }
  auto rend() noexcept -> reverse_iterator {
    return reverse_iterator(begin());
  }
  [[nodiscard]] auto rend() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator(cbegin());
  }

  [[nodiscard]] auto cbegin() const noexcept -> const_iterator {
    return const_iterator(buckets_.cbegin());
  }
  [[nodiscard]] auto cend() const noexcept -> const_iterator {
    return const_iterator(buckets_.cend());
  }
  [[nodiscard]] auto crbegin() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator(cend());
  }
  [[nodiscard]] auto crend() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator(cbegin());
  }
  //@}

  /// @name Observers
  //@{

  /// Return this routing table routing node.
  [[nodiscard]] auto ThisNode() const -> const Node & {
    return my_node_;
  }

  /// Return the number of nodes in the routing table (excluding any nodes
  /// stored in buckets as replacements).
  [[nodiscard]] auto NodesCount() const -> std::size_t;

  /// Return the number of buckets in this routing table.
  [[nodiscard]] auto BucketsCount() const -> std::size_t;

  /// Check if this routing table is empty (does not contain any node).
  [[nodiscard]] auto Empty() const -> bool;
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
  auto AddPeer(Node &&peer) -> bool;

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
  auto PeerTimedOut(Node const &peer) -> bool;

  /// Find the closest \em k nodes to the given id. If the routing table does
  /// not have enough nodes to satisfy k, all nodes will be returned.
  [[nodiscard]] auto FindNeighbors(Node::IdType const &node_id) const
      -> std::vector<Node> {
    return FindNeighbors(node_id, ksize_);
  }

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
   * @param [in] node_id the id for which neighbors are to be found.
   * @param [in] max_number the number of neighbors to find, unless the routing
   * table does not have enough nodes.
   *
   * @return up to max_number nodes representing the closest nodes to the given
   * id known by this routing table.
   */
  [[nodiscard]] auto FindNeighbors(Node::IdType const &node_id,
      std::size_t max_number) const -> std::vector<Node>;

  // TODO(Abdessattar): refactor this
  [[nodiscard]] auto GetBucketIndexFor(const Node::IdType &node) const
      -> size_t;

  /// Return a string representation of this routing table for debugging.
  [[nodiscard]] auto ToString() const -> std::string;

  /// Dump the contents of the routing table to logs.
  void DumpToLog() const;

private:
  /// Create the initial bucket (empty) for this routing table.
  void AddInitialBucket() {
    buckets_.push_front(KBucket(my_node_, 0, ksize_));
  }

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
auto operator<<(std::ostream &out, RoutingTable const &routing_table)
    -> std::ostream &;

} // namespace blocxxi::p2p::kademlia
