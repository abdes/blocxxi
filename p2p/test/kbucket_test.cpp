//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <cstdint>
#include <gtest/gtest.h>

#include <vector>

#include <p2p/kademlia/routing.h>

namespace blocxxi::p2p::kademlia {

// NOLINTNEXTLINE
TEST(KBucketTest, AddNode) {
  auto bucket = KBucket(Node(Node::IdType::RandomHash(), "::1", 0), 0, 4);
  ASSERT_EQ(0U, bucket.Size().first);

  auto first = Node(Node::IdType::RandomHash(), "::1", 0);
  auto node_id = first.Id();

  // Adding nodes while k-bucket not full succeeds and increments the number of
  // nodes in the bucket
  ASSERT_TRUE(bucket.AddNode(std::move(first)));
  ASSERT_EQ(1, bucket.Size().first);
  ASSERT_TRUE(bucket.AddNode(Node(Node::IdType::RandomHash(), "::1", 1)));
  ASSERT_EQ(2, bucket.Size().first);
  ASSERT_TRUE(bucket.AddNode(Node(Node::IdType::RandomHash(), "::1", 2)));
  ASSERT_EQ(3, bucket.Size().first);
  ASSERT_TRUE(bucket.AddNode(Node(Node::IdType::RandomHash(), "::1", 3)));
  ASSERT_EQ(4, bucket.Size().first);

  // Adding a node when bucket has ksize nodes returns false and the node is
  // added to the replacement cache instead
  ASSERT_FALSE(bucket.AddNode(Node(Node::IdType::RandomHash(), "::1", 4)));
  ASSERT_EQ(4, bucket.Size().first);
  // TODO(Abdessattar): assert replacement size increased

  // Adding a node with an id that is already in k-bucket will replace the
  // existing node
  ASSERT_TRUE(bucket.AddNode(Node(node_id, "::", 5)));
  ASSERT_EQ(4, bucket.Size().first);
  ASSERT_TRUE(bucket.AddNode(Node(node_id, "::", 6)));
  ASSERT_EQ(4, bucket.Size().first);

  ASSERT_FALSE(bucket.AddNode(Node(Node::IdType::RandomHash(), "::1", 7)));
  ASSERT_EQ(bucket.Size().first, 4);
}

// NOLINTNEXTLINE
TEST(KBucketTest, RemoveNode) {
  auto bucket = KBucket(Node(Node::IdType::RandomHash(), "::1", 0), 0, 3);
  ASSERT_EQ(bucket.Size().first, 0);

  // Remove from empty bucket has no effect
  bucket.RemoveNode(Node(Node::IdType::RandomHash(), "::1", 0));

  // Add then remove - no replacement cache
  Node::IdType ids[4];
  auto node = Node(Node::IdType::RandomHash(), "::1", 0);
  ids[0] = node.Id();
  bucket.AddNode(std::move(node));
  ASSERT_EQ(1, bucket.Size().first);
  bucket.RemoveNode(Node(ids[0], "::", 0));
  ASSERT_EQ(0, bucket.Size().first);

  // Create a replacement cache with 1 item
  for (uint16_t i = 0; i < 4; ++i) {
    node = Node(Node::IdType::RandomHash(), "::1", i);
    ids[i] = node.Id();
    bucket.AddNode(std::move(node));
  }
  ASSERT_EQ(3, bucket.Size().first);

  // Remove a node will get it replaced by the cached node
  bucket.RemoveNode(Node(ids[0], "::", 0));
  ASSERT_EQ(3, bucket.Size().first);
  bucket.RemoveNode(Node(ids[3], "::", 3));
  ASSERT_EQ(2, bucket.Size().first);

  // Remove the rest now until k-bucket is empty
  bucket.RemoveNode(Node(ids[1], "::", 1));
  ASSERT_EQ(1, bucket.Size().first);
  bucket.RemoveNode(Node(ids[2], "::", 2));
  ASSERT_EQ(0, bucket.Size().first);
}

} // namespace blocxxi::p2p::kademlia
