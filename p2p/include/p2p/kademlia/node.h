//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <p2p/blocxxi_p2p_export.h>

#include <chrono>
#include <string>
#include <utility>

#include <crypto/hash.h>
#include <p2p/kademlia/endpoint.h>
#include <p2p/kademlia/parameters.h>

namespace blocxxi::p2p::kademlia {

/*!
 * @brief Represents a Kademlia node or a remote peer in the kademlia P2P
 * network.
 *
 * A node is identified by a 160 bit random hash. It communicates with other
 * nodes over the UDP protocol using and IPv4/IPv6 endpoint.
 *
 * An Kademlia node can be described with a URL scheme "knode".
 * The hexadecimal node ID is encoded in the username portion of the URL,
 * separated from the host by an @ sign. The hostname can only be given as an
 * IP address, DNS domain names are not allowed. The port in the host name
 * section is the UDP discovery and P2P protocol port.
 *
 * The enode url scheme is used by the Node discovery protocol and can be used
 * in the bootstrap nodes argument to the Engine AddBootsrapNode() method.
 */
class BLOCXXI_P2P_API Node {
public:
  /// @name Type shortcuts
  //@{
  using IdType = blocxxi::crypto::Hash160;
  //@}

  /// @name Constructors etc.
  //@{
  Node() = default;

  /*!
   * @brief Create a Node object from the given id, IP address and port number.
   *
   * @param [in] node_id the 160 bit hash used as an ID for this node. Ensuring
   * randomness of the id is important for the proper working of the kademlia
   * P2P network.
   * @param [in] ip_address an IPv4 or IPv6 numeric IP address. Host names are
   * not acceptable.
   * @param [in] port_number the UDP discovery port number for this node's
   * endpoint.
   */
  Node(
      IdType node_id, std::string const &ip_address, unsigned short port_number)
      : node_id_{std::move(node_id)}, endpoint_{ip_address, port_number} {
  }

  /*!
   * @brief Create a Node object from the given id and endpoint.
   *
   * @param [in] node_id id the 160 bit hash used as an ID for this node.
   * Ensuring randomness of the id is important for the proper working of the
   * kademlia P2P network.
   * @param [in] endpoint UDP discovery and P2P RPC endpoint.
   */
  Node(IdType node_id, IpEndpoint endpoint)
      : node_id_(std::move(node_id)), endpoint_(std::move(endpoint)) {
  }

  /// Copy constructor.
  Node(const Node &other)
      : node_id_(other.node_id_), endpoint_(other.endpoint_),
        failed_requests_count_(other.failed_requests_count_),
        last_seen_time_(other.last_seen_time_) {
  }

  /// Assignment.
  auto operator=(Node const &rhs) -> Node & {
    // Effective C++ item 25 - call swap without namespace qualification
    using std::swap;
    auto tmp(rhs);
    swap(*this, tmp);
    return *this;
  }

  /// Move constructor.
  Node(Node &&other) noexcept
      : node_id_(std::move(other.node_id_)),
        endpoint_(std::move(other.endpoint_)),
        failed_requests_count_(other.failed_requests_count_),
        last_seen_time_(other.last_seen_time_) {
    // we don't move the url_, instead delete it
    delete other.url_;
    other.url_ = nullptr;
  }

  /// Move assignment.
  auto operator=(Node &&rhs) noexcept -> Node & {
    if (this != &rhs) {
      delete url_;
      url_ = nullptr;
      node_id_ = std::move(rhs.node_id_);
      endpoint_ = std::move(rhs.endpoint_);
      failed_requests_count_ = rhs.failed_requests_count_;
      last_seen_time_ = rhs.last_seen_time_;
    }
    return *this;
  }

  /// Destructor.
  ~Node() {
    delete url_;
  }

  /*!
   * @brief Create a node object from a knode URL string.
   *
   * @param url a valid knode URL in the form 'knode://id\@address:port'.
   * @return a node initialized with the id, and endpoint in the given url.
   */
  static auto FromUrlString(std::string const &url) -> Node;
  //@}

  /// @name Operator overloads
  //@{
  friend auto operator==(const Node &lhs, const Node &rhs) -> bool {
    return (lhs.Id() == rhs.Id()) || (lhs.Endpoint() == rhs.Endpoint());
  }
  //@}

  /// @name Observers
  //@{

  /// The node ID.
  auto Id() const -> IdType const & {
    return node_id_;
  }

  /// The node endpoint (addres/port).
  auto Endpoint() const -> IpEndpoint const & {
    return endpoint_;
  }

  /// The number of times this node failed to respond timely to communication
  /// requests. After NODE_FAILED_COMMS_BEFORE_STALE failures, the node is
  /// marked as stale.
  auto FailuresCount() const -> int {
    return failed_requests_count_;
  }

  /// Check if this node is stale.
  auto IsStale() const -> bool {
    return failed_requests_count_ == NODE_FAILED_COMMS_BEFORE_STALE;
  }

  /// Check if this node is questionable. A node becomes questionable if it has
  /// not been active for more than NODE_INACTIVE_TIME_BEFORE_QUESTIONABLE.
  auto IsQuestionable() const -> bool {
    return (std::chrono::steady_clock::now() - last_seen_time_) >
           NODE_INACTIVE_TIME_BEFORE_QUESTIONABLE;
  }

  /// Return the distance from this node to the given node.
  auto DistanceTo(Node const &node) const -> IdType {
    return DistanceTo(node.Id());
  }

  /// Return the distance from this node to the given ID.
  auto DistanceTo(blocxxi::crypto::Hash160 const &hash) const -> IdType {
    return node_id_ ^ hash;
  }

  /// Return the number 0 <= n < 160 where 2**n <= distance(a, b) < 2**(n+1), or
  ///-1 if the two nodes have the same ID.
  /// The value that is returned is the number trailing bits after the common
  /// prefix of a and b. If the first bits are different, that would be 159.
  auto LogDistanceTo(Node const &node) const -> size_t {
    return LogDistanceTo(node.Id());
  }

  /// @see LogDistance(Node const &)
  auto LogDistanceTo(blocxxi::crypto::Hash160 const &hash) const -> size_t;
  //@}

  /// @name Node state management
  //@{
  /// Increment the number of failed communications with this node.
  void IncFailuresCount() {
    ++failed_requests_count_;
  }
  //@}

  /// @name Serialization support
  //@{
  auto Id() -> IdType & {
    return node_id_;
  }
  auto Endpoint() -> IpEndpoint & {
    return endpoint_;
  }
  //@}

  /// Return a string representation of this node for debugging.
  auto ToString() const -> std::string const &;

private:
  /// The node ID (Hash 160)
  IdType node_id_;
  /// The node endpoint (address/port of its corresponding peer).
  IpEndpoint endpoint_;

  /// The URL representation of the Node. Initialized on demand when a call to
  /// ToString() is first made.
  mutable std::string *url_{nullptr};

  /// The number of times a communication with the peer represented by this
  /// node has failed (i.e. timed out). This number if incremented every time
  /// a request to this peer times out. It is reset to zero if a request is
  /// successful. The peer if considered to be stale (candidate for removal
  /// from the routing table) after NODE_FAILED_COMMS_BEFORE_STALE failures.
  int failed_requests_count_{0};

  /// The time point at which this node was last active (we received a request
  /// or response from this node)
  std::chrono::steady_clock::time_point last_seen_time_;
};

inline auto Distance(Node const &first_node, Node const &second_node)
    -> Node::IdType {
  return first_node.DistanceTo(second_node);
}

inline auto Distance(Node const &node, blocxxi::crypto::Hash160 const &hash)
    -> Node::IdType {
  return node.DistanceTo(hash);
}

inline auto Distance(Node::IdType const &ida, Node::IdType const &idb)
    -> Node::IdType {
  return ida ^ idb;
}

inline auto LogDistance(Node const &first_node, Node const &second_node)
    -> size_t {
  return first_node.LogDistanceTo(second_node);
}
inline auto LogDistance(const Node &node, blocxxi::crypto::Hash160 const &hash)
    -> size_t {
  return node.LogDistanceTo(hash);
}

inline auto operator!=(const Node &lhs, const Node &rhs) -> bool {
  return !(lhs == rhs);
}

/// Dump the url string of the node to the stream.
inline auto operator<<(std::ostream &out, Node const &node) -> std::ostream & {
  out << node.ToString();
  return out;
}

} // namespace blocxxi::p2p::kademlia
