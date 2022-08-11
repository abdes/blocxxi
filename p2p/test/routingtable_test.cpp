//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <p2p/kademlia/routing.h>

namespace blocxxi::p2p::kademlia {

// NOLINTNEXTLINE
TEST(RoutingTableTest, AddContact) {
  auto rt_node = Node(Node::IdType::RandomHash(), "::1", 3030);

  auto routing_table = RoutingTable(std::move(rt_node), 3);

  routing_table.AddPeer(Node(Node::IdType::RandomHash(), "::1", 1));
}

// NOLINTNEXTLINE
TEST(RoutingTableTest, FindNeighborsReturnsEmptyIfRoutingTableIsEmpty) {
  auto rt_node = Node(Node::IdType::RandomHash(), "::1", 3030);
  auto routing_table = RoutingTable(std::move(rt_node), 20);
  /*
  for (auto i = 0U; i < 19; ++i) {
    rt.AddContact(Node(Node::IdType::RandomHash(), "::1", i));
  }
  */
  auto neighbors = routing_table.FindNeighbors(Node::IdType::RandomHash(), 20);

  ASSERT_EQ(0, neighbors.size());
}

// NOLINTNEXTLINE
TEST(RoutingTableTest, FindNeighborsReturnsAllNodesIfNotEnoughAvailable) {
  auto rt_node = Node(Node::IdType::RandomHash(), "::1", 3030);
  auto routing_table = RoutingTable(std::move(rt_node), 20);
  for (uint16_t i = 0U; i < 4; ++i) {
    routing_table.AddPeer(Node(Node::IdType::RandomHash(), "::1", i));
  }
  auto neighbors = routing_table.FindNeighbors(Node::IdType::RandomHash(), 7);

  ASSERT_EQ(4, neighbors.size());
}

// NOLINTNEXTLINE
TEST(RoutingTableTest, FindNeighborsReturnsRequestedNodesIfAvailable) {
  auto rt_node = Node(Node::IdType::RandomHash(), "::1", 3030);
  auto routing_table = RoutingTable(std::move(rt_node), 5);
  for (uint16_t i = 0U; i < 30; ++i) {
    routing_table.AddPeer(Node(Node::IdType::RandomHash(), "::1", i));
  }
  routing_table.DumpToLog();
  auto neighbors10 =
      routing_table.FindNeighbors(Node::IdType::RandomHash(), 10);
  ASSERT_EQ(10, neighbors10.size());
  auto neighbors7 = routing_table.FindNeighbors(Node::IdType::RandomHash(), 7);
  ASSERT_EQ(7, neighbors7.size());
}

} // namespace blocxxi::p2p::kademlia
