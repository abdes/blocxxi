//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <chrono>
#include <deque>
#include <utility> // for std::pair

#include <common/compilers.h>
ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_GNUC_VERSION)
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#if defined(ASAP_CLANG_VERSION)
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma clang diagnostic ignored "-Wreserved-macro-identifier"
#endif
#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/iterator/reverse_iterator.hpp>
ASAP_DIAGNOSTIC_POP

#include <logging/logging.h>

#include <p2p/blocxxi_p2p_api.h>
#include <p2p/kademlia/node.h>
#include <p2p/kademlia/parameters.h>

namespace blocxxi::p2p::kademlia {

/*!
 * Represents a kademlia bucket of nodes within the kademlia routing table.
 *
 * As nodes are encountered on the kademlia network, they are added to lists.
 * This includes store and retrieval operations and even helping other nodes to
 * find a key. Every node encountered will be considered for inclusion in the
 * lists. In the Kademlia literature, the lists are referred to as k-buckets.
 *
 * @see https://en.wikipedia.org/wiki/Kademlia
 * @see https://pdos.csail.mit.edu/~petar/papers/maymounkov-kademlia-lncs.pdf
 */
class BLOCXXI_P2P_API KBucket : public asap::logging::Loggable<KBucket> {
public:
  /// The logger id used for logging within this class.
  static constexpr const char *LOGGER_NAME = "p2p-kademlia";

  /*!
   * Represents an iterator over the nodes in a bucket.
   *
   * @tparam TValue value type.
   * @tparam TIterator Underlying node collection iterator type.
   */
  template <typename TValue, typename TIterator>
  class NodeIterator
      : public boost::iterator_adaptor<NodeIterator<TValue, TIterator>,
            TIterator, TValue, std::bidirectional_iterator_tag> {
  public:
    NodeIterator() : NodeIterator::iterator_adaptor_() {
    }
    explicit NodeIterator(TIterator node)
        : NodeIterator::iterator_adaptor_(node) {
    }

    /// Conversion constructor mainly between const and non-const iterators.
    template <class OtherValue>
    NodeIterator(NodeIterator<OtherValue, TIterator> const &other,
        std::enable_if_t<
            std::is_convertible_v<OtherValue *, TValue *>> /*unused*/)
        : NodeIterator::iterator_adaptor_(other.base()) {
    }
  };

  using iterator = NodeIterator<Node, std::deque<Node>::iterator>;
  using const_iterator =
      NodeIterator<Node const, std::deque<Node>::const_iterator>;
  using reverse_iterator = boost::reverse_iterator<iterator>;
  using const_reverse_iterator = boost::reverse_iterator<const_iterator>;

  /// @name Constructors etc.
  //@{
  /*!
   * @brief Create a KBucket and initialize it with the given arguments.
   *
   * @param [in] node this routing node.
   * @param [in] depth the bucket depth (the length of the prefix shared by all
   * nodes in the k-bucket's range)
   * @param [in] ksize the maximum number of nodes the bucket can hold.
   */
  KBucket(Node node, unsigned int depth, std::size_t ksize);
  /// Not copyable
  KBucket(const KBucket &) = delete;
  /// Not copyable
  auto operator=(const KBucket &) -> KBucket & = delete;
  /// Move assignable
  KBucket(KBucket &&) = default;
  /// Movable
  auto operator=(KBucket &&) -> KBucket & = default;
  /// Default destructor
  ~KBucket() = default;
  //@}

  /// @name Iteration
  //@{
  auto begin() noexcept -> iterator {
    return iterator(nodes_.begin());
  }
  auto begin() const noexcept -> const_iterator {
    return const_iterator(nodes_.cbegin());
  }
  auto end() noexcept -> iterator {
    return iterator(nodes_.end());
  }
  auto end() const noexcept -> const_iterator {
    return const_iterator(nodes_.cend());
  }

  auto rbegin() noexcept -> reverse_iterator {
    return reverse_iterator(end());
  }
  auto rbegin() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator(cend());
  }
  auto rend() noexcept -> reverse_iterator {
    return reverse_iterator(begin());
  }
  auto rend() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator(cbegin());
  }

  auto cbegin() const noexcept -> const_iterator {
    return const_iterator(nodes_.cbegin());
  }
  auto cend() const noexcept -> const_iterator {
    return const_iterator(nodes_.cend());
  }
  auto crbegin() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator(cend());
  }
  auto crend() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator(cbegin());
  }
  //@}

  /// @name Observers
  //@{

  /// Return a pair of values where the first value refers to the number of
  /// nodes in the bucket, while the second one refers to the number of nodes in
  /// the replacements list.
  auto Size() const -> std::pair<size_t, size_t>;

  /// Check if the bucket is empty (no nodes in the bucket).
  /// Naturally if a bucket is empty, it also does not have any replacement
  /// nodes.
  auto Empty() const -> bool;

  /// Check if the bucket cannot hold anymore active nodes. It can still store
  /// nodes as replacements though.
  auto Full() const -> bool;

  /*!
   * @brief Get the bucket depth.
   *
   * Section 4.2
   * The depth is the length of the prefix shared by all nodes in the k-bucket's
   * range.
   *
   * @return the bucket depth.
   */
  auto Depth() const -> unsigned int;

  /// Get the time duration since this bucket has been last updated (i.e. nodes
  /// have been added, removed or moved around).
  auto TimeSinceLastUpdated() const -> std::chrono::seconds;

  /// Check if this bucket's range can hold the given node, i.e. the node's id
  /// has the same prefix than this bucket.
  auto CanHoldNode(const Node::IdType &node) const -> bool;

  /// Get the shared prefix (bits) with the router node
  auto SharedPrefix() const -> std::string {
    return prefix_.to_string().substr(0, prefix_size_);
  }
  //@}

  /// @name Node accessors and manipulators
  //@{
  /// Get the least recently seen node in the bucket.
  auto LeastRecentlySeenNode() -> Node & {
    return nodes_.front();
  }
  /// Get the least recently seen node in the bucket.
  auto LeastRecentlySeenNode() const -> Node const & {
    return nodes_.front();
  }
  /// Select a random node from the bucket.
  auto SelectRandomNode() const -> Node const &;

  /*!
   * @brief Add the node to the bucket.
   *
   * Section 4.1
   * If the bucket is already full with \em k entries, the node places the new
   * contact in a \em replacement \em cache of nodes eligible to replace stale
   * k-bucket entries. The replacement cache is kept sorted by time last seen,
   * with the most recently seen entry having the highest priority as a
   * replacement candidate.
   *
   * @param node the node to be added to the bucket.
   * @return true if the node has been added to the bucket, false if the bucket
   * is full.
   */
  auto AddNode(Node &&node) -> bool;

  /*!
   * @brief Remove a node from the bucket.
   *
   * If the bucket has a node with the same id than the given node, it is
   * removed. Subsequently, as per Section 4.1, if the replacement cache is not
   * empty, a replacement node is moved into the bucket.
   *
   * @param node the node to be removed from the bucket.
   */
  void RemoveNode(const Node &node);

  /// Remove the node at the given iterator position from the bucket.
  void RemoveNode(iterator &node_iter);

  /*!
   * @brief Split the bucket in two halves.
   *
   * Split the bucket in two and dividing the current range in two equal ranges,
   * redistributing the nodes among the them based on their distance to the
   * upper and lower limits of the ranges.
   *
   * @return {one, two} a pair of KBucket where \em contains nodes which id is
   * within the lower half of the original KBucket range and \em two contains
   * the nodes which id is within the upper half of the original KBucket range.
   */
  auto Split() -> std::pair<KBucket, KBucket>;
  //@}

  /// Print debug logs with the bucket contents.
  void DumpBucketToLog() const;

private:
  /// This routing node.
  Node my_node_;

  /// An ordered list of nodes in the bucket, where the most recently seen node
  /// is at the tail of the list.
  /// Section 2.2
  std::deque<Node> nodes_{};

  /*!
   * @brief The replacement cache holds nodes that were discovered but the
   * k-bucket was full at that time.
   *
   * Section 4.1
   * The next time the node queries contacts in the k-bucket, any unresponsive
   * ones can be evicted and replaced with entries in the replacement cache. The
   * replacement cache is kept sorted by time last seen, with the most recently
   * seen entry (at the front) having the highest priority as a replacement
   * candidate.
   */
  std::deque<Node> replacement_nodes_{};
  /// Check if this bucket has replacement nodes.
  auto HasReplacements() const -> bool;

  /// @brief The bucket depth.
  /// Each bucket covers a keyspace region. E.g. from 0x0000simplified
  /// to 2 bytes to 0x0FFF for 1/16th of the keyspace. This can be
  /// expressed in CIDR-like masks, 0x0/4 (4 prefix bits). That's more
  /// or less the depth of a bucket.
  unsigned int depth_{0};
  /// The maximum number of nodes a \em k-bucket can hold.
  std::size_t ksize_;

  /// Common prefix with the routing node.
  std::bitset<KEYSIZE_BITS> prefix_;
  /// Size of the common prefix with the routing node.
  std::size_t prefix_size_{0};

  /// The time point at which this bucket was last updated.
  mutable std::chrono::steady_clock::time_point last_updated_;
  /// Update the time at which this bucket was last updated to be the current
  /// time.
  void TouchLastUpdated() const;
};

/// Output this bucket to the given stream.
auto operator<<(std::ostream &out, KBucket const &bucket) -> std::ostream &;

} // namespace blocxxi::p2p::kademlia
