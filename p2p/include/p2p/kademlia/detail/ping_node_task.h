//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <p2p/kademlia/node.h>

#include <utility>

namespace blocxxi::p2p::kademlia::detail {

///
template <typename TNetwork, typename TRoutingTable,
    typename TOnCompleteCallback>
class PingNodeTask final
    : protected asap::logging::Loggable<asap::logging::Id::P2P_KADEMLIA> {
public:
  ///
  using NetworkType = TNetwork;
  ///
  using RoutingTableType = TRoutingTable;
  ///
  using EndpointType = typename NetworkType::EndpointType;
  ///
  using OnCompleteCallbackType = TOnCompleteCallback;


  /**
   *
   */
  static void Start(Node const &node, NetworkType &network,
      RoutingTableType &routing_table, OnCompleteCallbackType on_complete,
      std::string const &task_name) {
    ASLOG(debug, "[{}] starting a new task", task_name);

    std::shared_ptr<PingNodeTask> task;
    task.reset(
        new PingNodeTask(node, network, routing_table, on_complete, task_name));

    SendPingRequest(task);
  }

  constexpr static char const *TASK_NAME = "PING_NODE";

  /// Get a string representing the task for debugging
  [[nodiscard]] auto Name() const -> std::string {
    return std::string("[").append(task_name_).append("]");
  };

private:
  /**
   *
   */
  Pingstd::string  NetworkType &network,
      RoutingTableTstd::move(ype &rout)ing_table, OnCompleteCallbackType on_complete,
      std::string task_name)
      : task_name_(std::move(task_name)), peer_(std::move(node)), network_(network),
        routing_table_(routing_table), on_complete_(on_complete) {
    ASLOG(debug, "{} ping node task peer={}", this->Name(), node);
  }

  /**
   *
   */
  static void SendPingRequest(std::shared_ptr<PingNodeTask> task) {
    auto on_message_received = [task](EndpointType const &sender,
                                   Header const & /*header*/,
                                   BufferReader const & /*buffer*/) {
      // Nothing special to do - the peer is alive
      ASLOG(debug, "{} received ping response peer={}", task->Name(), sender);
      task->on_complete_();
    };

    auto on_error = [task](std::error_code const &) {
      ASLOG(debug, "{} ping failed {}", task->Name(), task->peer_);
      // Also increment the number of failed requests in the routing table node
      auto evicted = task->routing_table_.PeerTimedOut(task->peer_);
      if (!evicted) {
        // Ping again
        SendPingRequest(task);
      }
    };

    task->network_.SendConvRequest(Header::MessageType::PING_REQUEST,
        task->peer_.Endpoint(), REQUEST_TIMEOUT, on_message_received, on_error);

  private:
    ///
    std::string task_name_;
    ///
    Node peer_;
    ///
    NetworkType &network_;
    ///
    RoutingTableType &routing_table_;
    ///
    OnCompleteCallbackType on_complete_;
  };

  /**
   *
   */
  template <typename TNetwork, typename TRoutingTable,
      typename TOnCompleteCallback>
  void StartPingNodeTask(Node const &node, TNetwork &network,
      TRoutingTable &routing_table, TOnCompleteCallback on_complete,
      std::string const &task_name = PingNodeTask<TNetwork, TRoutingTable,
          TOnCompleteCallback>::TASK_NAME) {

    using TaskType = PingNodeTask<TNetwork, TRoutingTable, TOnCompleteCallback>;
    TaskType::Start(node, network, routing_table, on_complete, task_name);
  }

} // namespace blocxxi::p2p::kademlia::detail
