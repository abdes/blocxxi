//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/P2P/kademlia/asio.h>
#include <Nova/Base/Compilers.h>

#include <Nova/Base/Logging.h>

#include <Blocxxi/P2P/kademlia/buffer.h>
#include <Blocxxi/P2P/kademlia/detail/bootstrap_procedure.h>
#include <Blocxxi/P2P/kademlia/detail/find_node_task.h>
#include <Blocxxi/P2P/kademlia/detail/find_value_task.h>
#include <Blocxxi/P2P/kademlia/detail/ping_node_task.h>
#include <Blocxxi/P2P/kademlia/detail/store_value_task.h>
#include <Blocxxi/P2P/kademlia/key.h>
#include <Blocxxi/P2P/kademlia/network.h>
#include <Blocxxi/P2P/kademlia/node.h>
#include <Blocxxi/P2P/kademlia/parameters.h>
#include <Blocxxi/P2P/kademlia/routing.h>
#include <Blocxxi/P2P/kademlia/value_store.h>

#include <vector>

namespace blocxxi::p2p::kademlia {

// TODO(Abdessattar): when new node is discovered, transfer to it key/values
// (where this node id is closer to the key than are the IDs of other nodes)

template <typename TRoutingTable, typename TNetwork> class Engine final {
public:
  /// The logger id used for logging within this class.
  static constexpr const char* LOGGER_NAME = "p2p-kademlia";

  // We need to import the internal logger retrieval method symbol in this
  // context to avoid g++ complaining about the method not being declared before
  // being used. THis is due to the fact that the current class is a template
  // class and that method does not take any template argument that will enable
  // the compiler to resolve it unambiguously.

  using DataType = std::vector<std::uint8_t>;
  using RoutingTableType = TRoutingTable;
  using NetworkType = TNetwork;
  using EndpointType = typename NetworkType::EndpointType;
  using ValueStoreType = ValueStore<KeyType, DataType>;

  Engine(asio::io_context& io_context, RoutingTableType&& routing_table,
    NetworkType&& network)
    : io_context_(io_context)
    , routing_table_(std::move(routing_table))
    , network_(std::move(network))
    , refresh_timer_(io_context)
  {
    LOG_F(1, "Creating Engine DONE");
  }

  Engine(Engine const&) = delete;
  auto operator=(Engine const&) -> Engine& = delete;

  Engine(Engine&&) noexcept = default;
  auto operator=(Engine&&) noexcept -> Engine& = default;

  ~Engine() { LOG_F(1, "Destroy Engine"); }

  [[nodiscard]] auto GetRoutingTable() const -> RoutingTableType const&
  {
    return routing_table_;
  }

  void AddBootstrapNode(const std::string& bnode_url)
  {
    AddBootstrapNode(Node::FromUrlString(bnode_url));
  }

  void AddBootstrapNode(Node&& bnode)
  {
    LOG_F(1, "adding bootstrap node at {}", bnode.Endpoint().ToString());
    routing_table_.AddPeer(std::move(bnode));
  }

  void Start()
  {
    LOG_F(1, "Engine Start: {}", routing_table_.ThisNode().ToString());
    network_.OnMessageReceived([this](auto&& endpoint, auto&& buffer) {
      HandleNewMessage(std::forward<decltype(endpoint)>(endpoint),
        std::forward<decltype(buffer)>(buffer));
    });

    network_.Start();

    if (!routing_table_.Empty()) {
      // Queue the bootstrapping as a delayed task
      asio::post(io_context_, [this]() { DiscoverNeighbors(); });
    } else {
      LOG_F(INFO, "engine started as a bootstrap node - empty routing table");
    }

    ScheduleBucketRefreshTimer();
  }

  void Stop()
  {
    LOG_F(1, "Engine Stop");
    refresh_timer_.cancel();
  }

  void ScheduleBucketRefreshTimer()
  {
    // Start the buckets refresh period timer
    LOG_F(1, "[REFRESH] periodic bucket refresh timer started ({}s)",
      PERIODIC_REFRESH_TIMER.count());
    refresh_timer_.expires_at(
      std::chrono::steady_clock::now() + PERIODIC_REFRESH_TIMER);
    refresh_timer_.async_wait([this](std::error_code const& failure) {
      // If the deadline timer has been cancelled, just stop right there.
      if (failure == asio::error::operation_aborted) {
        return;
      }

      if (failure) {
        throw std::system_error { detail::make_error_code(TIMER_MALFUNCTION) };
      }

      LOG_F(1, "[REFRESH] periodic bucket refresh timer expired");

      // Select the next bucket, ping its least recently seen node
      auto bucket = routing_table_.cbegin();
      if (next_bucket_to_refresh_ < routing_table_.BucketsCount()) {
        std::advance(bucket, next_bucket_to_refresh_);
        if (!bucket->Empty()) {
          auto const& least_recent = bucket->LeastRecentlySeenNode();
          asio::post(io_context_, [this, least_recent]() {
            // Initiate a PING to the least recently seen node in the bucket
            detail::StartPingNodeTask(
              least_recent, network_, routing_table_, []() { });
          });
        }
        ++next_bucket_to_refresh_;
      } else {
        next_bucket_to_refresh_ = 0;
      }

      // Refresh buckets that have not been updated for a certain time
      RefreshBuckets();

      // Recheck every minute
      ScheduleBucketRefreshTimer();
    });
  }

  template <typename THandler>
  void AsyncFindValue(KeyType const& key, THandler& handler)
  {
    asio::post(io_context_, [this, key, handler]() {
      LOG_F(1, "executing async load of key '{}'", key.ToHex());
      detail::StartFindValueTask<DataType>(
        key, network_, routing_table_, handler);
    });
  }

  template <typename THandler>
  void AsyncStoreValue(
    KeyType const& key, DataType const& data, THandler& handler)
  {
    asio::post(io_context_, [this, key, data, handler]() {
      // Put in the value store
      LOG_F(1, "saving key '{}' in my own store", key.ToHex());
      value_store_[key] = data;

      // Publish the key/value
      LOG_F(1, "publishing key '{}' and its value", key.ToHex());
      detail::StartStoreValueTask(key, data, network_, routing_table_, handler);
    });
  }

private:
  void ProcessNewMessage(EndpointType const& sender, Header const& header,
    BufferReader const& buffer)
  {
    switch (header.type_) {
    case Header::MessageType::PING_REQUEST:
      HandlePingRequest(sender, header);
      break;
    case Header::MessageType::STORE_REQUEST:
      HandleStoreRequest(sender, header, buffer);
      break;
    case Header::MessageType::FIND_NODE_REQUEST:
      HandleFindPeerRequest(sender, header, buffer);
      break;
    case Header::MessageType::FIND_VALUE_REQUEST:
      HandleFindValueRequest(sender, header, buffer);
      break;
    case Header::MessageType::PING_RESPONSE:
    case Header::MessageType::FIND_NODE_RESPONSE:
    case Header::MessageType::FIND_VALUE_RESPONSE:
      network_.HandleNewResponse(sender, header, buffer);
      break;
    default:
      NOVA_UNREACHABLE();
    }
  }

  void HandlePingRequest(EndpointType const& sender, Header const& header)
  {
    LOG_F(1, "handling ping request");

    network_.SendResponse(
      header.random_token_, Header::MessageType::PING_RESPONSE, sender);
  }

  void HandleStoreRequest(EndpointType const& sender, Header const& /*header*/,
    BufferReader const& buffer)
  {
    LOG_F(1, "handling store request from {}", sender.ToString());

    StoreValueRequestBody request;
    try {
      Deserialize(buffer, request);
    } catch (std::exception const& ex) {
      LOG_F(1, "failed to deserialize store request ({})", ex.what());
      return;
    }
    // Save the key and its value
    LOG_F(1, "saving key '{}' in my own store", request.data_key_.ToHex());
    value_store_[request.data_key_] = std::move(request.data_value_);
    // Do not republish - Republishing will be done after at least one hour
    // as per the kademlia optimized key republishing

    // Section 2.5
    // First, when a node receives a STORE RPC on a given key-value pair, it
    // assumes the RPC was also issued to the other k-1 closes nodes, and thus
    // the recipient will not republish the key-value pair in the next hour.
  }

  void HandleFindPeerRequest(EndpointType const& sender, Header const& header,
    BufferReader const& buffer)
  {
    LOG_F(1, "handling find peer request from {}", sender.ToString());

    // Ensure the request is valid.
    FindNodeRequestBody request;
    try {
      Deserialize(buffer, request);
    } catch (std::exception const& ex) {
      LOG_F(1, "failed to deserialize find peer request ({})", ex.what());
      return;
    }

    SendFindPeerResponse(sender, header.random_token_, request.node_id_);
  }

  void SendFindPeerResponse(EndpointType const& sender,
    blocxxi::crypto::Hash160 const& random_token,
    Node::IdType const& peer_to_find_id)
  {
    // Find X closest peers and save
    // their location into the response..
    FindNodeResponseBody response;

    auto neighbors = routing_table_.FindNeighbors(peer_to_find_id);

    for (const auto& node : neighbors) {
      response.peers_.push_back(node);
    }

    // Now send the response.
    LOG_F(1, "sending find peer response");
    network_.SendResponse(random_token, response, sender);
  }

  /**
   *
   */
  void HandleFindValueRequest(EndpointType const& sender, Header const& header,
    BufferReader const& buffer)
  {
    LOG_F(1, "handling find value request");

    FindValueRequestBody request;
    try {
      Deserialize(buffer, request);
    } catch (std::exception const& ex) {
      LOG_F(1, "failed to deserialize find value request ({})", ex.what());
      return;
    }

    const auto found = value_store_.find(request.value_key_);
    if (found == value_store_.end()) {
      SendFindPeerResponse(sender, header.random_token_, request.value_key_);
    } else {
      FindValueResponseBody const response { found->second };
      network_.SendResponse(header.random_token_, response, sender);
    }
  }

  /**
   *
   */
  void DiscoverNeighbors()
  {
    // Initial peer should know our neighbors, hence ask
    // him which peers are close to our own id.
    detail::StartBootstrapProcedure(network_, routing_table_);
  }

  auto GetClosestNeighborId() -> Node::IdType
  {
    // Find our closest neighbor.
    auto closest_neighbor = routing_table_.GetClosestNeighbor();
    return closest_neighbor.Id();
  }

  /**
   *
   */
  void HandleNewMessage(IpEndpoint const& sender, BufferReader const& buffer)
  {
    LOG_F(1, "received new message from '{}'", sender.ToString());

    Header header;
    std::size_t consumed;
    try {
      consumed = Deserialize(buffer, header);
    } catch (std::exception const& ex) {
      LOG_F(1, "failed to deserialize header ({})", ex.what());
      return;
    }

    // A message has been received from a node, add it
    // If the node is already in the routing table, the existing object will be
    // replaced by this one and thus, its failure count will be reset to 0 and
    // its last activity time will be set to now.
    auto added = routing_table_.AddPeer(
      Node(header.source_id_, sender.address_.to_string(), sender.port_));
    if (!added) {
      auto bucket_index = routing_table_.GetBucketIndexFor(header.source_id_);
      auto bucket = routing_table_.cbegin();
      std::advance(bucket, bucket_index);
      auto const& least_recent = bucket->LeastRecentlySeenNode();
      if (least_recent.IsQuestionable()) {
        asio::post(io_context_, [this, least_recent]() {
          // Initiate a PING to the least recently seen node in the bucket
          detail::StartPingNodeTask(
            least_recent, network_, routing_table_, []() { });
        });
      }
    }

    // TODO(Abdessattar): Queue a task to store relevant key/value pairs to the
    // new contact

    // TODO(Abdessattar): Optimize things here by passing Node reference instead
    // of sender
    ProcessNewMessage(sender, header, buffer.subspan(consumed));
  }

  void RefreshBuckets()
  {
    // Refresh buckets that have not been updated for more than
    // PERIODIC_BUCKET_REFRESH
    LOG_F(1,
      "[REFRESH] refreshing all buckets not updated within the last {} "
      "seconds",
      PERIODIC_REFRESH_TIMER.count());

    for (auto const& bucket : routing_table_) {
      auto since_last_update = bucket.TimeSinceLastUpdated();
      LOG_F(1, "[REFRESH] time since this bucket last updated: {}s",
        since_last_update.count());
      if (since_last_update > BUCKET_INACTIVE_TIME_BEFORE_REFRESH) {
        // Select a random id in the bucket
        if (!bucket.Empty()) {
          auto const& node = bucket.SelectRandomNode();
          auto const& node_id = node.Id();

          LOG_F(1,
            "periodic bucket refresh -> lookup for random peer with id {}",
            node_id.ToHex());

          asio::post(io_context_, [this, node_id]() {
            detail::StartFindNodeTask(
              node_id, network_, routing_table_,
              [this]() {
                LOG_F(1, "periodic bucket refresh completed");
                routing_table_.DumpToLog();
              },
              "REFRESH/FIND_NODE");
          });
        }
      }
      LOG_F(1, "[REFRESH] all buckets refresh completed");
    }
  }

  /// io_context used for the name resolution. Must be run by the caller.
  asio::io_context& io_context_;

  RoutingTableType routing_table_;
  NetworkType network_;
  ValueStoreType value_store_;

  asio::steady_timer refresh_timer_;
  std::size_t next_bucket_to_refresh_ { 0 };
};

} // namespace blocxxi::p2p::kademlia
