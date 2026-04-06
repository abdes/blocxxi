//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/P2P/kademlia/mainline_node.h>

namespace blocxxi::p2p::kademlia {

namespace {

auto RequireString(KrpcQuery const& query, std::string_view key) -> std::string
{
  auto found = query.arguments_.find(key);
  if (found == query.arguments_.end()) {
    throw std::invalid_argument("missing query string argument");
  }
  return found->second.AsString();
}

auto FindInteger(KrpcQuery const& query, std::string_view key)
  -> std::optional<std::int64_t>
{
  auto found = query.arguments_.find(key);
  if (found == query.arguments_.end()) {
    return std::nullopt;
  }
  return found->second.AsInteger();
}

} // namespace

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

void MainlineDhtNode::AsyncPing(
  IpEndpoint const& destination, ResponseCallback callback)
{
  SendQuery(
    KrpcQuery {
      "ping",
      {
        { "id", blocxxi::codec::bencode::Value(HashToBytes(self_.Id())) },
      },
    },
    destination, std::move(callback));
}

void MainlineDhtNode::AsyncFindNode(Node::IdType const& target,
  IpEndpoint const& destination, ResponseCallback callback)
{
  SendQuery(
    KrpcQuery {
      "find_node",
      {
        { "id", blocxxi::codec::bencode::Value(HashToBytes(self_.Id())) },
        { "target",
          blocxxi::codec::bencode::Value(HashToBytes(target)) },
      },
    },
    destination, std::move(callback));
}

void MainlineDhtNode::AsyncGetPeers(Node::IdType const& info_hash,
  IpEndpoint const& destination, ResponseCallback callback)
{
  SendQuery(
    KrpcQuery {
      "get_peers",
      {
        { "id", blocxxi::codec::bencode::Value(HashToBytes(self_.Id())) },
        { "info_hash",
          blocxxi::codec::bencode::Value(HashToBytes(info_hash)) },
      },
    },
    destination, std::move(callback));
}

void MainlineDhtNode::AsyncAnnouncePeer(Node::IdType const& info_hash,
  std::string token, std::uint16_t port, bool implied_port,
  IpEndpoint const& destination, ResponseCallback callback)
{
  auto arguments = blocxxi::codec::bencode::Value::DictionaryType {
    { "id", blocxxi::codec::bencode::Value(HashToBytes(self_.Id())) },
    { "info_hash",
      blocxxi::codec::bencode::Value(HashToBytes(info_hash)) },
    { "token", blocxxi::codec::bencode::Value(std::move(token)) },
    { "port", blocxxi::codec::bencode::Value(static_cast<std::int64_t>(port)) },
  };
  if (implied_port) {
    arguments.emplace("implied_port", blocxxi::codec::bencode::Value(1));
  }

  SendQuery(KrpcQuery { "announce_peer", std::move(arguments) }, destination,
    std::move(callback));
}

void MainlineDhtNode::AsyncSampleInfohashes(Node::IdType const& target,
  IpEndpoint const& destination, ResponseCallback callback)
{
  SendQuery(
    KrpcQuery {
      "sample_infohashes",
      {
        { "id", blocxxi::codec::bencode::Value(HashToBytes(self_.Id())) },
        { "target",
          blocxxi::codec::bencode::Value(HashToBytes(target)) },
      },
    },
    destination, std::move(callback));
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
  for (auto& [transaction_id, timer] : pending_request_timers_) {
    (void)transaction_id;
    timer->cancel();
  }
  pending_request_timers_.clear();
  pending_requests_.clear();
}

auto MainlineDhtNode::HashToBytes(Node::IdType const& id) const -> std::string
{
  return std::string(reinterpret_cast<char const*>(id.Data()), id.Size());
}

auto MainlineDhtNode::MakeResponse(
  std::string_view method, KrpcQuery const& query, IpEndpoint const& sender,
  std::string transaction_id)
  -> std::optional<KrpcMessage>
{
  auto values = blocxxi::codec::bencode::Value::DictionaryType {
    { "id", blocxxi::codec::bencode::Value(HashToBytes(self_.Id())) },
  };

  if (method == "find_node") {
    values.emplace(
      "nodes", blocxxi::codec::bencode::Value(EncodeCompactNode(self_)));
  } else if (method == "get_peers") {
    auto const info_hash = RequireString(query, "info_hash");
    auto token = MakeToken(sender);
    issued_tokens_[sender.ToString()] = token;
    values.emplace("token", blocxxi::codec::bencode::Value(token));

    auto announced = announced_peers_.find(info_hash);
    if (announced != announced_peers_.end() && !announced->second.empty()) {
      auto compact_peers = blocxxi::codec::bencode::Value::ListType {};
      for (auto const& peer : announced->second) {
        compact_peers.emplace_back(
          blocxxi::codec::bencode::Value(EncodeCompactPeer(peer)));
      }
      values.emplace("values", blocxxi::codec::bencode::Value(compact_peers));
    } else {
      values.emplace(
        "nodes", blocxxi::codec::bencode::Value(EncodeCompactNode(self_)));
    }
  } else if (method == "sample_infohashes") {
    values = MakeSampleInfohashesPayload();
  } else if (method == "announce_peer") {
    auto const token = RequireString(query, "token");
    auto found = issued_tokens_.find(sender.ToString());
    if (found == issued_tokens_.end() || found->second != token) {
      return MakeErrorResponse(
        203, "invalid announce_peer token", std::move(transaction_id));
    }
    RecordAnnouncedPeer(RequireString(query, "info_hash"), sender, query);
  } else if (method == "ping") {
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

auto MainlineDhtNode::MakeErrorResponse(
  std::int64_t code, std::string message, std::string transaction_id) const
  -> KrpcMessage
{
  auto response = KrpcMessage {};
  response.transaction_id_ = std::move(transaction_id);
  response.version_ = "BX01";
  response.payload_ = KrpcError { code, std::move(message) };
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

auto MainlineDhtNode::MakeToken(IpEndpoint const& sender) const -> std::string
{
  return sender.ToString();
}

void MainlineDhtNode::SendQuery(
  KrpcQuery query, IpEndpoint const& destination, ResponseCallback callback)
{
  auto transaction_id = NextTransactionId();
  auto message = KrpcMessage {};
  message.transaction_id_ = transaction_id;
  message.version_ = "BX01";
  message.payload_ = std::move(query);

  pending_requests_.emplace(transaction_id, std::move(callback));
  auto timer = std::make_unique<asio::steady_timer>(io_context_);
  timer->expires_after(REQUEST_TIMEOUT);
  timer->async_wait([this, transaction_id](std::error_code const& failure) {
    if (failure == asio::error::operation_aborted) {
      return;
    }
    auto request = pending_requests_.find(transaction_id);
    if (request != pending_requests_.end()) {
      auto response = KrpcMessage {};
      request->second(std::make_error_code(std::errc::timed_out), response);
      pending_requests_.erase(request);
    }
    pending_request_timers_.erase(transaction_id);
  });
  pending_request_timers_.emplace(transaction_id, std::move(timer));

  auto encoded = Encode(message);
  auto payload = Buffer(encoded.begin(), encoded.end());
  channel_->AsyncSend(payload, destination,
    [this, transaction_id](std::error_code const& failure) {
      if (!failure) {
        return;
      }
      auto found = pending_requests_.find(transaction_id);
      if (found != pending_requests_.end()) {
        auto response = KrpcMessage {};
        found->second(failure, response);
        pending_requests_.erase(found);
      }
      if (auto timer = pending_request_timers_.find(transaction_id);
          timer != pending_request_timers_.end()) {
        timer->second->cancel();
        pending_request_timers_.erase(timer);
      }
    });
}

auto MainlineDhtNode::NextTransactionId() -> std::string
{
  auto id = std::string("q");
  id.append(std::to_string(next_request_id_++));
  return id;
}

auto MainlineDhtNode::MakeSampleInfohashesPayload() const
  -> blocxxi::codec::bencode::Value::DictionaryType
{
  auto samples = std::string {};
  samples.reserve(announced_peers_.size() * Node::IdType::Size());
  for (auto const& [info_hash, peers] : announced_peers_) {
    if (!peers.empty()) {
      samples.append(info_hash);
    }
  }

  return {
    { "id", blocxxi::codec::bencode::Value(HashToBytes(self_.Id())) },
    { "interval", blocxxi::codec::bencode::Value(std::int64_t { 21600 }) },
    { "nodes", blocxxi::codec::bencode::Value(EncodeCompactNode(self_)) },
    { "num",
      blocxxi::codec::bencode::Value(
        static_cast<std::int64_t>(announced_peers_.size())) },
    { "samples", blocxxi::codec::bencode::Value(std::move(samples)) },
  };
}

void MainlineDhtNode::RecordAnnouncedPeer(
  std::string info_hash, IpEndpoint const& sender, KrpcQuery const& query)
{
  auto endpoint = sender;
  auto implied_port = FindInteger(query, "implied_port").value_or(0);
  if (implied_port == 0) {
    auto port = FindInteger(query, "port");
    if (!port.has_value()) {
      throw std::invalid_argument("announce_peer requires a port");
    }
    endpoint.Port(static_cast<std::uint16_t>(*port));
  }

  auto& peers = announced_peers_[info_hash];
  auto duplicate = std::find(peers.begin(), peers.end(), endpoint);
  if (duplicate == peers.end()) {
    peers.push_back(endpoint);
  }
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
              = MakeResponse(query.method_, query, sender, message.transaction_id_)) {
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
          } else {
            auto request = pending_requests_.find(message.transaction_id_);
            if (request != pending_requests_.end()) {
              if (auto timer = pending_request_timers_.find(message.transaction_id_);
                  timer != pending_request_timers_.end()) {
                timer->second->cancel();
                pending_request_timers_.erase(timer);
              }
              request->second(std::error_code {}, message);
              pending_requests_.erase(request);
            }
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
