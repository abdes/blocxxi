//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <chrono>
#include <string>

#include <Nova/Base/Compilers.h>
#include <gtest/gtest.h>

#include <Blocxxi/P2P/kademlia/mainline_node.h>

// Disable compiler and linter warnings originating from the unit test framework
// and for which we cannot do anything. Additionally, every TEST or TEST_X macro
// usage must be preceded by a '// NOLINTNEXTLINE'.
NOVA_DIAGNOSTIC_PUSH
#if NOVA_CLANG_VERSION
#  pragma clang diagnostic ignored "-Wused-but-marked-unused"
#  pragma clang diagnostic ignored "-Wglobal-constructors"
#  pragma clang diagnostic ignored "-Wexit-time-destructors"
#  pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace blocxxi::p2p::kademlia {

namespace {

auto MakeTestNode(std::string_view address, std::uint16_t port) -> Node
{
  return Node(Node::IdType::RandomHash(), std::string(address), port);
}

auto MakePingQuery() -> Buffer
{
  auto query = KrpcMessage {};
  query.transaction_id_ = "aa";
  query.payload_ = KrpcQuery {
    "ping",
    {
      { "id",
        blocxxi::codec::bencode::Value("abcdefghij0123456789") },
    },
  };

  auto encoded = Encode(query);
  return Buffer(encoded.begin(), encoded.end());
}

} // namespace

// NOLINTNEXTLINE
TEST(MainlineDhtNodeTest, RespondsToPingQueries)
{
  asio::io_context io_context;
  auto receiver
    = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30111");
  auto sender = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30112");

  auto node = MainlineDhtNode(
    io_context, MakeTestNode("127.0.0.1", 30111), std::move(receiver));

  auto query_seen = false;
  node.OnQuery([&query_seen](std::string_view method, IpEndpoint const&) {
    ASSERT_EQ(method, "ping");
    query_seen = true;
  });
  node.Start();

  auto response_seen = false;
  sender->AsyncReceive([&response_seen](std::error_code const& failure,
                        IpEndpoint const&, BufferReader const& buffer) {
    ASSERT_FALSE(failure);
    auto message = DecodeKrpc(std::string_view(
      reinterpret_cast<char const*>(buffer.data()), buffer.size()));
    ASSERT_EQ(GetType(message), KrpcMessage::Type::Response);
    ASSERT_EQ(message.transaction_id_, "aa");
    response_seen = true;
  });

  auto ping = MakePingQuery();
  sender->AsyncSend(ping, IpEndpoint { "127.0.0.1", 30111 },
    [](std::error_code const& failure) { ASSERT_FALSE(failure); });

  io_context.run_for(std::chrono::seconds(1));

  ASSERT_TRUE(query_seen);
  ASSERT_TRUE(response_seen);
}

// NOLINTNEXTLINE
TEST(MainlineDhtNodeTest, ReportsBootstrapSuccessOnFindNodeReply)
{
  asio::io_context io_context;
  auto bootstrap_channel
    = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30113");
  auto main_channel = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30114");

  auto bootstrap = MainlineDhtNode(
    io_context, MakeTestNode("127.0.0.1", 30113), std::move(bootstrap_channel));
  bootstrap.Start();

  auto main = MainlineDhtNode(
    io_context, MakeTestNode("127.0.0.1", 30114), std::move(main_channel));

  auto bootstrap_ok = false;
  main.OnBootstrapSuccess([&bootstrap_ok](std::string const& bootstrap_endpoint) {
    ASSERT_EQ(bootstrap_endpoint, "127.0.0.1:30113");
    bootstrap_ok = true;
  });
  main.AddBootstrapNode("127.0.0.1:30113");
  main.Start();

  io_context.run_for(std::chrono::seconds(1));

  ASSERT_TRUE(bootstrap_ok);
}

} // namespace blocxxi::p2p::kademlia
NOVA_DIAGNOSTIC_POP
