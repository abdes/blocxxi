//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <gtest/gtest.h>

#include <forward_list>

#include <boost/multiprecision/cpp_int.hpp>

#include <p2p/kademlia/node.h>

using namespace boost::multiprecision;

namespace blocxxi {
namespace p2p {
namespace kademlia {

namespace {

using uint256_t =
    number<cpp_int_backend<256, 256, unsigned_magnitude, unchecked, void>>;

}  // anonymous namespace

TEST(NodeTest, Construction) {
  auto n1 = Node(Node::IdType::RandomHash(), "1.1.1.1", 3030);
  auto n2 = Node(n1);
  ASSERT_EQ(n1.Id(), n2.Id());
  ASSERT_EQ(n1.Endpoint(), n2.Endpoint());
  ASSERT_EQ(n1.ToString(), n2.ToString());

  auto n3 = std::move(n1);
  ASSERT_EQ(n2.Id(), n3.Id());
  ASSERT_EQ(n2.Endpoint(), n3.Endpoint());
  ASSERT_EQ(n2.ToString(), n3.ToString());

  auto n4(std::move(n3));
  ASSERT_EQ(n2.Id(), n4.Id());
  ASSERT_EQ(n2.Endpoint(), n4.Endpoint());
  ASSERT_EQ(n2.ToString(), n4.ToString());

  auto n5 = n4;
  ASSERT_EQ(n4.Id(), n5.Id());
  ASSERT_EQ(n4.Endpoint(), n5.Endpoint());
  ASSERT_EQ(n4.ToString(), n5.ToString());
}

TEST(NodeTest, LogDistance) {
  // LogDistance same id is -1
  auto ah = Node::IdType::RandomHash();
  auto an = Node(ah, "::1", 1);
  auto dist = LogDistance(an, an);
  ASSERT_EQ(-1, dist);

  // LogDistance first bits are different = 159
  auto bh = ah;
  bh[0] = ah[0] ^ 0x80;
  auto bn = Node(bh, "::1", 2);
  dist = LogDistance(an, bn);
  ASSERT_EQ(159, dist);

  for (int i = 0; i < 10; ++i) {
    auto a = Node(Node::IdType::RandomHash(), "::1", 0);
    auto b = Node(Node::IdType::RandomHash(), "::1", 0);

    auto logdist = LogDistance(a, b);

    auto distanceRaw = Distance(a, b);
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

TEST(NodeTest, FromUrlString) {
  auto id = Node::IdType::RandomHash().ToHex();
  auto address = "192.168.1.35";
  auto port = "4242";
  auto str = std::string("knode://")
                 .append(id)
                 .append("@")
                 .append(address)
                 .append(":")
                 .append(port);

  Node node;
  ASSERT_NO_THROW(node = Node::FromUrlString(str));
  ASSERT_EQ(id, node.Id().ToHex());
  ASSERT_EQ(address, node.Endpoint().Address().to_string());
  ASSERT_EQ(port, std::to_string(node.Endpoint().Port()));
}

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi
