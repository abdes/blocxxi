//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <logging/logging.h>

#include <p2p/kademlia/message.h>

#include "find_node_task.h"

#include <memory>

namespace blocxxi::p2p::kademlia::detail {

/*!
 *
 * To join the network, a node u must have a contact to an already participating
 * node w – usually a bootstrap node is available on every network. u inserts w
 * into the appropriate k-bucket. u then performs a node lookup for its own node
 * ID. Finally, u refreshes all k-buckets further away than its closest
 * neighbor. During the refreshes, u both populates its own k-buckets and
 * inserts itself into other nodes’ k-buckets as necessary.
 *
 * When a new node joins the system, it must store any KV pair to which it is
 * one of the k-closest. Existing nodes, by similarly exploiting complete
 * knowledge of their surrounding subtrees, will know which KV pairs the new
 * node should store. Any node learning of a new node therefore issues STORE
 * RPCs to transfer relevant KV pairs to the new node. To avoid redundant store
 * RPCs for the same content from different nodes, a node only transfers a KV
 * pair if its own ID is closer to the key than are the IDs of other nodes.
 */
template <typename TNetwork, typename TRoutingTable>
class BootstrapProcedure final
    : asap::logging::Loggable<BootstrapProcedure<TNetwork, TRoutingTable>> {
public:
  /// The logger id used for logging within this class.
  static constexpr const char *LOGGER_NAME = "p2p-kademlia";

  // We need to import the internal logger retrieval method symbol in this
  // context to avoid g++ complaining about the method not being declared before
  // being used. THis is due to the fact that the current class is a template
  // class and that method does not take any template argument that will enable
  // the compiler to resolve it unambiguously.
  using asap::logging::Loggable<BootstrapProcedure<TNetwork,
      TRoutingTable>>::internal_log_do_not_use_read_comment;

  using NetworkType = TNetwork;
  using RoutingTableType = TRoutingTable;

  static void Start(NetworkType &network, RoutingTableType &routing_table) {
    std::shared_ptr<BootstrapProcedure> task;
    task.reset(new BootstrapProcedure(network, routing_table));

    NodeLookupSelf(task);
  }

private:
  BootstrapProcedure(NetworkType &network, RoutingTableType &routing_table)
      : network_(network), routing_table_(routing_table) {
    ASLOG(debug, "create bootstrap procedure instance");
  }

  static void NodeLookupSelf(std::shared_ptr<BootstrapProcedure> task) {
    auto my_id = task->routing_table_.ThisNode().Id();
    auto on_complete = [task]() {
      ASLOG(debug, "find node on self completed");
      task->routing_table_.DumpToLog();
      RefreshBuckets(task);
    };

    StartFindNodeTask(my_id, task->network_, task->routing_table_, on_complete,
        "BOOT/FIND_NODE");
  }

  static void RefreshBuckets(std::shared_ptr<BootstrapProcedure> task) {
    // Section 2.3: Finally, u refreshes all k-buckets further away than its
    // closest neighbor. During the refresh u both populates its own k-buckets
    // and inserts itself into other nodes' k-buckets as necessary.

    // Given that we don't use 160 buckets but do the more relaxed routing
    // table, we'll simply refresh all buckets.
    for (auto const &bucket : task->routing_table_) {
      // Select a random id in the bucket
      if (!bucket.Empty()) {
        auto const &node = bucket.SelectRandomNode();

        ASLOG(debug,
            "[BOOT/REFRESH] bucket -> lookup for random peer with id {}",
            node.Id().ToHex());

        StartFindNodeTask(
            node.Id(), task->network_, task->routing_table_,
            [task]() {
              ASLOG(debug, "[BOOT/REFRESH] bucket refresh completed");
              task->routing_table_.DumpToLog();
            },
            "BOOT/REFRESH/FIND_NODE");
      }
    }
    ASLOG(debug, "[BOOT/REFRESH] all buckets refresh completed");
  }

  ///
  NetworkType &network_;
  ///
  RoutingTableType &routing_table_;
};

template <typename TNetwork, typename TRoutingTable>
inline void StartBootstrapProcedure(
    TNetwork &network, TRoutingTable &routing_table) {
  using TaskType = BootstrapProcedure<TNetwork, TRoutingTable>;

  TaskType::Start(network, routing_table);
}

} // namespace blocxxi::p2p::kademlia::detail
