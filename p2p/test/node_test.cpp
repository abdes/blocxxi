//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <common/compilers.h>

#include <gtest/gtest.h>

#include <forward_list>

#include <p2p/kademlia/node.h>

ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_GNUC_VERSION)
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include <boost/multiprecision/cpp_int.hpp>
ASAP_DIAGNOSTIC_POP

namespace blocxxi::p2p::kademlia {

namespace {

using uint256_t =
    boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256,
        256, boost::multiprecision::unsigned_magnitude,
        boost::multiprecision::unchecked, void>>;

} // anonymous namespace

// NOLINTNEXTLINE
TEST(NodeTest, Construction) {
  auto node_1 = Node(Node::IdType::RandomHash(), "1.1.1.1", 3030);
  auto node_2 = Node(node_1);
  ASSERT_EQ(node_1.Id(), node_2.Id());
  ASSERT_EQ(node_1.Endpoint(), node_2.Endpoint());
  ASSERT_EQ(node_1.ToString(), node_2.ToString());

  auto node_3 = std::move(node_1);
  ASSERT_EQ(node_2.Id(), node_3.Id());
  ASSERT_EQ(node_2.Endpoint(), node_3.Endpoint());
  ASSERT_EQ(node_2.ToString(), node_3.ToString());

  auto node_4(std::move(node_3));
  ASSERT_EQ(node_2.Id(), node_4.Id());
  ASSERT_EQ(node_2.Endpoint(), node_4.Endpoint());
  ASSERT_EQ(node_2.ToString(), node_4.ToString());

  auto node_5 = node_4;
  ASSERT_EQ(node_4.Id(), node_5.Id());
  ASSERT_EQ(node_4.Endpoint(), node_5.Endpoint());
  ASSERT_EQ(node_4.ToString(), node_5.ToString());
}

// NOLINTNEXTLINE
TEST(NodeTest, LogDistance) {
  // LogDistance same id is -1
  auto a_node_hash = Node::IdType::RandomHash();
  auto a_node = Node(a_node_hash, "::1", 1);
  auto dist = LogDistance(a_node, a_node);
  ASSERT_EQ(-1, dist);

  // LogDistance first bits are different = 159
  auto b_node_hash = a_node_hash;
  b_node_hash[0] = a_node_hash[0] ^ 0x80;
  auto b_node = Node(b_node_hash, "::1", 2);
  dist = LogDistance(a_node, b_node);
  ASSERT_EQ(159, dist);

  for (int i = 0; i < 10; ++i) {
    a_node = Node(Node::IdType::RandomHash(), "::1", 0);
    b_node = Node(Node::IdType::RandomHash(), "::1", 0);

    auto logdist = LogDistance(a_node, b_node);

    auto distanceRaw = Distance(a_node, b_node);
    uint256_t distance;
    import_bits(distance, distanceRaw.begin(), distanceRaw.end());

    auto kbucket = logdist;

    uint256_t two_k;
    bit_set(two_k, kbucket);
    ASSERT_TRUE(two_k < distance);
    bit_unset(two_k, kbucket);
    bit_set(two_k, kbucket + 1);
    ASSERT_TRUE(two_k > distance);
  }
}

// NOLINTNEXTLINE
TEST(NodeTest, FromUrlString) {
  auto node_id = Node::IdType::RandomHash().ToHex();
  const auto *address = "192.168.1.35";
  const auto *port = "4242";
  auto str = std::string("knode://")
                 .append(node_id)
                 .append("@")
                 .append(address)
                 .append(":")
                 .append(port);

  Node node;
  // NOLINTNEXTLINE
  ASSERT_NO_THROW(node = Node::FromUrlString(str));
  ASSERT_EQ(node_id, node.Id().ToHex());
  ASSERT_EQ(address, node.Endpoint().Address().to_string());
  ASSERT_EQ(
      port, std::to_string(static_cast<unsigned int>(node.Endpoint().Port())));
}

} // namespace blocxxi::p2p::kademlia
