//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <Blocxxi/P2P/api_export.h>
#include <Blocxxi/P2P/kademlia/asio.h>
#include <Blocxxi/P2P/kademlia/channel.h>
#include <Blocxxi/P2P/kademlia/krpc.h>
#include <Blocxxi/P2P/kademlia/node.h>

namespace blocxxi::p2p::kademlia {

class BLOCXXI_P2P_API MainlineDhtNode {
public:
  using ChannelType = AsyncUdpChannel;
  using QueryCallback = std::function<void(
    std::string_view method, IpEndpoint const& sender)>;
  using BootstrapCallback = std::function<void(std::string const& bootstrap)>;

  MainlineDhtNode(asio::io_context& io_context, Node self,
    ChannelType::PointerType channel);

  MainlineDhtNode(MainlineDhtNode const&) = delete;
  auto operator=(MainlineDhtNode const&) -> MainlineDhtNode& = delete;

  MainlineDhtNode(MainlineDhtNode&&) noexcept = default;
  auto operator=(MainlineDhtNode&&) noexcept -> MainlineDhtNode& = delete;

  ~MainlineDhtNode() = default;

  void AddBootstrapNode(std::string bootstrap_endpoint);
  void OnQuery(QueryCallback callback);
  void OnBootstrapSuccess(BootstrapCallback callback);

  void Start();
  void Stop();

  [[nodiscard]] auto Self() const -> Node const& { return self_; }

private:
  [[nodiscard]] auto HashToBytes(Node::IdType const& id) const -> std::string;
  [[nodiscard]] auto MakeResponse(std::string_view method,
    KrpcQuery const& query, IpEndpoint const& sender,
    std::string transaction_id)
    -> std::optional<KrpcMessage>;
  [[nodiscard]] auto MakeErrorResponse(
    std::int64_t code, std::string message, std::string transaction_id) const
    -> KrpcMessage;
  [[nodiscard]] auto MakeBootstrapQuery(std::string transaction_id) const
    -> KrpcMessage;
  [[nodiscard]] auto MakeToken(IpEndpoint const& sender) const -> std::string;
  [[nodiscard]] auto MakeSampleInfohashesPayload() const
    -> blocxxi::codec::bencode::Value::DictionaryType;
  void RecordAnnouncedPeer(
    std::string info_hash, IpEndpoint const& sender, KrpcQuery const& query);
  void SendBootstrapQueries();
  void ScheduleReceive();

  asio::io_context& io_context_;
  Node self_;
  ChannelType::PointerType channel_;
  std::vector<std::string> bootstrap_nodes_;
  std::unordered_map<std::string, std::string> pending_bootstraps_;
  std::unordered_map<std::string, std::string> issued_tokens_;
  std::map<std::string, std::vector<IpEndpoint>, std::less<>> announced_peers_;
  QueryCallback on_query_;
  BootstrapCallback on_bootstrap_success_;
  bool started_ { false };
};

} // namespace blocxxi::p2p::kademlia
