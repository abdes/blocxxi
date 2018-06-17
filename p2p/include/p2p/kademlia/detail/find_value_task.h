//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_P2P_KADEMLIA_FIND_VALUE_TASK_H_
#define BLOCXXI_P2P_KADEMLIA_FIND_VALUE_TASK_H_

#include <memory>
#include <system_error>

#include <common/logging.h>
#include <p2p/kademlia/key.h>
#include <p2p/kademlia/message.h>

#include "error_impl.h"
#include "lookup_task.h"

namespace blocxxi {
namespace p2p {
namespace kademlia {
namespace detail {

/**
 *  @brief This class represents a find value task.
 *  @details
 *  Its purpose is to perform network request
 *  to find the peer response of storing a value.
 *
 *  @dot
 *  digraph algorithm {
 *      fontsize=12
 *
 *      node [shape=diamond];
 *      "is any closer peer ?";
 *      "has peer responded ?";
 *      "does peer has data ?";
 *      "is any pending request ?";
 *
 *      node [shape=box];
 *      "query peer(s)";
 *      "add peer(s) from response"
 *
 *      node [shape=ellipse];
 *      "data not found";
 *      "data found";
 *      "start";
 *
 *      "start" -> "is any closer peer ?" [style=dotted]
 *      "is any closer peer ?":e -> "is any pending request ?" [label=no]
 *      "is any closer peer ?":s -> "query peer(s)" [label=yes]
 *      "query peer(s)" -> "has peer responded ?"
 *      "has peer responded ?":e -> "is any closer peer ?" [label=no]
 *      "has peer responded ?":s -> "does peer has data ?" [label=yes]
 *      "does peer has data ?":s -> "data found" [label=yes]
 *      "does peer has data ?":e -> "add peer(s) from response" [label=no]
 *      "add peer(s) from response":s -> "is any closer peer ?"
 *      "is any pending request ?":e -> "data not found" [label=no]
 *  }
 *  @enddot
 */
template <typename TValueHandler, typename TNetwork, typename TRoutingTable,
          typename TData>
class FindValueTask final : public BaseLookupTask {
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

  constexpr static char const *TASK_NAME = "FIND_VALUE";

 public:
  /**
   *
   */
  static void Start(KeyType const &key, NetworkType &network,
                    RoutingTableType &routing_table, HandlerType handler,
                    std::string const &task_name) {
    std::shared_ptr<FindValueTask> task;
    task.reset(new FindValueTask(key, network, routing_table,
                                 std::move(handler), task_name));

    TryCandidates(task);
  }

 private:
  /**
   *
   */
  FindValueTask(KeyType const &searched_key, NetworkType &network,
                TRoutingTable &routing_table, HandlerType load_handler,
                std::string const &task_name)
      : BaseLookupTask(searched_key, routing_table.FindNeighbors(
                                         searched_key, PARALLELISM_ALPHA),
                       task_name),
        network_(network),
        routing_table_(routing_table),
        handler_(std::move(load_handler)),
        is_finished_() {
    BXLOG(debug, "{} create new task for '{}'", this->Name(), searched_key);
  }

  /**
   *
   */
  void NotifyCaller(DataType const &data) {
    BLOCXXI_ASSERT(!IsCallerNotified());
    handler_(std::error_code(), data);
    is_finished_ = true;
  }

  /**
   *
   */
  void NotifyCaller(std::error_code const &failure) {
    BLOCXXI_ASSERT(!IsCallerNotified());
    handler_(failure, DataType{});
    is_finished_ = true;
  }

  /**
   *
   */
  bool IsCallerNotified() const { return is_finished_; }

  /**
   *
   */
  static void TryCandidates(std::shared_ptr<FindValueTask> task) {
    auto const closest_candidates =
        task->SelectUnContactedCandidates(PARALLELISM_ALPHA);

    FindValueRequestBody const request{task->Key()};
    for (auto const &peer : closest_candidates) {
      SendFindValueRequest(request, peer, task);
    }

    if (task->AllRequestsCompleted()) {
      task->NotifyCaller(make_error_code(VALUE_NOT_FOUND));
    }
  }

  /**
   *
   */
  static void SendFindValueRequest(FindValueRequestBody const &request,
                                   Node const &current_candidate,
                                   std::shared_ptr<FindValueTask> task) {
    BXLOG(debug, "{} sending find '{}' value request to '{}'", task->Name(),
          task->Key(), current_candidate);

    // On message received, process it.
    auto on_message_received = [task, current_candidate](
        EndpointType const &sender, Header const &header,
        BufferReader const &buffer) {
      if (task->IsCallerNotified()) return;

      task->MarkCandidateAsValid(current_candidate.Id());
      HandleFindValueResponse(sender, header, buffer, task);
    };

    // On error, retry with another endpoint.
    auto on_error = [task, current_candidate](std::error_code const &) {
      if (task->IsCallerNotified()) return;

      // Invalidate the candidate
      task->MarkCandidateAsInvalid(current_candidate.Id());
      // Also increment the number of failed requests in the routing table node
      task->routing_table_.PeerTimedOut(current_candidate);

      TryCandidates(task);
    };

    task->network_.SendConvRequest(request, current_candidate.Endpoint(),
                                   REQUEST_TIMEOUT, on_message_received,
                                   on_error);
  }

  /**
   *  @brief This method is called while searching for
   *         the peer owner of the value.
   */
  static void HandleFindValueResponse(EndpointType const &sender,
                                      Header const &header,
                                      BufferReader const &buffer,
                                      std::shared_ptr<FindValueTask> task) {
    BXLOG(debug, "{} handling response from '{}' to find '{}'", task->Name(),
          sender, task->Key());

    if (header.type_ == Header::MessageType::FIND_NODE_RESPONSE) {
      // The current peer didn't know the value
      // but provided closest peers.
      SendFindValueRequestsOnCloserPeers(buffer, task);
    } else if (header.type_ == Header::MessageType::FIND_VALUE_RESPONSE) {
      // The current peer knows the value.
      ProcessFoundValue(buffer, task);
    }
  }

  /**
   *  @brief This method is called when closest peers
   *         to the value we are looking are discovered.
   *         It recursively query new discovered peers
   *         or report an error to the use handler if
   *         all peers have been tried.
   */
  static void SendFindValueRequestsOnCloserPeers(
      BufferReader const &buffer, std::shared_ptr<FindValueTask> task) {
    BXLOG(debug, "{} checking if found closer peers to '{}' value",
          task->Name(), task->Key());

    FindNodeResponseBody response;
    try {
      Deserialize(buffer, response);
    } catch (std::exception const &ex) {
      BXLOG(debug, "{} failed to deserialize find node response ({})",
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
    TryCandidates(task);
  }

  /**
   *  @brief This method is called once the searched value
   *         has been found. It forwards the value to
   *         the user handler.
   */
  static void ProcessFoundValue(BufferReader const &buffer,
                                std::shared_ptr<FindValueTask> task) {
    BXLOG(debug, "{} found value for key '{}'", task->Name(), task->Key());

    FindValueResponseBody response;
    try {
      Deserialize(buffer, response);
    } catch (std::exception const &ex) {
      BXLOG(debug, "{} failed to deserialize find value response ({})",
            task->Name(), ex.what());
      return;
    }

    task->NotifyCaller(response.data_);
  }

 private:
  ///
  NetworkType &network_;
  ///
  RoutingTableType &routing_table_;
  ///
  HandlerType handler_;
  ///
  bool is_finished_;
};

/**
 *
 */
template <typename TData, typename TNetwork, typename TRoutingTable,
          typename THandler>
void StartFindValueTask(
    KeyType const &key, TNetwork &network, TRoutingTable &routing_table,
    THandler &&handler,
    std::string const &task_name =
        FindValueTask<THandler, TNetwork, TRoutingTable, TData>::TASK_NAME) {
  using handler_type = typename std::decay<THandler>::type;
  using task = FindValueTask<handler_type, TNetwork, TRoutingTable, TData>;

  task::Start(key, network, routing_table, std::forward<THandler>(handler),
              task_name);
}

}  // namespace detail
}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

#endif  // BLOCXXI_P2P_KADEMLIA_FIND_VALUE_TASK_H_
