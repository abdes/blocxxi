//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <vector>

#include <boost/asio/io_context.hpp>

#include <common/logging.h>
#include <p2p/kademlia/detail/bootstrap_procedure.h>
#include <p2p/kademlia/detail/find_node_task.h>
#include <p2p/kademlia/detail/find_value_task.h>
#include <p2p/kademlia/detail/ping_node_task.h>
#include <p2p/kademlia/detail/store_value_task.h>
#include <p2p/kademlia/key.h>
#include <p2p/kademlia/network.h>
#include <p2p/kademlia/node.h>
#include <p2p/kademlia/parameters.h>
#include <p2p/kademlia/routing.h>
#include <p2p/kademlia/value_store.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

// TODO: when new node is discovered, transfer to it key/values (where this node
// id is closer to the key than are the IDs of other nodes)

template <typename TRoutingTable, typename TNetwork>
class Engine final : asap::logging::Loggable<asap::logging::Id::P2P_KADEMLIA> {
 public:
  ///
  using DataType = std::vector<std::uint8_t>;
  ///
  using RoutingTableType = TRoutingTable;
  ///
  using NetworkType = TNetwork;
  ///
  using EndpointType = typename NetworkType::EndpointType;
  ///
  using ValueStoreType = ValueStore<KeyType, DataType>;

 public:
  Engine(boost::asio::io_context &io_context, RoutingTableType &&routing_table,
         NetworkType &&network)
      : io_context_(io_context),
        routing_table_(std::move(routing_table)),
        network_(std::move(network)),
        value_store_(),
        refresh_timer_(io_context) {
    ASLOG(debug, "Creating Engine DONE");
  }

  Engine(Engine const &) = delete;
  Engine &operator=(Engine const &) = delete;

  Engine(Engine &&) = default;
  Engine &operator=(Engine &&) = default;

  ~Engine() { ASLOG(debug, "Destroy Engine"); }

  RoutingTable const &GetRoutingTable() const { return routing_table_; }

  void AddBootstrapNode(const std::string &bnode_url) {
    AddBootstrapNode(Node::FromUrlString(bnode_url));
  }

  void AddBootstrapNode(Node &&bnode) {
    ASLOG(debug, "adding bootstrap node at {}", bnode.Endpoint());
    routing_table_.AddPeer(std::move(bnode));
  }

  void Start() {
    ASLOG(debug, "Engine Start: {}", routing_table_.ThisNode().ToString());
    network_.OnMessageReceived(std::bind(&Engine::HandleNewMessage, this,
                                         std::placeholders::_1,
                                         std::placeholders::_2));

    network_.Start();

    if (!routing_table_.Empty()) {
      // Queue the bootstrapping as a delayed task
      io_context_.post([this]() { DiscoverNeighbors(); });
    } else {
      ASLOG(info, "engine started as a bootstrap node - empty routing table");
    }

    ScheduleBucketRefreshTimer();
  }

  void ScheduleBucketRefreshTimer() {
    // Start the buckets refresh period timer
    ASLOG(debug, "[REFRESH] periodic bucket refresh timer started ({}s)",
          PERIODIC_REFRESH_TIMER.count());
    refresh_timer_.expires_at(std::chrono::steady_clock::now() +
                              PERIODIC_REFRESH_TIMER);
    refresh_timer_.async_wait([this](boost::system::error_code const &failure) {
      // If the deadline timer has been cancelled, just stop right there.
      if (failure == boost::asio::error::operation_aborted) return;

      if (failure)
        throw std::system_error{detail::make_error_code(TIMER_MALFUNCTION)};

      ASLOG(debug, "[REFRESH] periodic bucket refresh timer expired");

      // Select the next bucket, ping its least recently seen node
      static auto bucket_index = 0U;
      auto bucket = routing_table_.cbegin();
      if (bucket_index < routing_table_.BucketsCount()) {
        std::advance(bucket, bucket_index);
        if (!bucket->Empty()) {
          auto const &least_recent = bucket->LeastRecentlySeenNode();
          io_context_.post([this, least_recent]() {
            // Initiate a PING to the least recently seen node in the bucket
            detail::StartPingNodeTask(least_recent, network_, routing_table_,
                                      []() {});
          });
        }
        ++bucket_index;
      } else {
        bucket_index = 0;
      }

      // Refresh buckets that have not been updated for a certain time
      RefreshBuckets();

      // Recheck every minute
      ScheduleBucketRefreshTimer();
    });
  }

  /**
   *
   */
  template <typename THandler>
  void AsyncFindValue(KeyType const &key, THandler &handler) {
    io_context_.post([this, key, handler]() {
      ASLOG(debug, "executing async load of key '{}'", key);
      detail::StartFindValueTask<DataType>(key, network_, routing_table_,
                                           handler);
    });
  }

  template <typename THandler>
  void AsyncStoreValue(KeyType const &key, DataType const &data,
                       THandler &handler) {
    io_context_.post([this, key, data, handler]() {
      // Put in the value store
      ASLOG(debug, "saving key '{}' in my own store", key);
      value_store_[key] = data;

      // Publish the key/value
      ASLOG(debug, "publishing key '{}' and its value", key);
      detail::StartStoreValueTask(key, data, network_, routing_table_, handler);
    });
  }

 private:
  /**
   *
   */
  void ProcessNewMessage(EndpointType const &sender, Header const &header,
                         BufferReader const &buffer) {
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
      default:
        network_.HandleNewResponse(sender, header, buffer);
        break;
    }
  }

  /**
   *
   */
  void HandlePingRequest(EndpointType const &sender, Header const &header) {
    ASLOG(debug, "handling ping request");

    network_.SendResponse(header.random_token_,
                          Header::MessageType::PING_RESPONSE, sender);
  }

  /**
   *
   */
  void HandleStoreRequest(EndpointType const &sender, Header const & /*header*/,
                          BufferReader const &buffer) {
    ASLOG(debug, "handling store request from {}", sender);

    StoreValueRequestBody request;
    try {
      Deserialize(buffer, request);
    } catch (std::exception const &ex) {
      ASLOG(debug, "failed to deserialize store request ({})", ex.what());
      return;
    }
    // Save the key and its value
    ASLOG(debug, "saving key '{}' in my own store", request.data_key_);
    value_store_[request.data_key_] = std::move(request.data_value_);
    // Do not republish - Republishing will be done after at least one hour
    // as per the kademlia optimized key republishing

    // Section 2.5
    // First, when a node receives a STORE RPC on a given key-value pair, it
    // assumes the RPC was also issued to the other k-1 closes nodes, and thus
    // the recipient will not republish the key-value pair in the next hour.
  }

  /**
   *
   */
  void HandleFindPeerRequest(EndpointType const &sender, Header const &header,
                             BufferReader const &buffer) {
    ASLOG(debug, "handling find peer request from {}", sender);

    // Ensure the request is valid.
    FindNodeRequestBody request;
    try {
      Deserialize(buffer, request);
    } catch (std::exception const &ex) {
      ASLOG(debug, "failed to deserialize find peer request ({})", ex.what());
      return;
    }

    SendFindPeerResponse(sender, header.random_token_, request.node_id_);
  }

  /**
   *
   */
  void SendFindPeerResponse(EndpointType const &sender,
                            blocxxi::crypto::Hash160 const &random_token,
                            Node::IdType const &peer_to_find_id) {
    // Find X closest peers and save
    // their location into the response..
    FindNodeResponseBody response;

    auto neighbors = routing_table_.FindNeighbors(peer_to_find_id);

    for (auto node : neighbors) {
      response.peers_.push_back(node);
    };

    // Now send the response.
    ASLOG(debug, "sending find peer response");
    network_.SendResponse(random_token, response, sender);
  }

  /**
   *
   */
  void HandleFindValueRequest(EndpointType const &sender, Header const &header,
                              BufferReader const &buffer) {
    ASLOG(debug, "handling find value request");

    FindValueRequestBody request;
    try {
      Deserialize(buffer, request);
    } catch (std::exception const &ex) {
      ASLOG(debug, "failed to deserialize find value request ({})", ex.what());
      return;
    }

    auto found = value_store_.find(request.value_key_);
    if (found == value_store_.end())
      SendFindPeerResponse(sender, header.random_token_, request.value_key_);
    else {
      FindValueResponseBody const response{found->second};
      network_.SendResponse(header.random_token_, response, sender);
    }
  }

  /**
   *
   */
  void DiscoverNeighbors() {
    // Initial peer should know our neighbors, hence ask
    // him which peers are close to our own id.
    detail::StartBootstrapProcedure(network_, routing_table_);
  }

  Node::IdType GetClosestNeighborId(void) {
    // Find our closest neighbor.
    auto closest_neighbor = routing_table_.GetClosestNeighbor();
    return closest_neighbor.Id();
  }

  /**
   *
   */
  void HandleNewMessage(IpEndpoint const &sender, BufferReader const &buffer) {
    ASLOG(debug, "received new message from '{}'", sender);

    Header header;
    std::size_t consumed = 0;
    try {
      consumed = Deserialize(buffer, header);
    } catch (std::exception const &ex) {
      ASLOG(debug, "failed to deserialize header ({})", ex.what());
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
      auto const &least_recent = bucket->LeastRecentlySeenNode();
      if (least_recent.IsQuestionable()) {
        io_context_.post([this, least_recent]() {
          // Initiate a PING to the least recently seen node in the bucket
          detail::StartPingNodeTask(least_recent, network_, routing_table_,
                                    []() {});
        });
      }
    }

    // TODO: Queue a task to store relevant key/value pairs to the new contact

    // TODO: Optimize things here by passing Node reference instead of sender
    ProcessNewMessage(sender, header, buffer.subspan(consumed));
  }

  void RefreshBuckets() {
    // Refresh buckets that have not been updated for more than
    // PERIODIC_BUCKET_REFRESH
    ASLOG(debug,
          "[REFRESH] refreshing all buckets not updated within the last {} "
          "seconds",
          PERIODIC_REFRESH_TIMER.count());

    for (auto const &bucket : routing_table_) {
      auto since_last_update = bucket.TimeSinceLastUpdated();
      ASLOG(debug, "[REFRESH] time since this bucket last updated: {}s",
            since_last_update.count());
      if (since_last_update > BUCKET_INACTIVE_TIME_BEFORE_REFRESH) {
        // Select a random id in the bucket
        if (!bucket.Empty()) {
          auto const &node = bucket.SelectRandomNode();
          auto const &id = node.Id();

          ASLOG(debug,
                "periodic bucket refresh -> lookup for random peer with id {}",
                id.ToHex());

          io_context_.post([this, id]() {
            detail::StartFindNodeTask(
                id, network_, routing_table_,
                [this]() {
                  ASLOG(debug, "periodic bucket refresh completed");
                  routing_table_.DumpToLog();
                },
                "REFRESH/FIND_NODE");
          });
        }
      }
      ASLOG(debug, "[REFRESH] all buckets refresh completed");
    }
  }

  /// io_context used for the name resolution. Must be run by the caller.
  boost::asio::io_context &io_context_;

  ///
  RoutingTableType routing_table_;
  ///
  NetworkType network_;
  ///
  ValueStoreType value_store_;

  ///
  boost::asio::steady_timer refresh_timer_;
};

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi
