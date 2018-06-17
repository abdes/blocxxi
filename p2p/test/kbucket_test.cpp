//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <gtest/gtest.h>

#include <vector>

#include <p2p/kademlia/routing.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

TEST(KBucketTest, AddNode) {
  auto kb = KBucket(Node(Node::IdType::RandomHash(), "::1", 0), 0, 4);
  ASSERT_EQ(kb.Size().first, 0);

  auto first = Node(Node::IdType::RandomHash(), "::1", 0);
  auto id = first.Id();

  // Adding nodes while k-bucket not full succeeds and increments the number of
  // nodes in the bucket
  ASSERT_TRUE(kb.AddNode(std::move(first)));
  ASSERT_EQ(1, kb.Size().first);
  ASSERT_TRUE(kb.AddNode(Node(Node::IdType::RandomHash(), "::1", 1)));
  ASSERT_EQ(2, kb.Size().first);
  ASSERT_TRUE(kb.AddNode(Node(Node::IdType::RandomHash(), "::1", 2)));
  ASSERT_EQ(3, kb.Size().first);
  ASSERT_TRUE(kb.AddNode(Node(Node::IdType::RandomHash(), "::1", 3)));
  ASSERT_EQ(4, kb.Size().first);

  // Adding a node when bucket has ksize nodes returns false and the node is
  // added to the replacement cache instead
  ASSERT_FALSE(kb.AddNode(Node(Node::IdType::RandomHash(), "::1", 4)));
  ASSERT_EQ(4, kb.Size().first);
  // TODO: assert replacement size increased

  // Adding a node with an id that is already in k-bucket will replace the
  // existing node
  ASSERT_TRUE(kb.AddNode(Node(id, "::", 5)));
  ASSERT_EQ(4, kb.Size().first);
  ASSERT_TRUE(kb.AddNode(Node(id, "::", 6)));
  ASSERT_EQ(4, kb.Size().first);

  ASSERT_FALSE(kb.AddNode(Node(Node::IdType::RandomHash(), "::1", 7)));
  ASSERT_EQ(kb.Size().first, 4);
}

TEST(KBucketTest, RemoveNode) {
  auto kb = KBucket(Node(Node::IdType::RandomHash(), "::1", 0), 0, 3);
  ASSERT_EQ(kb.Size().first, 0);

  // Remove from empty bucket has no effect
  kb.RemoveNode(Node(Node::IdType::RandomHash(), "::1", 0));

  // Add then remove - no replacement cache
  Node::IdType ids[4];
  auto node = Node(Node::IdType::RandomHash(), "::1", 0);
  ids[0] = node.Id();
  kb.AddNode(std::move(node));
  ASSERT_EQ(1, kb.Size().first);
  kb.RemoveNode(Node(ids[0], "::", 0));
  ASSERT_EQ(0, kb.Size().first);

  // Create a replacement cache with 1 item
  for (auto i = 0; i < 4; ++i) {
    node = Node(Node::IdType::RandomHash(), "::1", i);
    ids[i] = node.Id();
    kb.AddNode(std::move(node));
  }
  ASSERT_EQ(3, kb.Size().first);

  // Remove a node will get it replaced by the cached node
  kb.RemoveNode(Node(ids[0], "::", 0));
  ASSERT_EQ(3, kb.Size().first);
  kb.RemoveNode(Node(ids[3], "::", 3));
  ASSERT_EQ(2, kb.Size().first);

  // Remove the rest now until k-bucket is empty
  kb.RemoveNode(Node(ids[1], "::", 1));
  ASSERT_EQ(1, kb.Size().first);
  kb.RemoveNode(Node(ids[2], "::", 2));
  ASSERT_EQ(0, kb.Size().first);
}

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi
