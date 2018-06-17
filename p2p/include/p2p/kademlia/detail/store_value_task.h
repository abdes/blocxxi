//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_P2P_KADEMLIA_STORE_VALUE_TASK_H_
#define BLOCXXI_P2P_KADEMLIA_STORE_VALUE_TASK_H_

#include <memory>
#include <system_error>

#include <common/logging.h>
#include <p2p/kademlia/key.h>
#include <p2p/kademlia/message.h>

#include "lookup_task.h"

namespace blocxxi {
namespace p2p {
namespace kademlia {
namespace detail {

///
template <typename TValueHandler, typename TNetwork, typename TRoutingTable,
          typename TData>
class StoreValueTask final : public BaseLookupTask {
 public:
  ///
  using HandlerType = TValueHandler;

  ///
  using NetworkType = TNetwork;
  ///
  using RoutingTableType = TRoutingTable;
  ///
  using EndpointType = typename NetworkType::EndpointType;

  ///
  using DataType = TData;

 public:
  ///
  constexpr static char const *TASK_NAME = "STORE_VALUE";

  /**
   *
   */
  static void Start(KeyType const &key, DataType const &data,
                    NetworkType &network, RoutingTableType &routing_table,
                    HandlerType handler, std::string const &task_name) {
    std::shared_ptr<StoreValueTask> task;
    task.reset(new StoreValueTask(key, data, network, routing_table,
                                  std::move(handler), task_name));

    TryToStoreValue(task);
  }

 private:
  StoreValueTask(KeyType const &key, DataType const &data, NetworkType &network,
                 RoutingTableType &routing_table, HandlerType &&save_handler,
                 std::string const &task_name)
      : BaseLookupTask(key, routing_table.FindNeighbors(key, PARALLELISM_ALPHA),
                       task_name),
        network_(network),
        routing_table_(routing_table),
        data_(data),
        save_handler_(std::forward<HandlerType>(save_handler)) {
    BXLOG(debug, "{} create store value task for '{}'", this->Name(),
          key.ToHex());
  }

  void NotifyCaller(std::error_code const &failure) { save_handler_(failure); }

  DataType const &GetData() const { return data_; }

  /**
   *
   */
  static void TryToStoreValue(
      std::shared_ptr<StoreValueTask> task,
      std::size_t concurrent_requests_count = PARALLELISM_ALPHA) {
    BXLOG(debug, "{} trying to find closer peer to store '{}' value",
          task->Name(), task->Key());

    FindNodeRequestBody const request{task->Key()};

    auto const closest_candidates =
        task->SelectUnContactedCandidates(concurrent_requests_count);

    for (auto const &peer : closest_candidates) {
      SendFindPeerRequest(request, peer, task);
    }

    // If no more requests are in flight
    // we know the closest peers hence ask
    // them to store the value.
    if (task->AllRequestsCompleted()) {
      BXLOG(debug, "{} task completed -> notify caller", task->Name());
      SendStoreRequests(task);
    }
  }

  /**
   *
   */
  static void SendFindPeerRequest(FindNodeRequestBody const &request,
                                  Node const &current_candidate,
                                  std::shared_ptr<StoreValueTask> task) {
    BXLOG(debug, "{} sending find peer request to store '{}' to '{}'",
          task->Name(), task->Key(), current_candidate);

    // On message received, process it.
    auto on_message_received = [task](EndpointType const &sender,
                                      Header const &header,
                                      BufferReader const &buffer) {
      HandleFindPeerResponse(sender, header, buffer, task);
    };

    // On error, retry with another endpoint.
    auto on_error = [task, current_candidate](std::error_code const &) {
      BXLOG(debug, "{} peer {} timed out on find peer request for store value",
            task->Name(), current_candidate);
      // Invalidate the candidate
      task->MarkCandidateAsInvalid(current_candidate.Id());
      // Also increment the number of failed requests in the routing table node
      task->routing_table_.PeerTimedOut(current_candidate);

      TryToStoreValue(task);
    };

    task->network_.SendConvRequest(request, current_candidate.Endpoint(),
                                   REQUEST_TIMEOUT, on_message_received,
                                   on_error);
  }

  /**
   *
   */
  static void HandleFindPeerResponse(EndpointType const &sender,
                                     Header const &header,
                                     BufferReader const &buffer,
                                     std::shared_ptr<StoreValueTask> task) {
    BXLOG(debug, "{} handle response from '{}@{}'", task->Name(),
          header.source_id_, sender);

    if (header.type_ != Header::MessageType::FIND_NODE_RESPONSE) {
      BXLOG(debug, "{} unexpected find peer response (type={})", task->Name(),
            int(header.type_));

      task->MarkCandidateAsInvalid(header.source_id_);
      TryToStoreValue(task);
      return;
    };

    FindNodeResponseBody response;
    try {
      Deserialize(buffer, response);
      task->MarkCandidateAsValid(header.source_id_);

      // If new candidate have been discovered, ask them.
      // but filter out ourselves from the list
      response.peers_.erase(
          std::remove_if(response.peers_.begin(), response.peers_.end(),
                         [task](Node &peer) {
                           return peer == task->routing_table_.ThisNode();
                         }),
          response.peers_.end());
      task->AddCandidates(response.peers_);
    } catch (std::exception const &ex) {
      BXLOG(debug, "{} failed to deserialize find peer response ({})",
            task->Name(), ex.what());
      task->MarkCandidateAsInvalid(header.source_id_);
    }

    TryToStoreValue(task);
  }

  /**
   *
   */
  static void SendStoreRequests(std::shared_ptr<StoreValueTask> task) {
    auto const &candidates = task->GetValidCandidates(REDUNDANT_SAVE_COUNT);
    if (candidates.empty()) {
      task->NotifyCaller(make_error_code(INITIAL_PEER_FAILED_TO_RESPOND));
    } else {
      for (auto peer : candidates) {
        SendStoreRequest(peer, task);
      }
      task->NotifyCaller(std::error_code{});
    }
  }

  /**
   *
   */
  static void SendStoreRequest(Node const &current_candidate,
                               std::shared_ptr<StoreValueTask> task) {
    BXLOG(debug, "{} send store request of '{}' to '{}'", task->Name(),
          task->Key(), current_candidate);

    StoreValueRequestBody const request{task->Key(), task->GetData()};
    task->network_.SendUniRequest(request, current_candidate.Endpoint());
  }

 private:
  ///
  NetworkType &network_;
  ///
  RoutingTableType &routing_table_;
  ///
  DataType data_;
  ///
  HandlerType save_handler_;
};

/**
 *
 */
template <typename TData, typename TNetwork, typename TRoutingTable,
          typename THandler>
void StartStoreValueTask(
    KeyType const &key, TData const &data, TNetwork &network,
    TRoutingTable &routing_table, THandler &&save_handler,
    std::string const &task_name =
        StoreValueTask<THandler, TNetwork, TRoutingTable, TData>::TASK_NAME) {
  using handler_type = typename std::decay<THandler>::type;
  using task = StoreValueTask<handler_type, TNetwork, TRoutingTable, TData>;

  task::Start(key, data, network, routing_table,
              std::forward<THandler>(save_handler), task_name);
}

}  // namespace detail
}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

#endif  // BLOCXXI_P2P_KADEMLIA_STORE_VALUE_TASK_H_
