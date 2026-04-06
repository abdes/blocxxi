//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/P2P/kademlia/mainline_node.h>

namespace blocxxi::p2p::kademlia {

MainlineDhtNode::MainlineDhtNode(asio::io_context& io_context, Node self,
  ChannelType::PointerType channel)
  : io_context_(io_context)
  , self_(std::move(self))
  , channel_(std::move(channel))
{
}

void MainlineDhtNode::AddBootstrapNode(std::string bootstrap_endpoint)
{
  bootstrap_nodes_.push_back(std::move(bootstrap_endpoint));
}

void MainlineDhtNode::OnQuery(QueryCallback callback)
{
  on_query_ = std::move(callback);
}

void MainlineDhtNode::OnBootstrapSuccess(BootstrapCallback callback)
{
  on_bootstrap_success_ = std::move(callback);
}

void MainlineDhtNode::Start()
{
  if (started_) {
    return;
  }
  started_ = true;
  ScheduleReceive();
  SendBootstrapQueries();
}

void MainlineDhtNode::Stop()
{
  started_ = false;
}

auto MainlineDhtNode::HashToBytes(Node::IdType const& id) const -> std::string
{
  return std::string(reinterpret_cast<char const*>(id.Data()), id.Size());
}

auto MainlineDhtNode::MakeResponse(
  std::string_view method, std::string transaction_id) const
  -> std::optional<KrpcMessage>
{
  auto values = blocxxi::codec::bencode::Value::DictionaryType {
    { "id", blocxxi::codec::bencode::Value(HashToBytes(self_.Id())) },
  };

  if (method == "find_node") {
    values.emplace(
      "nodes", blocxxi::codec::bencode::Value(EncodeCompactNode(self_)));
  } else if (method == "get_peers") {
    values.emplace(
      "nodes", blocxxi::codec::bencode::Value(EncodeCompactNode(self_)));
    values.emplace("token", blocxxi::codec::bencode::Value("blocxxi-token"));
  } else if (method == "announce_peer" || method == "ping") {
    // id only
  } else {
    return std::nullopt;
  }

  auto response = KrpcMessage {};
  response.transaction_id_ = std::move(transaction_id);
  response.version_ = "BX01";
  response.payload_ = KrpcResponse { std::move(values) };
  return response;
}

auto MainlineDhtNode::MakeBootstrapQuery(std::string transaction_id) const
  -> KrpcMessage
{
  auto query = KrpcMessage {};
  query.transaction_id_ = std::move(transaction_id);
  query.version_ = "BX01";
  query.payload_ = KrpcQuery {
    "find_node",
    {
      { "id", blocxxi::codec::bencode::Value(HashToBytes(self_.Id())) },
      { "target", blocxxi::codec::bencode::Value(HashToBytes(self_.Id())) },
    },
  };
  return query;
}

void MainlineDhtNode::SendBootstrapQueries()
{
  auto tx_index = 0U;
  for (auto const& bootstrap : bootstrap_nodes_) {
    auto const separator = bootstrap.rfind(':');
    if (separator == std::string::npos) {
      throw std::invalid_argument("bad bootstrap endpoint: " + bootstrap);
    }

    auto const host = bootstrap.substr(0, separator);
    auto const service = bootstrap.substr(separator + 1);
    auto endpoints = ChannelType::ResolveEndpoint(io_context_, host, service);
    if (endpoints.empty()) {
      throw std::invalid_argument("no endpoint resolved for " + bootstrap);
    }

    auto transaction_id
      = std::string("b") + static_cast<char>('0' + (tx_index % 10U));
    ++tx_index;
    pending_bootstraps_.emplace(transaction_id, bootstrap);

    auto encoded = Encode(MakeBootstrapQuery(transaction_id));
    auto buffer = Buffer(encoded.begin(), encoded.end());
    channel_->AsyncSend(buffer, endpoints.front(), [](std::error_code const&) { });
  }
}

void MainlineDhtNode::ScheduleReceive()
{
  if (!started_) {
    return;
  }

  channel_->AsyncReceive(
    [this](std::error_code const& failure, IpEndpoint const& sender,
      BufferReader const& buffer) {
      if (failure) {
        if (started_) {
          ScheduleReceive();
        }
        return;
      }

      try {
        auto message = DecodeKrpc(std::string_view(
          reinterpret_cast<char const*>(buffer.data()), buffer.size()));

        if (std::holds_alternative<KrpcQuery>(message.payload_)) {
          auto const& query = std::get<KrpcQuery>(message.payload_);
          if (on_query_) {
            on_query_(query.method_, sender);
          }
          if (auto response
              = MakeResponse(query.method_, message.transaction_id_)) {
            auto encoded = Encode(*response);
            auto payload = Buffer(encoded.begin(), encoded.end());
            channel_->AsyncSend(
              payload, sender, [](std::error_code const&) { });
          }
        } else {
          auto found = pending_bootstraps_.find(message.transaction_id_);
          if (found != pending_bootstraps_.end()) {
            if (on_bootstrap_success_) {
              on_bootstrap_success_(found->second);
            }
            pending_bootstraps_.erase(found);
          }
        }
      } catch (...) {
        // ignore malformed packets
      }

      if (started_) {
        ScheduleReceive();
      }
    });
}

} // namespace blocxxi::p2p::kademlia
