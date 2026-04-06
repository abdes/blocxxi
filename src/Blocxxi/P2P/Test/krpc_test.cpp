//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <Nova/Base/Compilers.h>

#include <Blocxxi/P2P/kademlia/krpc.h>

// Disable compiler and linter warnings originating from the unit test framework
// and for which we cannot do anything. Additionally, every TEST or TEST_X macro
// usage must be preceded by a '// NOLINTNEXTLINE'.
NOVA_DIAGNOSTIC_PUSH
#if NOVA_CLANG_VERSION
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace blocxxi::p2p::kademlia {

// NOLINTNEXTLINE
TEST(KrpcTest, EncodesPingQueryToBep5Shape)
{
  auto message = KrpcMessage {};
  message.transaction_id_ = "aa";
  message.payload_ = KrpcQuery {
    "ping",
    {
      { "id",
        blocxxi::codec::bencode::Value("abcdefghij0123456789") },
    },
  };

  ASSERT_EQ(Encode(message),
    "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe");
}

// NOLINTNEXTLINE
TEST(KrpcTest, DecodesPingQueryFromBep5Example)
{
  auto const message = DecodeKrpc(
    "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe");

  ASSERT_EQ(message.transaction_id_, "aa");
  ASSERT_EQ(GetType(message), KrpcMessage::Type::Query);
  auto const& query = std::get<KrpcQuery>(message.payload_);
  ASSERT_EQ(query.method_, "ping");
  ASSERT_EQ(query.arguments_.at("id").AsString(), "abcdefghij0123456789");
}

// NOLINTNEXTLINE
TEST(KrpcTest, RoundTripsResponseWithVersionField)
{
  auto message = KrpcMessage {};
  message.transaction_id_ = "aa";
  message.version_ = "LT01";
  message.payload_ = KrpcResponse {
    {
      { "id",
        blocxxi::codec::bencode::Value("mnopqrstuvwxyz123456") },
      { "token", blocxxi::codec::bencode::Value("aoeusnth") },
    },
  };

  auto const encoded = Encode(message);
  auto const decoded = DecodeKrpc(encoded);

  ASSERT_EQ(decoded, message);
}

// NOLINTNEXTLINE
TEST(KrpcTest, CompactPeerRoundTripPreservesEndpoint)
{
  auto const endpoint = IpEndpoint("127.0.0.1", 6881);
  auto const encoded = EncodeCompactPeer(endpoint);
  auto const decoded = DecodeCompactPeer(encoded);

  ASSERT_EQ(encoded.size(), 6U);
  ASSERT_EQ(decoded, endpoint);
}

// NOLINTNEXTLINE
TEST(KrpcTest, CompactNodeRoundTripPreservesNode)
{
  auto const node = Node(
    Node::IdType::FromHex("0123456789abcdeffedcba987654321001234567"),
    "127.0.0.1", 6881);

  auto const encoded = EncodeCompactNode(node);
  auto const decoded = DecodeCompactNode(encoded);

  ASSERT_EQ(encoded.size(), 26U);
  ASSERT_EQ(decoded.Id(), node.Id());
  ASSERT_EQ(decoded.Endpoint(), node.Endpoint());
}

} // namespace blocxxi::p2p::kademlia
NOVA_DIAGNOSTIC_POP
