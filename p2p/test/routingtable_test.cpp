//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <gtest/gtest.h>

#include <p2p/kademlia/routing.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

TEST(RoutingTableTest, AddContact) {
  auto rt_node = Node(Node::IdType::RandomHash(), "::1", 3030);

  auto rt = RoutingTable(std::move(rt_node), 3);

  rt.AddPeer(Node(Node::IdType::RandomHash(), "::1", 1));
}

TEST(RoutingTableTest, FindNeighborsReturnsEmptyIfRoutingTableIsEmpty) {
  auto rt_node = Node(Node::IdType::RandomHash(), "::1", 3030);
  auto rt = RoutingTable(std::move(rt_node), 20);
  /*
  for (auto i = 0U; i < 19; ++i) {
    rt.AddContact(Node(Node::IdType::RandomHash(), "::1", i));
  }
  */
  auto neighbors = rt.FindNeighbors(Node::IdType::RandomHash(), 20);

  ASSERT_EQ(0, neighbors.size());
}

TEST(RoutingTableTest, FindNeighborsReturnsAllNodesIfNotEnoughAvailable) {
  auto rt_node = Node(Node::IdType::RandomHash(), "::1", 3030);
  auto rt = RoutingTable(std::move(rt_node), 20);
  for (auto i = 0U; i < 4; ++i) {
    rt.AddPeer(Node(Node::IdType::RandomHash(), "::1", i));
  }
  auto neighbors = rt.FindNeighbors(Node::IdType::RandomHash(), 7);

  ASSERT_EQ(4, neighbors.size());
}

TEST(RoutingTableTest, FindNeighborsReturnsRequestedNodesIfAvailable) {
  auto rt_node = Node(Node::IdType::RandomHash(), "::1", 3030);
  auto rt = RoutingTable(std::move(rt_node), 5);
  for (auto i = 0U; i < 30; ++i) {
    rt.AddPeer(Node(Node::IdType::RandomHash(), "::1", i));
  }
  auto neighbors = rt.FindNeighbors(Node::IdType::RandomHash(), 7);
  ASSERT_EQ(7, neighbors.size());
  neighbors = rt.FindNeighbors(Node::IdType::RandomHash(), 10);
  ASSERT_EQ(10, neighbors.size());
}

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi
