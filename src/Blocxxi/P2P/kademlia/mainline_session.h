//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/P2P/kademlia/mainline_node.h>

namespace blocxxi::p2p::kademlia {

class MainlineSession final {
public:
  using QueryCallback = MainlineDhtNode::QueryCallback;
  using BootstrapCallback = MainlineDhtNode::BootstrapCallback;
  using ResponseCallback = MainlineDhtNode::ResponseCallback;

  explicit MainlineSession(MainlineDhtNode&& node)
    : node_(std::move(node))
  {
  }

  MainlineSession(MainlineSession const&) = delete;
  auto operator=(MainlineSession const&) -> MainlineSession& = delete;

  MainlineSession(MainlineSession&&) noexcept = default;
  auto operator=(MainlineSession&&) noexcept -> MainlineSession& = delete;

  ~MainlineSession() = default;

  void AddBootstrapNode(std::string endpoint)
  {
    node_.AddBootstrapNode(std::move(endpoint));
  }

  void OnQuery(QueryCallback callback) { node_.OnQuery(std::move(callback)); }

  void OnBootstrapSuccess(BootstrapCallback callback)
  {
    node_.OnBootstrapSuccess(std::move(callback));
  }

  void Start() { node_.Start(); }
  void Stop() { node_.Stop(); }

  void AsyncPing(IpEndpoint const& destination, ResponseCallback callback)
  {
    node_.AsyncPing(destination, std::move(callback));
  }

  void AsyncFindNode(Node::IdType const& target, IpEndpoint const& destination,
    ResponseCallback callback)
  {
    node_.AsyncFindNode(target, destination, std::move(callback));
  }

  void AsyncGetPeers(Node::IdType const& info_hash,
    IpEndpoint const& destination, ResponseCallback callback)
  {
    node_.AsyncGetPeers(info_hash, destination, std::move(callback));
  }

  void AsyncAnnouncePeer(Node::IdType const& info_hash, std::string token,
    std::uint16_t port, bool implied_port, IpEndpoint const& destination,
    ResponseCallback callback)
  {
    node_.AsyncAnnouncePeer(info_hash, std::move(token), port, implied_port,
      destination, std::move(callback));
  }

  void AsyncSampleInfohashes(Node::IdType const& target,
    IpEndpoint const& destination, ResponseCallback callback)
  {
    node_.AsyncSampleInfohashes(target, destination, std::move(callback));
  }

  [[nodiscard]] auto Self() const -> Node const& { return node_.Self(); }

private:
  MainlineDhtNode node_;
};

} // namespace blocxxi::p2p::kademlia
