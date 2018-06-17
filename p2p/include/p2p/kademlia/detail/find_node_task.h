//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_P2P_KADEMLIA_FIND_NODE_TASK_H_
#define BLOCXXI_P2P_KADEMLIA_FIND_NODE_TASK_H_

#include <memory>
#include <system_error>

#include <p2p/kademlia/message.h>
#include <p2p/kademlia/network.h>
#include <p2p/kademlia/node.h>
#include <p2p/kademlia/parameters.h>

#include "lookup_task.h"

namespace blocxxi {
namespace p2p {
namespace kademlia {
namespace detail {

///
template <typename TNetwork, typename TRoutingTable,
          typename TOnCompleteCallback>
class FindNodeTask final : public BaseLookupTask {
 public:
  ///
  using NetworkType = TNetwork;
  ///
  using RoutingTableType = TRoutingTable;
  ///
  using EndpointType = typename NetworkType::EndpointType;
  ///
  using OnCompleteCallbackType = TOnCompleteCallback;

 public:
  /**
   *
   */
  static void Start(Node::IdType const &key, NetworkType &network,
                    RoutingTableType &routing_table,
                    OnCompleteCallbackType on_complete,
                    std::string const &task_name) {
    BXLOG(debug, "[{}] starting a new task", task_name);

    std::shared_ptr<FindNodeTask> task;
    task.reset(
        new FindNodeTask(key, network, routing_table, on_complete, task_name));

    QueryUncontactedNeighbors(task);
  }

  constexpr static char const *TASK_NAME = "FIND_NODE";

 private:
  /**
   *
   */
  FindNodeTask(Node::IdType const &key, NetworkType &network,
               RoutingTableType &routing_table,
               OnCompleteCallbackType on_complete, std::string const &task_name)
      : BaseLookupTask(key, routing_table.FindNeighbors(key, PARALLELISM_ALPHA),
                       task_name),
        network_(network),
        routing_table_(routing_table),
        on_complete_(on_complete) {
    BXLOG(debug, "{} find node task on key={}", this->Name(), key);
  }

  /**
   *
   */
  static void QueryUncontactedNeighbors(std::shared_ptr<FindNodeTask> task) {
    FindNodeRequestBody const request{task->Key()};

    auto const peers = task->SelectUnContactedCandidates(PARALLELISM_ALPHA);

    for (auto const &peer : peers) {
      SendFindPeerRequest(request, peer, task);
    }

    if (task->AllRequestsCompleted()) {
      BXLOG(debug, "{} find node procedure completed.", task->Name());
      task->on_complete_();
    }
  }

  /**
   *
   */
  static void SendFindPeerRequest(FindNodeRequestBody const &request,
                                  Node const &current_peer,
                                  std::shared_ptr<FindNodeTask> task) {
    auto on_message_received = [task, current_peer](
        EndpointType const &s, Header const &h, BufferReader const &buffer) {
      task->MarkCandidateAsValid(current_peer.Id());
      HandleFindPeerResponse(s, h, buffer, task);
    };

    auto on_error = [task, current_peer](std::error_code const &) {
      // Invalidate the peer
      task->MarkCandidateAsInvalid(current_peer.Id());
      // Also increment the number of failed requests in the routing table node
      task->routing_table_.PeerTimedOut(current_peer);

      QueryUncontactedNeighbors(task);
    };

    task->network_.SendConvRequest(request, current_peer.Endpoint(),
                                   REQUEST_TIMEOUT, on_message_received,
                                   on_error);
  }

  /**
   *
   */
  static void HandleFindPeerResponse(EndpointType const &sender,
                                     Header const & /*header*/,
                                     BufferReader const &buffer,
                                     std::shared_ptr<FindNodeTask> task) {
    BXLOG(debug, "{} handle find peer response from '{}'", task->Name(),
          sender);
    FindNodeResponseBody response;

    try {
      Deserialize(buffer, response);
    } catch (std::exception const &ex) {
      BXLOG(debug, "{} failed to deserialize find peer response ({})",
            task->Name(), ex.what());
      return;
    }

    // If new candidate have been discovered, ask them.
    // but filter out ourselves from the list
    response.peers_.erase(
        std::remove_if(response.peers_.begin(), response.peers_.end(),
                       [task](Node &peer) {
                         return peer == task->routing_table_.ThisNode();
                       }),
        response.peers_.end());
    task->AddCandidates(response.peers_);

    QueryUncontactedNeighbors(task);
  }

 private:
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
void StartFindNodeTask(
    Node::IdType const &key, TNetwork &network, TRoutingTable &routing_table,
    TOnCompleteCallback on_complete,
    std::string const &task_name =
        FindNodeTask<TNetwork, TRoutingTable, TOnCompleteCallback>::TASK_NAME) {
  using TaskType = FindNodeTask<TNetwork, TRoutingTable, TOnCompleteCallback>;

  TaskType::Start(key, network, routing_table, on_complete, task_name);
}

}  // namespace detail
}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

#endif  // BLOCXXI_P2P_KADEMLIA_FIND_NODE_TASK_H_