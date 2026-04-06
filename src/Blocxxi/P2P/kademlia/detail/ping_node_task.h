//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Nova/Base/Logging.h>

#include <Blocxxi/P2P/kademlia/message.h>
#include <Blocxxi/P2P/kademlia/node.h>

#include <utility>

namespace blocxxi::p2p::kademlia::detail {

///
template <typename TNetwork, typename TRoutingTable,
    typename TOnCompleteCallback>
class PingNodeTask final {
public:
  /// The logger id used for logging within this class.
  static constexpr const char *LOGGER_NAME = "p2p-kademlia";

  // We need to import the internal logger retrieval method symbol in this
  // context to avoid g++ complaining about the method not being declared before
  // being used. THis is due to the fact that the current class is a template
  // class and that method does not take any template argument that will enable
  // the compiler to resolve it unambiguously.

  using NetworkType = TNetwork;
  using RoutingTableType = TRoutingTable;
  using EndpointType = typename NetworkType::EndpointType;
  using OnCompleteCallbackType = TOnCompleteCallback;

  /**
   *
   */
  static void Start(Node const &node, NetworkType &network,
      RoutingTableType &routing_table, OnCompleteCallbackType on_complete,
      std::string const &task_name) {
    LOG_F(1, "[{}] starting a new task", task_name);

    std::shared_ptr<PingNodeTask> task;
    task.reset(
        new PingNodeTask(node, network, routing_table, on_complete, task_name));

    SendPingRequest(task);
  }

  constexpr static char const *TASK_NAME = "PING_NODE";

  /// Get a string representing the task for debugging
  auto Name() const -> std::string {
    return std::string("[").append(task_name_).append("]");
  }

private:
  PingNodeTask(Node const &node, NetworkType &network,
      RoutingTableType &routing_table, OnCompleteCallbackType on_complete,
      std::string task_name)
      : task_name_(std::move(task_name)), peer_(node), network_(network),
        routing_table_(routing_table), on_complete_(on_complete) {
    LOG_F(1, "{} ping node task peer={}", this->Name(), node.ToString());
  }

  /**
   *
   */
  static void SendPingRequest(std::shared_ptr<PingNodeTask> task) {
    auto on_message_received = [task](EndpointType const &sender,
                                   Header const & /*header*/,
                                   BufferReader const & /*buffer*/) {
      // Nothing special to do - the peer is alive
      LOG_F(1, "{} received ping response peer={}", task->Name(),
          sender.ToString());
      task->on_complete_();
    };

    auto on_error = [task](std::error_code const &) {
      LOG_F(1, "{} ping failed {}", task->Name(), task->peer_.ToString());
      // Also increment the number of failed requests in the routing table node
      auto evicted = task->routing_table_.PeerTimedOut(task->peer_);
      if (!evicted) {
        // Ping again
        SendPingRequest(task);
      }
    };

    task->network_.SendConvRequest(Header::MessageType::PING_REQUEST,
        task->peer_.Endpoint(), REQUEST_TIMEOUT, on_message_received, on_error);
  }

  std::string task_name_;
  Node peer_;
  NetworkType &network_;
  RoutingTableType &routing_table_;
  OnCompleteCallbackType on_complete_;
};

/**
 *
 */
template <typename TNetwork, typename TRoutingTable,
    typename TOnCompleteCallback>
void StartPingNodeTask(Node const &node, TNetwork &network,
    TRoutingTable &routing_table, TOnCompleteCallback on_complete,
    std::string const &task_name =
        PingNodeTask<TNetwork, TRoutingTable, TOnCompleteCallback>::TASK_NAME) {

  using TaskType = PingNodeTask<TNetwork, TRoutingTable, TOnCompleteCallback>;
  TaskType::Start(node, network, routing_table, on_complete, task_name);
}

} // namespace blocxxi::p2p::kademlia::detail
